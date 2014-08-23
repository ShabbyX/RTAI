/*
 * Copyright (C) 2005 Jan Kiszka <jan.kiszka@web.de>.
 * Copyright (C) 2005 Joerg Langenberg <joerg.langenberg@gmx.net>.
 *
 * with adaptions for RTAI by Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * RTAI is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * RTAI is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RTAI; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _RTDM_DEVICE_H
#define _RTDM_DEVICE_H

#include <rtdm/rtdm_driver.h>
#include <linux/sem.h>


#define DEF_DEVNAME_HASHTAB_SIZE    256 /* entries in name hash table */
#define DEF_PROTO_HASHTAB_SIZE      256 /* entries in protocol hash table */


extern struct semaphore nrt_dev_lock;
extern xnlock_t	 rt_dev_lock;

extern unsigned int     devname_hashtab_size;
extern unsigned int     protocol_hashtab_size;

extern struct list_head *rtdm_named_devices;
extern struct list_head *rtdm_protocol_devices;

#ifdef MODULE
#define rtdm_initialised 1
#else /* !MODULE */
extern int	      rtdm_initialised;
#endif /* MODULE */


int rtdm_no_support(void);

struct rtdm_device *get_named_device(const char *name);
struct rtdm_device *get_protocol_device(int protocol_family, int socket_type);

static inline void rtdm_dereference_device(struct rtdm_device *device)
{
    atomic_dec(&device->reserved.refcount);
}

int __init rtdm_dev_init(void);

static inline void rtdm_dev_cleanup(void)
{
    kfree(rtdm_named_devices);
    kfree(rtdm_protocol_devices);
}

#endif /* _RTDM_DEVICE_H */
