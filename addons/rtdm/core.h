/*
 * Copyright (C) 2005 Jan Kiszka <jan.kiszka@web.de>
 * Copyright (C) 2005 Joerg Langenberg <joerg.langenberg@gmx.net>
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

#ifndef _RTDM_CORE_H
#define _RTDM_CORE_H

#include <rtdm/rtdm_driver.h>


#ifdef CONFIG_SMP
extern xnlock_t             rt_fildes_lock;
#endif /* CONFIG_SMP */

#define RTDM_FD_MAX         CONFIG_RTAI_RTDM_FD_MAX

struct rtdm_fildes {
    struct rtdm_dev_context *context;
};

extern struct rtdm_fildes   fildes_table[];
extern int                  open_fildes;

#endif /* _RTDM_CORE_H */
