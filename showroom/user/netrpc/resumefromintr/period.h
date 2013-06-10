#define SEMAFORO        // uncomment this to use semaphore wait/signal
#define TEST_TIME 5     // secs
#define RTC_FREQ      1024
#define PERIOD  (1000000000 + RTC_FREQ/2)/RTC_FREQ // nanos
#define TASK_CPU  (1 << 1)
#define IRQ_CPU   (1 << 0)
