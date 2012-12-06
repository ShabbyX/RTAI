/*
 * rtai_leds.c - mini-driver for generic control of digital signals
 *
 * Copyright (C) 2000  Pierre Cloutier <pcloutier@PoseidonControls.com>
 *               2001  David A. Schleef <ds@schleef.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <rtai.h>
#include <rtai_leds.h>

MODULE_LICENSE("GPL");

static unsigned int leds;
static unsigned long port;

static void (*leds_func)(unsigned long port, unsigned int leds);

void rt_leds_set_mask(unsigned int mask, unsigned int value)
{
	leds &= ~mask;
	leds |= (mask & value);
	leds_func(port, leds);
}

void rt_toggle_leds(unsigned int l)
{
	leds ^= l;
	leds_func(port, leds);
}

void rt_reset_leds(unsigned int l)
{
	leds &= ~l;
	leds_func(port, leds);
}

void rt_set_leds(unsigned int l)
{
	leds |= l;
	leds_func(port, leds);
}

void rt_clear_leds(void)
{
	leds = 0;
	leds_func(port, leds);
}

unsigned int rt_get_leds(void)
{
	return leds;
}

void rt_set_leds_port(int p)
{
	port = p;
}

void rt_config_leds(unsigned int type,
	void (*func)(unsigned long p, unsigned int l),
	unsigned long p)
{

	switch(type){
	case 0:
		leds_func = func;
		port = p;
		break;
	case 1:
		leds_func = LEDS_DEFAULT_FUNC;
		port = LEDS_DEFAULT_PORT;
		break;
	}
}

int __rtai_leds_init(void)
{
	rt_config_leds(1,NULL,0);

	printk(KERN_INFO "RTAI[leds]: loaded.\n");
	return(0);
}

void __rtai_leds_exit(void)
{
	printk(KERN_INFO "RTAI[leds]: unloaded.\n");
}

#ifndef CONFIG_RTAI_LEDS_BUILTIN
module_init(__rtai_leds_init);
module_exit(__rtai_leds_exit);
#endif /* !CONFIG_RTAI_LEDS_BUILTIN */

#ifdef CONFIG_KBUILD
EXPORT_SYMBOL(rt_leds_set_mask);
EXPORT_SYMBOL(rt_toggle_leds);
EXPORT_SYMBOL(rt_reset_leds);
EXPORT_SYMBOL(rt_set_leds);
EXPORT_SYMBOL(rt_clear_leds);
EXPORT_SYMBOL(rt_get_leds);
EXPORT_SYMBOL(rt_set_leds_port);
EXPORT_SYMBOL(rt_config_leds);
#endif /* CONFIG_KBUILD */
