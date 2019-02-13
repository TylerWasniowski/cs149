#include <stdio.h>
#include <math.h>

int main(int argc, char* args) {

	// // Taken from: https://lemire.me/blog/2013/12/26/fastest-way-to-compute-the-greatest-common-divisor/
	// // Because C lib returned an int
	// long long gcd_long(long long a, long long b) {
	// 	if (a < 0)
	// 		a = -a;
	// 	if (b < 0)
	// 		b = -b;
	// 	if (b)
	// 		return gcd_long(b, a % b);
	// 	else
	// 		return a;
	// }

	long long N = 234121324120;
	long long Nsqrt = (long long) sqrt(N);
	for (long long n = 2; n <= Nsqrt; n++) {
		if (N % n == 0)
			printf("%lld\n", n);		
	}

	printf("done\n");
}