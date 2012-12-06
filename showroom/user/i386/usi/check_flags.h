/*
COPYRIGHT (C) 2005  Michael Neuhauser <mike@firmix.at>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
*/


//#define DIAG_FLAGS  X86

#ifdef DIAG_FLAGS
 
#	if DIAG_FLAGS == X86
#		define CHECK_FLAGS() do { \
 			unsigned long flags; \
 			__asm__ __volatile__("pushfl; popl %0": "=g" (flags)); \
 			if (flags & (1 << 9)) rt_printk("< BAD! ENABLED >\n"); \
 		} while (0);
#	elif DIAG_FLAGS == ARM
#		include <asm/ptrace.h>
#		define CHECK_FLAGS() do { \
 			unsigned long flags; \
 			__asm__ __volatile__("mrs %0, cpsr" : "=r" (flags)); \
			if (!(flags & I_BIT)) rt_printk("< BAD! ENABLED >\n"); \
		} while (0);
#	else
#		error "sorry, architecture is not supported"
#	endif
 
#else /* !DIAG_FLAGS */
 
#	define CHECK_FLAGS()	do { /* nop */ } while (0)
 
#endif /* DIAG_FLAGS */
