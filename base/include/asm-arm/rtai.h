/*
 * RTAI/ARM over Adeos -- based on ARTI for x86 and RTHAL for ARM.
 *
 * Original RTAI/x86 layer implementation:
 *   Copyright (C) 2000 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *   Copyright (C) 2000 Steve Papacharalambous <stevep@freescale.com>
 *   Copyright (C) 2000 Stuart Hughes <stuarth@lineo.com>
 *   and others.
 *
 * RTAI/x86 rewrite over Adeos:
 *   Copyright (C) 2002 Philippe Gerum <rpm@xenomai.org>
 *
 * Original RTAI/ARM RTHAL implementation:
 *   Copyright (C) 2000 Pierre Cloutier <pcloutier@poseidoncontrols.com>
 *   Copyright (C) 2001 Alex Züpke, SYSGO RTS GmbH <azu@sysgo.de>
 *   Copyright (C) 2002 Guennadi Liakhovetski DSA GmbH <gl@dsa-ac.de>
 *   Copyright (C) 2002 Steve Papacharalambous <stevep@freescale.com>
 *   Copyright (C) 2002 Wolfgang Müller <wolfgang.mueller@dsa-ac.de>
 *   Copyright (C) 2003 Bernard Haible, Marconi Communications
 *   Copyright (C) 2003 Thomas Gleixner <tglx@linutronix.de>
 *   Copyright (C) 2003 Philippe Gerum <rpm@xenomai.org>
 *
 * RTAI/ARM over Adeos rewrite:
 *   Copyright (C) 2004-2005 Michael Neuhauser, Firmix Software GmbH <mike@firmix.at>
 *
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge MA 02139, USA; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <rtai_config.h>
#include <asm/rtai_hal.h>
