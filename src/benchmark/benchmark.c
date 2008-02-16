#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define NUM_LOOPS 20

static double bench(const char *cmd);

int main(void)
{
	double mark1;
	double mark2;

	system("make clean");
	system("make");
	system("make clean");
	mark1 = bench("make");
	mark2 = bench("make WRAPPER=\"\"");
	printf("Using wrapper:  %f\n", mark1);
	printf("Standard build: %f\n", mark2);
	printf("Factor: %f\n", mark1 / mark2);
	return 0;
}

static double bench(const char *cmd)
{
	int x;
	struct timeval t1;
	struct timeval t2;
	double total = 0.0;

	for(x=0; x<NUM_LOOPS; x++) {
		system("make clean");
		gettimeofday(&t1, NULL);
		system(cmd);
		gettimeofday(&t2, NULL);

		total += t2.tv_sec - t1.tv_sec + (double)(t2.tv_usec - t1.tv_usec) / 1e6;
	}
	return total / (double)NUM_LOOPS;
}
