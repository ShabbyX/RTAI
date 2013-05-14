#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <task.h>
#include <timer.h>
#include <sem.h>

RT_TASK latency_task, display_task;

RT_SEM display_sem;

#define ONE_BILLION  1000000000
#define TEN_MILLION    10000000

long minjitter, maxjitter, avgjitter;
long gminjitter = TEN_MILLION,
    gmaxjitter = -TEN_MILLION,
    gavgjitter = 0,
    goverrun = 0;

long long period_ns = 0;
int test_duration = 0;  /* sec of testing, via -T <sec>, 0 is inf */
int data_lines = 21;    /* data lines per header line, -l <lines> to change */
int quiet = 0;          /* suppress printing of RTH, RTD lines when -T given */

time_t test_start, test_end;    /* report test duration */
int test_loops = 0;             /* outer loop count */

#define MEASURE_PERIOD ONE_BILLION
#define SAMPLE_COUNT (MEASURE_PERIOD / period_ns)

/* Warmup time : in order to avoid spurious cache effects on low-end machines. */
#define WARMUP_TIME 1
#define HISTOGRAM_CELLS 100
int histogram_size = HISTOGRAM_CELLS;
long *histogram_avg = NULL,
     *histogram_max = NULL,
     *histogram_min = NULL;

int do_histogram = 0, do_stats = 0, finished = 0;
int bucketsize = 1000;  /* default = 1000ns, -B <size> to override */

static inline void add_histogram (long *histogram, long addval)
{
    /* bucketsize steps */
    long inabs = rt_timer_tsc2ns(addval >= 0 ? addval : -addval) / bucketsize;
    histogram[inabs < histogram_size ? inabs : histogram_size-1]++;
}

void latency (void *cookie)
{
    int err, count, nsamples, warmup = 1;
    RTIME expected_tsc, period_tsc, start_ticks;
    RT_TIMER_INFO timer_info;

    err = rt_timer_start(TM_ONESHOT);

    if (err)
        {
        fprintf(stderr,"latency: cannot start timer, code %d\n",err);
        return;
        }

    err = rt_timer_inquire(&timer_info);

    if (err)
        {
        fprintf(stderr,"latency: rt_timer_inquire, code %d\n",err);
        return;
        }

    nsamples = ONE_BILLION / period_ns;
    period_tsc = rt_timer_ns2tsc(period_ns);
    /* start time: one millisecond from now. */
    start_ticks = timer_info.date + rt_timer_ns2ticks(1000000);
    expected_tsc = timer_info.tsc + rt_timer_ns2tsc(1000000);

    err = rt_task_set_periodic(NULL,start_ticks,rt_timer_ns2ticks(period_ns));

    if (err)
        {
        fprintf(stderr,"latency: failed to set periodic, code %d\n",err);
        return;
        }

    for (;;)
        {
        long minj = TEN_MILLION, maxj = -TEN_MILLION, dt, sumj;
        long overrun = 0;
        test_loops++;

        for (count = sumj = 0; count < nsamples; count++)
            {
            expected_tsc += period_tsc;
            err = rt_task_wait_period(NULL);

            if (err)
                {
                if (err != -ETIMEDOUT)
                    {
                    fprintf(stderr,"latency: wait period failed, code %d\n",err);
                    rt_task_delete(NULL); /* Timer stopped. */
                    }

                overrun++;
                }

            dt = (long)(rt_timer_tsc() - expected_tsc);
            if (dt > maxj) maxj = dt;
            if (dt < minj) minj = dt;
            sumj += dt;

            if (!(finished || warmup) && (do_histogram || do_stats))
                add_histogram(histogram_avg, dt);
            }

        if(!warmup)
            {
            if (!finished && (do_histogram || do_stats))
                {
                add_histogram(histogram_max, maxj);
                add_histogram(histogram_min, minj);
                }

            minjitter = minj;
            if(minj < gminjitter)
                gminjitter = minj;

            maxjitter = maxj;
            if(maxj > gmaxjitter)
                gmaxjitter = maxj;

            avgjitter = sumj / nsamples;
            gavgjitter += avgjitter;
            goverrun += overrun;
            rt_sem_v(&display_sem);
            }

        if(warmup && test_loops == WARMUP_TIME)
            {
            test_loops = 0;
            warmup = 0;
            }
        }
}

