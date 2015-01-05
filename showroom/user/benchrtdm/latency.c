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

#ifndef __UCLIBC__
#include <execinfo.h>
#endif /* !__UCLIBC__ */

#include <task.h>
#include <timer.h>
#include <sem.h>
#include "rttesting.h"

#define CONFIG_XENO_DEFAULT_PERIOD 100000LL

#define SIGDEBUG                        SIGXCPU
#define SIGDEBUG_UNDEFINED              0
#define SIGDEBUG_MIGRATE_SIGNAL         1
#define SIGDEBUG_MIGRATE_SYSCALL        2
#define SIGDEBUG_MIGRATE_FAULT          3
#define SIGDEBUG_MIGRATE_PRIOINV        4
#define SIGDEBUG_NOMLOCK                5
#define SIGDEBUG_WATCHDOG               6
#define SIGDEBUG_RESCNT_IMBALANCE       7

#define T_WARNSW 0

#define xntrace_user_freeze(x, y)

RT_TASK latency_task, display_task;

RT_SEM display_sem;

#define ONE_BILLION  1000000000
#define TEN_MILLION    10000000

unsigned max_relaxed;
long minjitter, maxjitter, avgjitter;
long gminjitter = TEN_MILLION, gmaxjitter = -TEN_MILLION, goverrun = 0;
long long gavgjitter = 0;

long long period_ns = 0;
int test_duration = 0;		/* sec of testing, via -T <sec>, 0 is inf */
int data_lines = 21;		/* data lines per header line, -l <lines> to change */
int quiet = 0;			/* suppress printing of RTH, RTD lines when -T given */
int benchdev_no = 0;
int benchdev = -1;
int freeze_max = 0;
int priority = T_HIPRIO;
int stop_upon_switch = 0;
sig_atomic_t sampling_relaxed = 0;

#define USER_TASK       0
#define KERNEL_TASK     1
#define TIMER_HANDLER   2

int test_mode = USER_TASK;
const char *test_mode_names[] = {
	"periodic user-mode task",
	"in-kernel periodic task",
	"in-kernel timer handler"
};

time_t test_start, test_end;	/* report test duration */
int test_loops = 0;		/* outer loop count */

#define MEASURE_PERIOD ONE_BILLION
#define SAMPLE_COUNT (MEASURE_PERIOD / period_ns)

/* Warmup time : in order to avoid spurious cache effects on low-end machines. */
#define WARMUP_TIME 1
#define HISTOGRAM_CELLS 300
int histogram_size = HISTOGRAM_CELLS;
long *histogram_avg = NULL, *histogram_max = NULL, *histogram_min = NULL;

char *do_gnuplot = NULL;
int do_histogram = 0, do_stats = 0, finished = 0;
int bucketsize = 1000;		/* default = 1000ns, -B <size> to override */

#define need_histo() (do_histogram || do_stats || do_gnuplot)

static inline void add_histogram(long *histogram, long addval)
{
	/* bucketsize steps */
	long inabs =
	    rt_timer_tsc2ns(addval >= 0 ? addval : -addval) / bucketsize;
	histogram[inabs < histogram_size ? inabs : histogram_size - 1]++;
}

