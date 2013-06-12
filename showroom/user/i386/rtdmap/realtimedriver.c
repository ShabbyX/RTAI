/**********************************************************/
/*    HARD REAL TIME RTDM Driver Skeleton V1.1            */
/*                                                        */
/*    Based on code of Jan Kiszka - thanks Jan!           */
/*    (C) 2006 Jan Kiszka, www.captain.at                 */
/*    License: GPL                                        */
/**********************************************************/

#include <linux/mman.h>
#include <rtdm/rtdm_driver.h>
#include "inc.h"

char dummy_buffer[BUFSIZE];
int  events;
int  ioctlvalue;

// our driver context struct: used for storing various information
struct demodrv_context {
    rtdm_irq_t              irq_handle;
    rtdm_lock_t             lock;
    int                     dev_id;
    u64                     last_timestamp;
    rtdm_event_t            irq_event;
    volatile unsigned long  irq_event_lock;
    volatile int            irq_events;
    int64_t                 timeout;
    void                    *buf;
    rtdm_user_info_t        *mapped_user_info;
    void                    *mapped_user_addr;
};

#ifdef USEMMAP
static void demo_vm_open(struct vm_area_struct *vma)
{
    printk("opening %p, private_data = %p\n", vma, vma->vm_private_data);
}
static void demo_vm_close(struct vm_area_struct *vma)
{
    printk("releasing %p, private_data = %p\n", vma, vma->vm_private_data);
}
static struct vm_operations_struct mmap_ops = {
    .open = demo_vm_open,
    .close = demo_vm_close,
};
#endif

/**********************************************************/
/*            INTERRUPT HANDLING                          */
/**********************************************************/
static int demo_interrupt(rtdm_irq_t *irq_context)
{
    struct demodrv_context *ctx;
    int           dev_id;
//    timestamp, if needed, can be obtained list this:
//    u64           timestamp = rtdm_clock_read();
    int           ret = RTDM_IRQ_HANDLED; // usual return value

    ctx = rtdm_irq_get_arg(irq_context, struct demodrv_context);
    dev_id    = ctx->dev_id;

    rtdm_lock_get(&ctx->lock);

    // do stuff
#ifdef TIMERINT
    if (events > EVENT_SIGNAL_COUNT) {
	rtdm_event_signal(&ctx->irq_event);
	events = 0;
    }
#else
    rtdm_event_signal(&ctx->irq_event);
#endif
    events++;

    rtdm_lock_put(&ctx->lock);
    // those return values were dropped from the RTDM
    // ret = RTDM_IRQ_ENABLE | RTDM_IRQ_PROPAGATE;

#ifdef TIMERINT
    // Only propagate the timer interrupt, so that linux sees it.
    // Forwarding interrupts to the non-realtime domain is not a common
    //     use-case of realtime device drivers, so usually DON'T DO THIS.
    // But here we grab the important timer interrupt, so we need to propagate it.
    return XN_ISR_PROPAGATE;
#else
    // signal interrupt is handled and don't propagate the interrupt to linux
    return RTDM_IRQ_HANDLED;
#endif
}

/**********************************************************/
/*            DRIVER OPEN                                 */
/**********************************************************/
int demo_open_rt(struct rtdm_dev_context    *context,
		 rtdm_user_info_t           *user_info,
		 int                        oflags)
{
    struct demodrv_context  *my_context;
#ifdef USEMMAP
    unsigned long vaddr;
#endif
    int dev_id = context->device->device_id;
    int ret;

    // get the context for our driver - used to store driver info
    my_context = (struct demodrv_context *)context->dev_private;

#ifdef USEMMAP
    // allocate and prepare memory for our buffer
    my_context->buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    /* mark pages reserved so that remap_pfn_range works */
    for (vaddr = (unsigned long)my_context->buf;
	 vaddr < (unsigned long)my_context->buf + BUFFER_SIZE;
	 vaddr += PAGE_SIZE)
	SetPageReserved(virt_to_page(vaddr));
    // write some test value to the start of our buffer
    *(int *)my_context->buf = 1234;

    my_context->mapped_user_addr = NULL;
#endif

    // we also have an interrupt handler:
#ifdef TIMERINT
    ret = rtdm_irq_request(&my_context->irq_handle, TIMER_INT, demo_interrupt,
		 0, context->device->proc_name, my_context);
#else
    ret = rtdm_irq_request(&my_context->irq_handle, PAR_INT, demo_interrupt,
		 0, context->device->proc_name, my_context);
#endif
    if (ret < 0)
	return ret;

    /* IPC initialisation - cannot fail with used parameters */
    rtdm_lock_init(&my_context->lock);
    rtdm_event_init(&my_context->irq_event, 0);
    my_context->dev_id         = dev_id;

    my_context->irq_events     = 0;
    my_context->irq_event_lock = 0;
    my_context->timeout = 0; // wait INFINITE

#ifndef TIMERINT
    //set port to interrupt mode; pins are output
    outb_p(0x10, BASEPORT + 2);
#endif
    // enable interrupt in RTDM
    rtdm_irq_enable(&my_context->irq_handle);
    return 0;
}

