/* @(#)w_fmax.c */
/*
 * ====================================================
 * Copyright (C) 2014 by Alec Ari All rights reserved.
 * ====================================================
 */

#include "math.h"
#include "mathP.h"

	double fmax(double x,double y)
{
	return __ieee754_fmax(x,y);
}
