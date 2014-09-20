/*
 * cabs() wrapper for hypot().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include "mathP.h"

double cabs(double __complex__ z)
{
	return hypot(__builtin_creal(z), __builtin_cimag(z));
}
