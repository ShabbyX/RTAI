/*
 * e_fmax.c
 * Written by Alec Ari, feel free to modify, don't care
 */

#include "mathP.h"

	double __ieee754_fmax(double __x, double __y)
{
	return __y < __x || __builtin_isnan(__x) ? __y : __x;
}
