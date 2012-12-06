// if TIMERINT is defined, the linux timer interrupt is used
//   otherwise the parallel port interrupt is used
#define TIMERINT

// if USEMMAP IS defined, also MMAP operations will be made
#define USEMMAP

#define DEV_FILE                  "demodev0"
#define DEV_FILE_NAME             "demodev"
#define DRV_NAME                  "demodrv"

#define BUFFER_SIZE     (128*1024)
#define BUFSIZE         17
#define PAR_INT         7
#define TIMER_INT       0
#define BASEPORT        0x378
#define MMAP            0
#define GETVALUE        1
#define SETVALUE        2

#define EVENT_SIGNAL_COUNT  0

#define STATE_FILE_OPENED         1
#define STATE_TASK_CREATED        2
