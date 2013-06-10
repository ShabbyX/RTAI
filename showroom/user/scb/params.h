#define BUFSIZE     139
#define SLEEP_TIME  100000

// see: Computing Practices, ACM, vol. 31, n. 10, 1988, pgs 1192-1201.

#define TWOPWR31M1 2147483647  // 2^31 - 1

static inline long next_rand(long rand)
{
	const long a = 16807;
	const long m = TWOPWR31M1;
	const long q = 127773;
	const long r = 2836;

	long lo, hi;

	hi = rand/q;
	lo = rand - hi*q;
	rand = a*lo - r*hi;
	if (rand <= 0) {
		rand += m;
	}
	return rand;
}

#ifdef __KERNEL__
static unsigned long randu(unsigned long range)
{
	static long seed = 783637;
	const long m = TWOPWR31M1;

	seed = next_rand(seed);
	return rtai_imuldiv(seed, range, m);
}
#else
static float randu(float range)
{
	static long seed = 783637;
	const float fm = 1.0/TWOPWR31M1;

	seed = next_rand(seed);
	return fm*seed*range;
}
#endif
