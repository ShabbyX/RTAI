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

#ifndef _RTAI_ASM_M68K_LEDS_H
#define _RTAI_ASM_M68K_LEDS_H

#include <asm/io.h>

static inline void leds_parport_func(unsigned long port, unsigned leds)
{
	outb(~leds,port);
}

#define LEDS_DEFAULT_FUNC leds_parport_func
#define LEDS_DEFAULT_PORT MCF_MBAR + MCFSIM_PADAT

#endif /* !_RTAI_ASM_M68K_LEDS_H */