/**********************************************************/
/*            DRIVER CLOSE                                */
/**********************************************************/
int demo_close_rt(struct rtdm_dev_context   *context,
		  rtdm_user_info_t          *user_info)
{
    struct demodrv_context  *my_context;
    rtdm_lockctx_t          lock_ctx;
#ifdef USEMMAP
    unsigned long vaddr;
#endif
    // get the context
    my_context = (struct demodrv_context *)context->dev_private;

#ifdef USEMMAP
    // printk some test value
    printk("%d\n", *((int *)my_context->buf + 10));

    // munmap our buffer
    if (my_context->mapped_user_addr) {
	int ret = rtdm_munmap(my_context->mapped_user_info,
			      my_context->mapped_user_addr, BUFFER_SIZE);
	printk("rtdm_munmap = %p, %d\n", my_context->mapped_user_info, ret);
    }

    /* clear pages reserved */
    for (vaddr = (unsigned long)my_context->buf;
	 vaddr < (unsigned long)my_context->buf + BUFFER_SIZE;
	 vaddr += PAGE_SIZE)
	ClearPageReserved(virt_to_page(vaddr));

    kfree(my_context->buf);
#endif

    // if we need to do some stuff with preemption disabled:
    rtdm_lock_get_irqsave(&my_context->lock, lock_ctx);
    // other stuff here
    rtdm_lock_put_irqrestore(&my_context->lock, lock_ctx);

    // free irq in RTDM
    rtdm_irq_free(&my_context->irq_handle);
    // destroy our interrupt signal/event
    rtdm_event_destroy(&my_context->irq_event);
    return 0;
}


/**********************************************************/
/*            DRIVER IOCTL                                */
/**********************************************************/
int demo_ioctl_rt(struct rtdm_dev_context   *context,
		  rtdm_user_info_t          *user_info,
		  unsigned int              request,
		  void                      *arg)
{
    struct demodrv_context  *my_context;
#ifdef USEMMAP
    int err;
#endif
    int ret = 0;
    my_context = (struct demodrv_context *)context->dev_private;

    switch (request) {
	case MMAP: // set mmap pointer
#ifdef USEMMAP
	  printk("buf = %p:%x\n", my_context->buf, *(int *)my_context->buf);

	  err = rtdm_mmap_to_user(user_info, my_context->buf, BUFFER_SIZE,
			      PROT_READ|PROT_WRITE, (void **)arg, &mmap_ops,
			      (void *)0x12345678);
	  if (!err) {
	      my_context->mapped_user_info = user_info;
	      my_context->mapped_user_addr = *(void **)arg;
	  }
	  printk("rtdm_mmap = %p %d\n", my_context->mapped_user_info, err);
#else
	  return -EPERM;
#endif
	  break;
	case GETVALUE: // write "ioctlvalue" to user
	    if (user_info) {
		if (!rtdm_rw_user_ok(user_info, arg, sizeof(int)) ||
		    rtdm_copy_to_user(user_info, arg, &ioctlvalue,
				      sizeof(int)))
		    return -EFAULT;
	    } else
		memcpy(arg, &ioctlvalue, sizeof(int));
	    break;
	case SETVALUE: // read "ioctlvalue" from user
	    if (user_info) {
		if ((unsigned long)arg < BUFFER_SIZE)
		    ioctlvalue = ((int *)my_context->buf)[(unsigned long)arg];
		else if (!rtdm_read_user_ok(user_info, arg, sizeof(int)) ||
		    rtdm_copy_from_user(user_info, &ioctlvalue, arg,
					sizeof(int)))
		    return -EFAULT;
	    }
	    break;
	default:
	    ret = -ENOTTY;
    }
    return ret;
}