void latency(void *cookie)
{
	int err, count, nsamples, warmup = 1;
	RTIME expected_tsc, period_tsc, start_ticks, fault_threshold;
	RT_TIMER_INFO timer_info;
	unsigned old_relaxed = 0;

	err = rt_timer_inquire(&timer_info);

	if (err) {
		fprintf(stderr, "latency: rt_timer_inquire, code %d\n", err);
		return;
	}

	fault_threshold = rt_timer_ns2tsc(CONFIG_XENO_DEFAULT_PERIOD);
	nsamples = ONE_BILLION / period_ns / 1000;
	period_tsc = rt_timer_ns2tsc(period_ns);
	/* start time: one millisecond from now. */
	start_ticks = timer_info.date + rt_timer_ns2ticks(1000000);
	expected_tsc = timer_info.tsc + rt_timer_ns2tsc(1000000);

	err =
	    rt_task_set_periodic(NULL, start_ticks,
				 rt_timer_ns2ticks(period_ns));

	if (err) {
		fprintf(stderr, "latency: failed to set periodic, code %d\n",
			err);
		return;
	}

	for (;;) {
		long minj = TEN_MILLION, maxj = -TEN_MILLION, dt;
		long overrun = 0;
		long long sumj;
		test_loops++;

		for (count = sumj = 0; count < nsamples; count++) {
			unsigned new_relaxed;
			unsigned long ov;

			expected_tsc += period_tsc;
			err = rt_task_wait_period(&ov);

			dt = (long)(rt_timer_tsc() - expected_tsc);
			new_relaxed = sampling_relaxed;
			if (dt > maxj) {
				if (new_relaxed != old_relaxed
				    && dt > fault_threshold)
					max_relaxed +=
						new_relaxed - old_relaxed;
				maxj = dt;
			}
			old_relaxed = new_relaxed;
			if (dt < minj)
				minj = dt;
			sumj += dt;

			if (err) {
				if (err != -ETIMEDOUT) {
					fprintf(stderr,
						"latency: wait period failed, code %d\n",
						err);
					exit(EXIT_FAILURE); /* Timer stopped. */
				}

				overrun += ov;
				expected_tsc += period_tsc * ov;
			}

			if (freeze_max && (dt > gmaxjitter)
			    && !(finished || warmup)) {
				xntrace_user_freeze(rt_timer_tsc2ns(dt), 0);
				gmaxjitter = dt;
			}

			if (!(finished || warmup) && need_histo())
				add_histogram(histogram_avg, dt);
		}

		if (!warmup) {
			if (!finished && need_histo()) {
				add_histogram(histogram_max, maxj);
				add_histogram(histogram_min, minj);
			}

			minjitter = minj;
			if (minj < gminjitter)
				gminjitter = minj;

			maxjitter = maxj;
			if (maxj > gmaxjitter)
				gmaxjitter = maxj;

			avgjitter = sumj / nsamples;
			gavgjitter += avgjitter;
			goverrun += overrun;
			rt_sem_v(&display_sem);
		}

		if (warmup && test_loops == WARMUP_TIME) {
			test_loops = 0;
			warmup = 0;
		}
	}
}

void display(void *cookie)
{
	int err, n = 0;
	time_t start;
	char sem_name[16];

	if (test_mode == USER_TASK) {
		snprintf(sem_name, sizeof(sem_name), "dispsem-%d", getpid());
		err = rt_sem_create(&display_sem, sem_name, 0, S_FIFO);

		if (err) {
			fprintf(stderr,
				"latency: cannot create semaphore: %s\n",
				strerror(-err));
			return;
		}

	} else {
		struct rttst_tmbench_config config;

		if (test_mode == KERNEL_TASK)
			config.mode = RTTST_TMBENCH_TASK;
		else
			config.mode = RTTST_TMBENCH_HANDLER;

		config.period = period_ns;
		config.priority = priority;
		config.warmup_loops = WARMUP_TIME;
		config.histogram_size = need_histo() ? histogram_size : 0;
		config.histogram_bucketsize = bucketsize;
		config.freeze_max = freeze_max;

		err =
		    rt_dev_ioctl(benchdev, RTTST_RTIOC_TMBENCH_START, &config);

		if (err) {
			fprintf(stderr,
				"latency: failed to start in-kernel timer benchmark, code %d\n",
				err);
			return;
		}
	}

	time(&start);

	if (WARMUP_TIME)
		printf("warming up...\n");

	if (quiet)
		fprintf(stderr, "running quietly for %d seconds\n",
			test_duration);

	for (;;) {
		long minj, gminj, maxj, gmaxj, avgj;

		if (test_mode == USER_TASK) {
			err = rt_sem_p(&display_sem, TM_INFINITE);

			if (err) {
				if (err != -EIDRM)
					fprintf(stderr,
						"latency: failed to pend on semaphore, code %d\n",
						err);

				return;
			}

			/* convert jitters to nanoseconds. */
			minj = rt_timer_tsc2ns(minjitter);
			gminj = rt_timer_tsc2ns(gminjitter);
			avgj = rt_timer_tsc2ns(avgjitter);
			maxj = rt_timer_tsc2ns(maxjitter);
			gmaxj = rt_timer_tsc2ns(gmaxjitter);

		} else {
			struct rttst_interm_bench_res result;

			err =
			    rt_dev_ioctl(benchdev, RTTST_RTIOC_INTERM_BENCH_RES,
					 &result);

			if (err) {
				if (err != -EIDRM)
					fprintf(stderr,
						"latency: failed to call RTTST_RTIOC_INTERM_BENCH_RES, code %d\n",
						err);

				return;
			}

			minj = result.last.min;
			gminj = result.overall.min;
			avgj = result.last.avg;
			maxj = result.last.max;
			gmaxj = result.overall.max;
			goverrun = result.overall.overruns;
		}

		if (!quiet) {
			if (data_lines && (n++ % data_lines) == 0) {
				time_t now, dt;
				time(&now);
				dt = now - start - WARMUP_TIME;
				printf
				    ("RTT|  %.2ld:%.2ld:%.2ld  (%s, %Ld us period, "
				     "priority %d)\n", dt / 3600,
				     (dt / 60) % 60, dt % 60,
				     test_mode_names[test_mode],
				     period_ns / 1000, priority);
				printf("RTH|%11s|%11s|%11s|%8s|%6s|%11s|%11s\n",
				       "----lat min", "----lat avg",
				       "----lat max", "-overrun", "---msw",
				       "---lat best", "--lat worst");
			}
			printf("RTD|%11.3f|%11.3f|%11.3f|%8ld|%6u|%11.3f|%11.3f\n",
			       (double)minj / 1000,
			       (double)avgj / 1000,
			       (double)maxj / 1000,
			       goverrun,
			       max_relaxed,
			       (double)gminj / 1000, (double)gmaxj / 1000);
		}
	}
}

