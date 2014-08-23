/* @(#)w_fmod.c 5.1 93/09/24 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$NetBSD: w_fmod.c,v 1.6 1995/05/10 20:48:55 jtc Exp $";
#endif

/* 
 * wrapper fmod(x,y)
 */

#include "mathP.h"

	double fmod(double x, double y)	/* wrapper fmod */
{
	return __ieee754_fmod(x,y);
}