void display (void *cookie)
{
    int err, n = 0;
    time_t start;

    err = rt_sem_create(&display_sem,"dispsem",0,S_FIFO);

    if (err)
        {
        fprintf(stderr,"latency: cannot create semaphore: %s\n",strerror(-err));
        return;
        }

    time(&start);

    if (quiet)
        fprintf(stderr, "running quietly for %d seconds\n", test_duration);

    for (;;)
        {
        long minj, gminj, maxj, gmaxj, avgj;
        err = rt_sem_p(&display_sem,TM_INFINITE);

        if (err)
            {
            if (err != -EIDRM)
                fprintf(stderr,"latency: failed to pend on semaphore, code %d\n",err);

            rt_task_delete(NULL);
            }

        /* convert jitters to nanoseconds. */
        minj = rt_timer_tsc2ns(minjitter);
        gminj = rt_timer_tsc2ns(gminjitter);
        avgj = rt_timer_tsc2ns(avgjitter);
        maxj = rt_timer_tsc2ns(maxjitter);
        gmaxj = rt_timer_tsc2ns(gmaxjitter);

        if (!quiet)
            {
            if (data_lines && (n++ % data_lines)==0)
                {
                time_t now, dt;
                time(&now);
                dt = now - start - WARMUP_TIME;
                printf("RTT|  %.2ld:%.2ld:%.2ld\n",
                       dt / 3600,(dt / 60) % 60,dt % 60);
                printf("RTH|%12s|%12s|%12s|%8s|%12s|%12s\n",
                       "-----lat min","-----lat avg","-----lat max","-overrun",
                       "----lat best","---lat worst");
                }

            printf("RTD|%12ld|%12ld|%12ld|%8ld|%12ld|%12ld\n",
                   minj,
                   avgj,
                   maxj,
                   goverrun,
                   gminj,
                   gmaxj);
            }
        }
}

double dump_histogram (long *histogram, char* kind)
{
    int n, total_hits = 0;
    double avg = 0;             /* used to sum hits 1st */

    if (do_histogram)
        fprintf(stderr,"---|--param|----range-|--samples\n");

    for (n = 0; n < histogram_size; n++)
        {
        long hits = histogram[n];

        if (hits)
            {
            total_hits += hits;
            avg += n * hits;
            if (do_histogram)
                fprintf(stderr,
                        "HSD|    %s| %3d -%3d | %8ld\n",
                        kind,
                        n,
                        n+1,
                        hits);
            }
        }

    avg /= total_hits;  /* compute avg, reuse variable */

    return avg;
}

void dump_stats (long *histogram, char* kind, double avg)
{
    int n, total_hits = 0;
    double variance = 0;

    for (n = 0; n < histogram_size; n++)
        {
        long hits = histogram[n];

        if (hits)
            {
            total_hits += hits;
            variance += hits * (n-avg) * (n-avg);
            }
        }

    /* compute std-deviation (unbiased form) */
    variance /= total_hits - 1;
    variance = sqrt(variance);

    fprintf(stderr,"HSS|    %s| %9d| %10.3f| %10.3f\n",
            kind, total_hits, avg, variance);
}

void dump_hist_stats (void)
{
    double minavg, maxavg, avgavg;

    /* max is last, where its visible w/o scrolling */
    minavg = dump_histogram (histogram_min, "min");
    avgavg = dump_histogram (histogram_avg, "avg");
    maxavg = dump_histogram (histogram_max, "max");

    fprintf(stderr,"HSH|--param|--samples-|--average--|---stddev--\n");

    dump_stats (histogram_min, "min", minavg);
    dump_stats (histogram_avg, "avg", avgavg);
    dump_stats (histogram_max, "max", maxavg);
}

