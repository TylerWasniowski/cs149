#include <stdio.h>
#include <math.h>
#include <sys\timeb.h>

int main(int argc, char* args) {
	/*
		Used https://cboard.cprogramming.com/c-programming/171428-measuring-time-function-execution-milliseconds-linux-environment.html
		to figure out how to get system time
	*/
	time_t start, stop;

	time(&start);

	long long N = 9222343223213138933;
	long long Nsqrt = (long long) sqrt(N);
	for (long long n = 2; n <= Nsqrt; n++) {
		if (N % n == 0) {
			printf("%lld\n", n);
			printf("%lld\n", N / n);
		}
	}
	
	time(&stop);

	printf("done in %is\n", stop - start);
}