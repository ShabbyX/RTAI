#include "rtai_wrapper.h"

extern "C"
{

#include "rtai.h"
#include "rtai_malloc.h"

}

#if __GNUC__ < 3
 /* use __builtin_delete() */
#else

void operator delete(void* vp)
{
	rt_printk("my __builtin_delete %p\n",vp);

	if(vp != 0)
		rt_free(vp);
}

/* for gcc-3.3 */

void operator delete[](void* vp){
	if(vp != 0)
		rt_free(vp);
}

void *operator new(size_t size){
	void* vp = rt_malloc(size);
	return vp;
}

void *operator new[](size_t size){
	void* vp = rt_malloc(size);
	return vp;
}

/* __cxa_pure_virtual(void) support */

extern "C" void __cxa_pure_virtual(void)
{
	rt_printk("attempt to use a virtual function before object has been constructed\n");
	for ( ; ; );
}


#endif