/**********************************************************/
/*            DRIVER READ                                 */
/**********************************************************/
int demo_read_rt(struct rtdm_dev_context *context,
		  rtdm_user_info_t *user_info, void *buf, size_t nbyte)
{
    struct demodrv_context *ctx;
    int                     dev_id;
//    rtdm_lockctx_t          lock_ctx;
    char                    *out_pos = (char *)buf;
    rtdm_toseq_t            timeout_seq;
    int                     ret;

    // zero bytes requested ? return!
    if (nbyte == 0)
	return 0;

    // check if R/W actions to user-space are allowed
    if (user_info && !rtdm_rw_user_ok(user_info, buf, nbyte))
	return -EFAULT;

    ctx    = (struct demodrv_context *)context->dev_private;
    dev_id = ctx->dev_id;

    // in case we need to check if reading is allowed (locking)
/*    if (test_and_set_bit(0, &ctx->in_lock))
	return -EBUSY;
*/
/*  // if we need to do some stuff with preemption disabled:
    rtdm_lock_get_irqsave(&ctx->lock, lock_ctx);
    // stuff here
    rtdm_lock_put_irqrestore(&ctx->lock, lock_ctx);
*/

    // wait: if ctx->timeout = 0, it will block infintely until
    //       rtdm_event_signal(&ctx->irq_event); is called from our
    //       interrupt routine
    ret = rtdm_event_timedwait(&ctx->irq_event, ctx->timeout, &timeout_seq);

    // now write the requested stuff to user-space
    if (rtdm_copy_to_user(user_info, out_pos,
		dummy_buffer, BUFSIZE) != 0) {
	ret = -EFAULT;
    } else {
	ret = BUFSIZE;
    }
    return ret;
}

/**********************************************************/
/*            DRIVER WRITE                                */
/**********************************************************/
int demo_write_rt(struct rtdm_dev_context *context,
		   rtdm_user_info_t *user_info,
		   const void *buf, size_t nbyte)
{
    struct demodrv_context *ctx;
    int                     dev_id;
    char                    *in_pos = (char *)buf;
    int                     ret;

    if (nbyte == 0)
	return 0;
    if (user_info && !rtdm_read_user_ok(user_info, buf, nbyte))
	return -EFAULT;

    ctx    = (struct demodrv_context *)context->dev_private;
    dev_id = ctx->dev_id;

    // make write operation atomic
/*  ret = rtdm_mutex_timedlock(&ctx->out_lock,
	      ctx->config.rx_timeout, &timeout_seq);
    if (ret)
	return ret; */

/*  // if we need to do some stuff with preemption disabled:
    rtdm_lock_get_irqsave(&ctx->lock, lock_ctx);
    // stuff here
    rtdm_lock_put_irqrestore(&ctx->lock, lock_ctx);
*/

    // now copy the stuff from user-space to our kernel dummy_buffer
    if (rtdm_copy_from_user(user_info, dummy_buffer,
			     in_pos, BUFSIZE) != 0) {
	ret = -EFAULT;
    } else {
       ret = BUFSIZE;
    }

    // used when it is atomic
//   rtdm_mutex_unlock(&ctx->out_lock);
    return ret;
}

/**********************************************************/
/*            DRIVER OPERATIONS                           */
/**********************************************************/
static struct rtdm_device demo_device = {
    struct_version:     RTDM_DEVICE_STRUCT_VER,

    device_flags:       RTDM_NAMED_DEVICE,
    context_size:       sizeof(struct demodrv_context),
    device_name:        DEV_FILE,

/* open and close functions are not real-time safe due kmalloc
   and kfree. If you do not use kmalloc and kfree, and you made
   sure that there is no syscall in the open/close handler, you
   can declare the open_rt and close_rt handler.
*/
    open_rt:            NULL,
    open_nrt:           demo_open_rt,

    ops: {
	close_rt:       NULL,
	close_nrt:      demo_close_rt,

	ioctl_rt:       NULL,
	ioctl_nrt:      demo_ioctl_rt, // rtdm_mmap_to_user is not RT safe

	read_rt:        demo_read_rt,
	read_nrt:       NULL,

	write_rt:       demo_write_rt,
	write_nrt:      NULL,

	recvmsg_rt:     NULL,
	recvmsg_nrt:    NULL,

	sendmsg_rt:     NULL,
	sendmsg_nrt:    NULL,
    },

    device_class:       RTDM_CLASS_EXPERIMENTAL,
    device_sub_class:   222,
    driver_name:        DRV_NAME,
    peripheral_name:    DEV_FILE_NAME,
    provider_name:      "-",
    proc_name:          demo_device.device_name,
};

/**********************************************************/
/*            INIT DRIVER                                 */
/**********************************************************/
int init_module(void)
{
    return rtdm_dev_register(&demo_device);
}

/**********************************************************/
/*            CLEANUP DRIVER                              */
/**********************************************************/
void cleanup_module(void)
{
    rtdm_dev_unregister(&demo_device, 1000);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RTDM Real Time Driver Example");