double dump_histogram(long *histogram, char *kind)
{
	int n, total_hits = 0;
	double avg = 0;		/* used to sum hits 1st */

	if (do_histogram)
		printf("---|--param|----range-|--samples\n");

	for (n = 0; n < histogram_size; n++) {
		long hits = histogram[n];

		if (hits) {
			total_hits += hits;
			avg += n * hits;
			if (do_histogram)
				printf("HSD|    %s| %3d -%3d | %8ld\n",
				       kind, n, n + 1, hits);
		}
	}

	avg /= total_hits;	/* compute avg, reuse variable */

	return avg;
}

void dump_histo_gnuplot(long *histogram)
{
	unsigned start, stop;
	FILE *f;
	int n;

	f = fopen(do_gnuplot, "w");
	if (!f)
		return;

	for (n = 0; n < histogram_size && histogram[n] == 0L; n++)
		;
	start = n;

	for (n = histogram_size - 1; n >= 0 && histogram[n] == 0L; n--)
		;
	stop = n;

	fprintf(f, "%g 1\n", start * bucketsize / 1000.0);
	for (n = start; n <= stop; n++)
		fprintf(f, "%g %ld\n",
			(n + 0.5) * bucketsize / 1000.0, histogram[n] + 1);
	fprintf(f, "%g 1\n", (stop + 1) * bucketsize / 1000.0);

	fclose(f);
}

void dump_stats(long *histogram, char *kind, double avg)
{
	int n, total_hits = 0;
	double variance = 0;

	for (n = 0; n < histogram_size; n++) {
		long hits = histogram[n];

		if (hits) {
			total_hits += hits;
			variance += hits * (n - avg) * (n - avg);
		}
	}

	/* compute std-deviation (unbiased form) */
	if (total_hits > 1) {
		variance /= total_hits - 1;
		variance = sqrt(variance);
	} else
		variance = 0;

	printf("HSS|    %s| %9d| %10.3f| %10.3f\n",
	       kind, total_hits, avg, variance);
}

void dump_hist_stats(void)
{
	double minavg, maxavg, avgavg;

	/* max is last, where its visible w/o scrolling */
	minavg = dump_histogram(histogram_min, "min");
	avgavg = dump_histogram(histogram_avg, "avg");
	maxavg = dump_histogram(histogram_max, "max");

	printf("HSH|--param|--samples-|--average--|---stddev--\n");

	dump_stats(histogram_min, "min", minavg);
	dump_stats(histogram_avg, "avg", avgavg);
	dump_stats(histogram_max, "max", maxavg);

	if (do_gnuplot)
		dump_histo_gnuplot(histogram_avg);
}

