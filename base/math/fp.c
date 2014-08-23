/*
 * Based off of some code by Robert A. Murley and Ali Sazegari from Apple
 * Re-written and trimmmed down by Alec Ari
 */

#include "fp.h"

/* Stuff for insan */

long int __forisnan ( double arg )
{
	register unsigned long int exponent;
	union
	{
		dHexParts hex;
		double dbl;
	} x;

	x.dbl = arg;

	exponent = x.hex.high & 0x7FF00000;
	if ( exponent == 0x7FF00000 )
		{
		if ( ( ( x.hex.high & 0x000FFFFF ) | x.hex.low ) == 0 )
			return (long int) FP_INFINITE;
		else
			return ( x.hex.high & 0x00080000 ) ? FP_QNAN : FP_SNAN;
		}
		else if ( exponent != 0)
			return (long int) FP_NORMAL;
	else {
		if ( arg == 0.0 )
			return (long int) FP_ZERO;
		else
			return (long int) FP_SUBNORMAL;
	}
}

/* Required for undefined reference in rtai_math.ko */

long int __isnan ( double x )
{
	long int class = __forisnan(x);
	return ( ( class == FP_SNAN ) || ( class == FP_QNAN ) );
}
