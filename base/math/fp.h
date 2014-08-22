/*
 * Based off of some code by Ali Sazegari
 * Re-written and trimmed down by Alec Ari
 */

/* Hex representation of a double */
typedef struct
      {
      unsigned long int low;
      unsigned long int high;
      } dHexParts;

typedef union
      {
      unsigned char byties[8];
      double dbl;
      } DblInHex;

enum {
	FP_SNAN		= 0,    /* signaling NaN */
	FP_QNAN		= 1,    /* quiet NaN */
	FP_INFINITE	= 2,    /* + or - infinity */
	FP_ZERO		= 3,    /* + or - zero */
	FP_NORMAL	= 4,    /* normal numbers */
	FP_SUBNORMAL	= 5     /* not normal numbers */
};