void cleanup(void)
{
	time_t actual_duration;
	long gmaxj, gminj, gavgj;

	if (test_mode == USER_TASK) {
		rt_sem_delete(&display_sem);

		gavgjitter /= (test_loops > 1 ? test_loops : 2) - 1;

		gminj = rt_timer_tsc2ns(gminjitter);
		gmaxj = rt_timer_tsc2ns(gmaxjitter);
		gavgj = rt_timer_tsc2ns(gavgjitter);
	} else {
		struct rttst_overall_bench_res overall;

		overall.histogram_min = histogram_min;
		overall.histogram_max = histogram_max;
		overall.histogram_avg = histogram_avg;

		rt_dev_ioctl(benchdev, RTTST_RTIOC_TMBENCH_STOP, &overall);

		gminj = overall.result.min;
		gmaxj = overall.result.max;
		gavgj = overall.result.avg;
		goverrun = overall.result.overruns;
	}

	if (benchdev >= 0)
		rt_dev_close(benchdev);

	if (need_histo())
		dump_hist_stats();

	time(&test_end);
	actual_duration = test_end - test_start - WARMUP_TIME;
	if (!test_duration)
		test_duration = actual_duration;

	printf
	    ("---|-----------|-----------|-----------|--------|------|-------------------------\n"
	     "RTS|%11.3f|%11.3f|%11.3f|%8ld|%6u|    %.2ld:%.2ld:%.2ld/%.2d:%.2d:%.2d\n",
	     (double)gminj / 1000, (double)gavgj / 1000, (double)gmaxj / 1000,
	     goverrun, max_relaxed, actual_duration / 3600, (actual_duration / 60) % 60,
	     actual_duration % 60, test_duration / 3600,
	     (test_duration / 60) % 60, test_duration % 60);
	if (max_relaxed > 0)
		printf(
"Warning! some latency maxima may have been due to involuntary mode switches.\n"
"Please contact rtai@rtai.org\n"); 
	if (histogram_avg)
		free(histogram_avg);
	if (histogram_max)
		free(histogram_max);
	if (histogram_min)
		free(histogram_min);

	exit(0);
}

