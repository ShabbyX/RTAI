/* @(#)w_gamma.c 5.1 93/09/24 */
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

/* double gamma(double x)
 * Return the logarithm of the Gamma function of x.
 *
 * Method: call gamma_r
 */

#include "mathP.h"

extern int signgam;

	double gamma(double x)
{
	return __ieee754_lgamma_r(x,&signgam);
}
