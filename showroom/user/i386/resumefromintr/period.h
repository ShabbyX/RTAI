#define RTC_FREQ  2048 //8192
#define PERIOD  ((1000000000 + RTC_FREQ/2)/RTC_FREQ) // nanos
#define TASK_CPU  (1 << 0)
#define IRQ_CPU   (1 << 0)