void cleanup_upon_sig(int sig __attribute__((unused)))
{
    time_t actual_duration;
    long gmaxj, gminj, gavgj;

    if (finished)
        return;

    finished = 1;
//    rt_timer_stop();
    rt_sem_delete(&display_sem);

    if (do_histogram || do_stats)
        dump_hist_stats();

    time(&test_end);
    actual_duration = test_end - test_start - WARMUP_TIME;
    if (!test_duration) test_duration = actual_duration;
    gavgjitter /= (test_loops > 1 ? test_loops : 2)-1;

    gminj = rt_timer_tsc2ns(gminjitter);
    gmaxj = rt_timer_tsc2ns(gmaxjitter);
    gavgj = rt_timer_tsc2ns(gavgjitter);

    printf("---|------------|------------|------------|--------|-------------------------\n"
           "RTS|%12ld|%12ld|%12ld|%8ld|    %.2ld:%.2ld:%.2ld/%.2d:%.2d:%.2d\n",
           gminj,
           gavgj,
           gmaxj,
           goverrun,
           actual_duration / 3600,
           (actual_duration / 60) % 60,
           actual_duration % 60,
           test_duration / 3600,
           (test_duration / 60) % 60,
           test_duration % 60);

    if (histogram_avg)  free(histogram_avg);
    if (histogram_max)  free(histogram_max);
    if (histogram_min)  free(histogram_min);

    exit(0);
}

int main (int argc, char **argv)
{
    int c, err;

    while ((c = getopt(argc,argv,"hp:l:T:qH:B:s")) != EOF)
        switch (c)
            {
            case 'h':

                do_histogram = 1;
                break;

            case 's':

                do_stats = 1;
                break;

            case 'H':

                histogram_size = atoi(optarg);
                break;

            case 'B':

                bucketsize = atoi(optarg);
                break;

            case 'p':

                period_ns = atoi(optarg) * 1000LL;
                break;

            case 'l':

                data_lines = atoi(optarg);
                break;

            case 'T':

                test_duration = atoi(optarg);
                alarm(test_duration + WARMUP_TIME);
                break;

            case 'q':

                quiet = 1;
                break;

            default:

                fprintf(stderr, "usage: latency [options]\n"
                        "  [-h]                         # print histograms of min, avg, max latencies\n"
                        "  [-s]                         # print statistics of min, avg, max latencies\n"
                        "  [-H <histogram-size>]        # default = 200, increase if your last bucket is full\n"
                        "  [-B <bucket-size>]           # default = 1000ns, decrease for more resolution\n"
                        "  [-p <period_us>]             # sampling period\n"
                        "  [-l <data-lines per header>] # default=21, 0 to supress headers\n"
                        "  [-T <test_duration_seconds>] # default=0, so ^C to end\n"
                        "  [-q]                         # supresses RTD, RTH lines if -T is used\n");
                exit(2);
            }

    if (!test_duration && quiet)
        {
        fprintf(stderr, "latency: -q only works if -T has been given.\n");
        quiet = 0;
        }

    time(&test_start);

    histogram_avg = calloc(histogram_size, sizeof(long));
    histogram_max = calloc(histogram_size, sizeof(long));
    histogram_min = calloc(histogram_size, sizeof(long));

    if (!(histogram_avg && histogram_max && histogram_min))
        cleanup_upon_sig(0);

    if (period_ns == 0)
        period_ns = 100000LL; /* ns */

    signal(SIGINT, cleanup_upon_sig);
    signal(SIGTERM, cleanup_upon_sig);
    signal(SIGHUP, cleanup_upon_sig);
    signal(SIGALRM, cleanup_upon_sig);

    setlinebuf(stdout);

    printf("== Sampling period: %Ld us\n",period_ns / 1000);

    mlockall(MCL_CURRENT|MCL_FUTURE);

    err = rt_task_create(&display_task,"display",0,98,0);

    if (err)
        {
        fprintf(stderr,"latency: failed to create display task, code %d\n",err);
        return 0;
        }

    err = rt_task_start(&display_task,&display,NULL);

    if (err)
        {
        fprintf(stderr,"latency: failed to start display task, code %d\n",err);
        return 0;
        }

    err = rt_task_create(&latency_task,"sampling",0,99,T_FPU);

    if (err)
        {
        fprintf(stderr,"latency: failed to create latency task, code %d\n",err);
        return 0;
        }

    err = rt_task_start(&latency_task,&latency,NULL);

    if (err)
        {
        fprintf(stderr,"latency: failed to start latency task, code %d\n",err);
        return 0;
        }

    pause();

    return 0;
}
