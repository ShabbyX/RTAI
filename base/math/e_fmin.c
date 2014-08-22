/*
 * e_fmin.c
 * Written by Alec Ari, feel free to modify, don't care
 */

#include "math.h"
#include "mathP.h"

	double __ieee754_fmin(double __x, double __y)
{
	return __y < __x || __builtin_isnan(__x) ? __y : __x;
}
