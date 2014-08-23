/* @(#)w_j0.c 5.1 93/09/24 */
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
 * wrapper j0(double x), y0(double x)
 */

#include "mathP.h"

	double j0(double x)	/* wrapper j0 */
{
	return __ieee754_j0(x);
}

	double y0(double x)	/* wrapper y0 */
{
	return __ieee754_y0(x);
}
