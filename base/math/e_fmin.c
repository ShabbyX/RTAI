/* e_fmin.c Alec Ari */
/*
 * ====================================================
 * Copyright (C) 2014 by Alec Ari, All rights reserved.
 * ====================================================
 */

#include "math.h"
#include "mathP.h"

	double __ieee754_fmin(double __x, double __y)
{
	return __y < __x || __builtin_isnan(__x) ? __y : __x;
}
