/* @(#)w_pow.c 5.2 93/10/01 */
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

/*
 * wrapper pow(x,y) return x**y
 */

#include "mathP.h"

	double pow(double x, double y)	/* wrapper pow */
{
	return  __ieee754_pow(x,y);
}
