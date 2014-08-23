/* @(#)w_fmin.c */
/*
 * ====================================================
 * Copyright (C) 2014 by Alec Ari All rights reserved.
 * ====================================================
 */

#include "mathP.h"

	double fmin(double x,double y)
{
	return __ieee754_fmin(x,y);
}
