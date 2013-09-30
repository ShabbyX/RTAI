/*!\file select.h
 * \brief file descriptors events multiplexing header.
 * \author Gilles Chanteperdrix
 *
 * Copyright (C) 2008 Efixo <gilles.chanteperdrix@laposte.net>
 *
 * Rtai is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Rtai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rtai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * \ingroup select
 */

#ifndef XNSELECT_H
#define XNSELECT_H

/*! \addtogroup select
 *@{*/

#include "xn.h"

#define XNSELECT_READ      0
#define XNSELECT_WRITE     1
#define XNSELECT_EXCEPT    2
#define XNSELECT_MAX_TYPES 3

struct xnselector {
	xnsynch_t synchbase;
	struct fds {
		fd_set expected;
		fd_set pending;
	} fds [XNSELECT_MAX_TYPES];
	xnholder_t destroy_link;
	xnqueue_t bindings; /* only used by xnselector_destroy */
};

#define __NFDBITS__	(8 * sizeof(unsigned long))
#define __FDSET_LONGS__	(__FD_SETSIZE/__NFDBITS__)
#define	__FDELT__(d)	((d) / __NFDBITS__)
#define	__FDMASK__(d)	(1UL << ((d) % __NFDBITS__))

static inline void __FD_SET__(unsigned long __fd, __kernel_fd_set *__fdsetp)
{
	 unsigned long __tmp = __fd / __NFDBITS__;
	 unsigned long __rem = __fd % __NFDBITS__;
	 __fdsetp->fds_bits[__tmp] |= (1UL<<__rem);
}

static inline void __FD_CLR__(unsigned long __fd, __kernel_fd_set *__fdsetp)
{
	 unsigned long __tmp = __fd / __NFDBITS__;
	 unsigned long __rem = __fd % __NFDBITS__;
	 __fdsetp->fds_bits[__tmp] &= ~(1UL<<__rem);
}

static inline int __FD_ISSET__(unsigned long __fd, const __kernel_fd_set *__p)
{
	 unsigned long __tmp = __fd / __NFDBITS__;
	 unsigned long __rem = __fd % __NFDBITS__;
	 return (__p->fds_bits[__tmp] & (1UL<<__rem)) != 0;
}

static inline void __FD_ZERO__(__kernel_fd_set *__p)
{
	unsigned long *__tmp = __p->fds_bits;
	int __i;

	__i = __FDSET_LONGS__;
	while (__i) {
		__i--;
		*__tmp = 0;
		__tmp++;
	}
}

#ifdef CONFIG_RTAI_RTDM_SELECT

struct xnselect {
	xnqueue_t bindings;
};

#define DECLARE_XNSELECT(name) struct xnselect name

struct xnselect_binding {
	struct xnselector *selector;
	struct xnselect *fd;
	unsigned type;
	unsigned bit_index;
	xnholder_t link;  /* link in selected fds list. */
	xnholder_t slink; /* link in selector list */
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void xnselect_init(struct xnselect *select_block);

int xnselect_bind(struct xnselect *select_block,
		  struct xnselect_binding *binding,
		  struct xnselector *selector,
		  unsigned type,
		  unsigned bit_index,
		  unsigned state);

int __xnselect_signal(struct xnselect *select_block, unsigned state);

/**
 * Signal a file descriptor state change.
 *
 * @param select_block pointer to an @a xnselect structure representing the file
 * descriptor whose state changed;
 * @param state new value of the state.
 *
 * @retval 1 if rescheduling is needed;
 * @retval 0 otherwise.
 */
static inline int
xnselect_signal(struct xnselect *select_block, unsigned state)
{
	if (!emptyq_p(&select_block->bindings))
		return __xnselect_signal(select_block, state);
	return 0;
}

void xnselect_destroy(struct xnselect *select_block);

int xnselector_init(struct xnselector *selector);

int xnselect(struct xnselector *selector,
	     fd_set *out_fds[XNSELECT_MAX_TYPES],
	     fd_set *in_fds[XNSELECT_MAX_TYPES],
	     int nfds,
	     xnticks_t timeout, xntmode_t timeout_mode);

void xnselector_destroy(struct xnselector *selector);

int xnselect_mount(void);

int xnselect_umount(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#else /* !CONFIG_RTAI_RTDM_SELECT */
struct xnselector;
#define DECLARE_XNSELECT(name)
#define xnselect_init(block)
#define xnselect_bind(select_block,binding,selector,type,bit_index,state) \
	({ -EBADF; })
#define xnselect_signal(block, state) ({ int __ret = 0; __ret; })
#define xnselect_destroy(block)
#endif /* !CONFIG_RTAI_RTDM_SELECT */

/*@}*/

/*
 * In Linux 3.5.7, some __F* constants defined in linux/posix_types.h have been removed.
 * They are quite simple constants, used in core.c and select.c so I'm putting them here
 */

#ifndef __NFDBITS
# define __NFDBITS	(8 * sizeof(long))
#endif

#ifndef __FDSET_LONGS
# define __FDSET_LONGS	(__FD_SETSIZE/__NFDBITS)
#endif

#ifndef __FDELT
# define __FDELT(d)	((d) / __NFDBITS)
#endif

#ifndef __FDMASK
# define __FDMASK(d)	(1UL << ((d) % __NFDBITS))
#endif

#ifndef __FD_SET
static inline void __FD_SET(unsigned long __fd, __kernel_fd_set *__fdsetp)
{
	__set_bit(__fd, __fdsetp->fds_bits);
}
#endif

#ifndef __FD_CLR
static inline void __FD_CLR(unsigned long __fd, __kernel_fd_set *__fdsetp)
{
	__clear_bit(__fd, __fdsetp->fds_bits);
}
#endif

#ifndef __FD_ISSET
static inline int __FD_ISSET(unsigned long __fd, const __kernel_fd_set *__fdsetp)
{
	return test_bit(__fd, __fdsetp->fds_bits);
}
#endif

#ifndef __FD_ZERO
static inline void __FD_ZERO(__kernel_fd_set *__fdsetp)
{
	memset(__fdsetp->fds_bits, 0, sizeof __fdsetp->fds_bits);
}
#endif

#endif /* XNSELECT_H */
