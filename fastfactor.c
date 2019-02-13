#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* args) {

	// Taken from: https://lemire.me/blog/2013/12/26/fastest-way-to-compute-the-greatest-common-divisor/
	// Because C lib returned an int
	long long gcd_long(long long a, long long b) {
		if (a < 0)
			a = -a;
		if (b < 0)
			b = -b;
		if (b)
			return gcd_long(b, a % b);
		else
			return a;
	}

	long long N = 3423;

	long long xi = 2;
	long long xm = 2;
	for (long long i = 1; N > 1; i++) {
		xi = (xi * xi + 1) % N;
		printf("xi: %lld\n", xi);
		long long s = gcd_long(xi - xm, N);
		if (s > 1 && s < N) {
			printf("%lld", s);
			N /= s;
		}
		// if xi is a power of 2
		if ((i & (i - 1)) == 0)
			xm = xi;
	}

	printf("done\n");
}