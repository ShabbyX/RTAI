/*
 * Copyright (C) 2005 Joerg Langenberg <joerg.langenberg@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include <stdarg.h>
#include <stddef.h>

#include <rtdm/rtdm.h>
#include <rtdm/syscall.h>

extern int __rtdm_muxid;

static inline int rt_dev_open(const char *path, int oflag, ...)
{
	return XENOMAI_SKINCALL2( __rtdm_muxid, __rtdm_open, path, oflag);
}

static inline int rt_dev_socket(int protocol_family, int socket_type, int protocol)
{
	return XENOMAI_SKINCALL3( __rtdm_muxid, __rtdm_socket, protocol_family, socket_type, protocol);
}

static inline int rt_dev_close(int fd)
{
	return XENOMAI_SKINCALL1( __rtdm_muxid, __rtdm_close, fd);
}

static inline int rt_dev_ioctl(int fd, int request, ...)
{
	va_list ap;
	va_start(ap, request);
	void *arg = va_arg(ap, void*);
	va_end(ap);
	return XENOMAI_SKINCALL3( __rtdm_muxid, __rtdm_ioctl, fd, request, arg);
}

static inline ssize_t rt_dev_read(int fd, void *buf, size_t nbyte)
{
	return XENOMAI_SKINCALL3( __rtdm_muxid, __rtdm_read, fd, buf, nbyte);
}

ssize_t rt_dev_write(int fd, const void *buf, size_t nbyte)
{
  return XENOMAI_SKINCALL3( __rtdm_muxid,
			    __rtdm_write,
			    fd,
			    buf,
			    nbyte);
}

static inline ssize_t rt_dev_recvmsg(int fd, struct msghdr *msg, int flags)
{
	return XENOMAI_SKINCALL3( __rtdm_muxid, __rtdm_recvmsg, fd, msg, flags);
}

static inline ssize_t rt_dev_sendmsg(int fd, const struct msghdr *msg, int flags)
{
	return XENOMAI_SKINCALL3( __rtdm_muxid, __rtdm_sendmsg, fd, msg, flags);
}

static inline ssize_t rt_dev_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	struct iovec    iov = {buf, len};
	struct msghdr   msg =
	{ from, (from != NULL) ? *fromlen : 0, &iov, 1, NULL, 0 };
	int             ret;

	ret = XENOMAI_SKINCALL3( __rtdm_muxid, __rtdm_recvmsg, fd, &msg, flags);
	if ((ret >= 0) && (from != NULL)) {
		*fromlen = msg.msg_namelen;
	}
	return ret;
}
