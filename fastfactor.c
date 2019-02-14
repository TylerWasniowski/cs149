#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include <sys/types.h>



/**
 * Writes the factors of N from first and to last (both inclusive) into the provided pipe.
**/
void writeFactors(long long N, long long first, long long last, int pipeWriteEnd) {
	for (long long n = first; n <= last; n++)
	{
		if (N % n == 0)
		{
			write(pipeWriteEnd, &n, 8);

			long long nDiv = N / n;
			if (n != nDiv)
				write(pipeWriteEnd, &nDiv, 8);
		}
	}
}

/**
 * Spawn children and have them start factoring and piping the results
 * Terminates children afterwards
 **/
void computeFactors(int numChildren, long long N, int pipeWriteEnd) {
	long long Nsqrt = (long long) sqrt(N) + 1;
	long long Npart = Nsqrt / numChildren;

	long long start = 2;
	for (int n = 0; n < numChildren; n++) {
		long long end = start + Npart;
		end = end < Nsqrt ? end : Nsqrt;

		pid_t pid = fork();
		if (pid == 0) {
			writeFactors(N, start, end, pipeWriteEnd);
			exit(0);
		} else if (pid == -1) {
			perror("Could not spawn child. Exiting...");
			exit(0);
		}

		start = end + 1;
	}
}

int main(int argc, char* args)
{
	int pipeArr[2];

	pipe(pipeArr);

	long long N = 9222343223213138933;

	computeFactors(8, N, pipeArr[1]);
	// Was told to close the end that I'm not using by https://www.tldp.org/LDP/lpg/node11.html
	close(pipeArr[1]);

	long long factor;
	while (1) {
		read(pipeArr[0], &factor, 8);
		printf("%lld\n", factor);
	}
}