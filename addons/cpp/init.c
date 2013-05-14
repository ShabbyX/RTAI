/*
 * Project: rtai_cpp - RTAI C++ Framework
 *
 * File: $Id: init.c,v 1.3 2005/03/18 09:29:59 rpm Exp $
 *
 * Copyright: (C) 2001,2002 Erwin Rol <erwin@muffin.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <asm/unistd.h>

#include <rtai_malloc.h>
#include "tld_key.h"

extern void init_iostream( void );

// RTAI-- MODULE INIT and CLEANUP functions

MODULE_LICENSE("GPL");
MODULE_AUTHOR("the RTAI-Team (contact person Erwin Rol)");
MODULE_DESCRIPTION("RTAI C++ support");

int __init rtai_cpp_init(void){

	cpp_key = __rt_tld_create_key();
	
	if(cpp_key == -1){
		rt_printk("Could not get free TLD key\n");
		return -1;
	}
	
	rt_printk("Got %d TLD key\n",cpp_key);

	init_iostream();
		
	return 0;
}

void rtai_cpp_cleanup(void)
{
	if(cpp_key != -1)
		__rt_tld_free_key(cpp_key);
}

module_init(rtai_cpp_init)
module_exit(rtai_cpp_cleanup)

