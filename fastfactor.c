#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
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
			// Learned to use sizeof from: https://stackoverflow.com/a/1788048
			write(pipeWriteEnd, &n, sizeof(N));

			// If statement here removes dupes
			long long nDiv = N / n;
			if (nDiv > last)
				write(pipeWriteEnd, &nDiv, sizeof(nDiv));
		}
	}
}

/**
 * Spawn children and have them start factoring and piping the results
 * Terminates children afterwards
 **/
void writeFactorsParallel(long long N, int numChildren, pid_t children[], int pipeWriteEnd) {
	long long Nsqrt = (long long) ceil(sqrt(N));
	long long Npart = Nsqrt / numChildren;

	long long start = 1;
	for (int n = 0; n < numChildren; n++) {
		long long end = start + Npart;
		end = end < Nsqrt ? end : Nsqrt;

		pid_t pid = fork();
		if (pid == 0) {
			writeFactors(N, start, end, pipeWriteEnd);
			exit(0);
		} else if (pid == -1) {
			printf("Could not spawn child number, %d, in writeFactorsParallel(). Exiting...\n", n);
			exit(-1);
		}
		children[n] = pid;

		start = end + 1;
	}
}

void printFactors(long long N, int numChildren) {
	if (N < 0) {
		printf("Got negative number, %lld, in printFactors(). Exiting...\n", N);
		exit(-1);
	}

	int pipeArr[2];
	pipe(pipeArr);

	if (N < 128) {
		numChildren = 1;
	}

	pid_t children[numChildren];
	writeFactorsParallel(N, numChildren, children, pipeArr[1]);

	// Was told to close the end that I'm not using by https://www.tldp.org/LDP/lpg/node11.html
	close(pipeArr[1]);

	for (int i = 0; i < numChildren; i++) wait(&children[i]);

	long long factor;
	printf("%lld: ", N);
	while (read(pipeArr[0], &factor, sizeof(factor))) printf("%lld ", factor);
	printf("\n");
}

int main(int argc, char** argv)
{
	const int NUM_CHILDREN = 8;

	if (argc == 1) {
		long long N;
        int scanResCode = 1;
        for (int i = 0; scanResCode != EOF; i++) {
            scanResCode = scanf("%lld\n", &N);

			if (scanResCode <= 0) {
				printf("Couldn't convert number in main(). Exiting...\n");
				exit(-1);
			}

			printFactors(N, NUM_CHILDREN);
        }
	} else {
        for (int i = 1; i < argc; i++) {
			// Followed this example for strtoll: https://codeforwin.org/2018/01/convert-string-to-long-long-using-strtoll-c.html
			char* endPtr;
			long long N = strtoll(argv[i], &endPtr, 10);
			if (*endPtr) {
				printf("Couldn't convert number, %s, in main(). Exiting...\n", argv[i]);
				exit(-1);
			}

            printFactors(N, NUM_CHILDREN);
        }
    }

}