void faulthand(int sig)
{
	xntrace_user_freeze(0, 1);
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

static const char *reason_str[] = {
	[SIGDEBUG_UNDEFINED] = "latency: received SIGXCPU for unknown reason",
	[SIGDEBUG_MIGRATE_SIGNAL] = "received signal",
	[SIGDEBUG_MIGRATE_SYSCALL] = "invoked syscall",
	[SIGDEBUG_MIGRATE_FAULT] = "triggered fault",
	[SIGDEBUG_MIGRATE_PRIOINV] = "affected by priority inversion",
	[SIGDEBUG_NOMLOCK] = "Xenomai: process memory not locked "
	"(missing mlockall?)",
	[SIGDEBUG_WATCHDOG] = "Xenomai: watchdog triggered "
	"(period too short?)",
};

static char buffer[256];

void sigdebug(int sig, siginfo_t *si, void *context)
{
#ifndef __UCLIBC__
	const char fmt[] = "Mode switch (reason: %s), aborting. Backtrace:\n";
	static void *bt[200];
#else /* __UCLIBC__ */
	const char fmt[] = "Mode switch (reason: %s), aborting."
		" Backtrace unavailable with uclibc.\n";
#endif /* __UCLIBC__ */
	unsigned int reason = si->si_value.sival_int;
	unsigned n;

	if (reason > SIGDEBUG_WATCHDOG)
		reason = SIGDEBUG_UNDEFINED;

	switch(reason) {
	case SIGDEBUG_UNDEFINED:
	case SIGDEBUG_NOMLOCK:
	case SIGDEBUG_WATCHDOG:
		n = snprintf(buffer, sizeof(buffer),
			     "%s\n", reason_str[reason]);
		write(STDERR_FILENO, buffer, n);
		exit(EXIT_FAILURE);
	}

	if (!stop_upon_switch) {
		++sampling_relaxed;
		return;
	}

	n = snprintf(buffer, sizeof(buffer), fmt, reason_str[reason]);
	n = write(STDERR_FILENO, buffer, n);
#ifndef __UCLIBC__
	n = backtrace(bt, sizeof(bt)/sizeof(bt[0]));
	backtrace_symbols_fd(bt, n, STDERR_FILENO);
#endif /* !__UCLIBC__ */

	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

int main(int argc, char **argv)
{
	int cpu = 0, c, err, sig;
	struct sigaction sa;
	char task_name[16];
	sigset_t mask;

	while ((c = getopt(argc, argv, "g:hp:l:T:qH:B:sD:t:fc:P:b")) != EOF)
		switch (c) {
		case 'g':
			do_gnuplot = strdup(optarg);
			break;

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

		case 'D':

			benchdev_no = atoi(optarg);
			break;

		case 't':

			test_mode = atoi(optarg);
			break;

		case 'f':

			freeze_max = 1;
			break;

		case 'c':
			cpu = T_CPU(atoi(optarg));
			break;

		case 'P':
			priority = atoi(optarg);
			break;

		case 'b':
			stop_upon_switch = 1;
			break;

		default:

			fprintf(stderr,
"usage: latency [options]\n"
"  [-h]                         # print histograms of min, avg, max latencies\n"
"  [-g <file>]                  # dump histogram to <file> in gnuplot format\n"
"  [-s]                         # print statistics of min, avg, max latencies\n"
"  [-H <histogram-size>]        # default = 200, increase if your last bucket is full\n"
"  [-B <bucket-size>]           # default = 1000ns, decrease for more resolution\n"
"  [-p <period_us>]             # sampling period\n"
"  [-l <data-lines per header>] # default=21, 0 to supress headers\n"
"  [-T <test_duration_seconds>] # default=0, so ^C to end\n"
"  [-q]                         # supresses RTD, RTH lines if -T is used\n"
"  [-D <testing_device_no>]     # number of testing device, default=0\n"
"  [-t <test_mode>]             # 0=user task (default), 1=kernel task, 2=timer IRQ\n"
"  [-f]                         # freeze trace for each new max latency\n"
"  [-c <cpu>]                   # pin measuring task down to given CPU\n"
"  [-P <priority>]              # task priority (test mode 0 and 1 only)\n"
"  [-b]                         # break upon mode switch\n"
);
			exit(2);
		}

	if (!test_duration && quiet) {
		fprintf(stderr,
			"latency: -q only works if -T has been given.\n");
		quiet = 0;
	}

	if ((test_mode < USER_TASK) || (test_mode > TIMER_HANDLER)) {
		fprintf(stderr, "latency: invalid test mode.\n");
		exit(2);
	}

	time(&test_start);

	histogram_avg = calloc(histogram_size, sizeof(long));
	histogram_max = calloc(histogram_size, sizeof(long));
	histogram_min = calloc(histogram_size, sizeof(long));

	if (!(histogram_avg && histogram_max && histogram_min))
		cleanup();

	if (period_ns == 0)
		period_ns = CONFIG_XENO_DEFAULT_PERIOD;	/* ns */

	if (priority <= T_LOPRIO)
		priority = T_LOPRIO + 1;
	else if (priority > T_HIPRIO)
		priority = T_HIPRIO;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &mask, NULL);

	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = sigdebug;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGDEBUG, &sa, NULL);

	if (freeze_max) {
		/* If something goes wrong, we want to freeze the current
		   trace path to help debugging. */
		signal(SIGSEGV, faulthand);
		signal(SIGBUS, faulthand);
	}

	setlinebuf(stdout);

	printf("== Sampling period: %Ld us\n"
	       "== Test mode: %s\n"
	       "== All results in microseconds\n",
	       period_ns / 1000, test_mode_names[test_mode]);

	mlockall(MCL_CURRENT | MCL_FUTURE);

	if (test_mode != USER_TASK) {
		char devname[RTDM_MAX_DEVNAME_LEN];

		snprintf(devname, RTDM_MAX_DEVNAME_LEN, "rttest-timerbench%d",
			 benchdev_no);
		benchdev = rt_dev_open(devname, O_RDWR);

		if (benchdev < 0) {
			fprintf(stderr,
				"latency: failed to open benchmark device, code %d\n"
				"(modprobe RTAI_timerbench?)\n", benchdev);
			return 0;
		}
	}

	rt_timer_set_mode(TM_ONESHOT);	/* Force aperiodic timing. */

	snprintf(task_name, sizeof(task_name), "display-%d", getpid());
	err = rt_task_create(&display_task, task_name, 0, 0, T_FPU);

	if (err) {
		fprintf(stderr,
			"latency: failed to create display task, code %d\n",
			err);
		return 0;
	}

	err = rt_task_start(&display_task, &display, NULL);

	if (err) {
		fprintf(stderr,
			"latency: failed to start display task, code %d\n",
			err);
		return 0;
	}

	if (test_mode == USER_TASK) {
		snprintf(task_name, sizeof(task_name), "sampling-%d", getpid());
		err =
		    rt_task_create(&latency_task, task_name, 0, priority,
				   T_FPU | cpu | T_WARNSW);

		if (err) {
			fprintf(stderr,
				"latency: failed to create latency task, code %d\n",
				err);
			return 0;
		}

		err = rt_task_start(&latency_task, &latency, NULL);

		if (err) {
			fprintf(stderr,
				"latency: failed to start latency task, code %d\n",
				err);
			return 0;
		}
	}

	sigwait(&mask, &sig);
	finished = 1;

	cleanup();

	return 0;
}
