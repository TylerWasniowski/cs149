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
	long long x2i = 2;
	while (N > 1) {
		long long xiPrime = xi * xi + 1;
		long long x2iPrime = (x2i * x2i + 1);
		x2iPrime = x2iPrime * x2iPrime + 1;
		xi = xiPrime % N;
		x2i = x2iPrime % N;
		long long s = gcd_long(xi - x2i, N);
		printf("xi: %lld\n", xi);
		if (s > 1 && s < N) {
			printf("%lld", s);
			N /= s;
		}
	}

	printf("done\n");
}