#define CALSRQ        0xcacca

#define CAL_8254      1
#define KLATENCY      2
#define KTHREADS      3
#define END_KLATENCY  4
#define FREQ_CAL      5
#define END_FREQ_CAL  6
#define BUS_CHECK     7
#define END_BUS_CHECK 8

#define PARPORT       0x370

#define MAXARGS       4
#define STACKSIZE     2000
#define FIFOBUFSIZE   1000
#define INILOOPS      100

struct params_t { unsigned long 
	mp,
	setup_time_8254, 
	latency_8254,  
	freq_apic,
	latency_apic,
	setup_time_apic,
	calibrated_apic_freq,
	cpu_freq,
	calibrated_cpu_freq,
	clock_tick_rate,
	latch;
};

struct times_t { 
	unsigned long long cpu_time;
	unsigned long apic_time;
	int intrs;
};
