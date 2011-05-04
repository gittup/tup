/* I see you! */
#include <stdio.h>
#include <unistd.h>

#define CC(c) \
	do { \
	pid = fork(); \
	if(pid == 0) {\
		execl("/usr/bin/gcc", "gcc", ipath, "-c", #c ".c", "-o", #c ".o", NULL); \
		perror("execl"); \
		return 1; \
	} \
	wait(pid); \
	} while(0)

#define LINK(arr...) \
	do { \
	pid = fork(); \
	if(pid == 0) { \
		execl("/usr/bin/gcc", "gcc", arr, "-o", "prog", NULL); \
		perror("execl"); \
		return 1; \
	} \
	wait(pid); \
	} while(0)

int main(int argc, char **argv)
{
	pid_t pid;
	int test;
	const char *ipath = "-I.";

	if(argc < 2) {
		fprintf(stderr, "The All-Seeing Eye of Mordor would sooner squish your mind than attempt to read it.\n");
		return 1;
	}
	test = strtol(argv[1], NULL, 0);

	switch(test) {
		case 0: /* 1 c */
		case 1: /* 1 h */
			CC(0);
			LINK("0.o");
			break;
		case 2: /* 10 c */
			CC(0);
			LINK("0.o", "1.o", "2.o", "3.o", "4.o", "5.o", "6.o", "7.o", "8.o", "9.o");
			break;
		case 3: /* 10 h */
			CC(9);
			CC(8);
			CC(7);
			CC(6);
			CC(5);
			CC(4);
			CC(0);
			LINK("0.o", "1.o", "2.o", "3.o", "4.o", "5.o", "6.o", "7.o", "8.o", "9.o");
			break;
		case 4: /* 100 c */
			CC(0);
			LINK("0.o", "1.o", "2.o", "3.o", "4.o", "5.o", "6.o", "7.o", "8.o", "9.o");
			break;
		case 5: /* 100 h */
			ipath = "-I../..";
			chdir("src/marf");
			CC(99);
			CC(98);
			CC(97);
			CC(96);
			CC(95);
			CC(94);
			LINK("90.o", "91.o", "92.o", "93.o", "94.o", "95.o", "96.o", "97.o", "98.o", "99.o");
			ipath = "-I.";
			chdir("../..");
			CC(0);
			LINK("0.o", "1.o", "2.o", "3.o", "4.o", "5.o", "6.o", "7.o", "8.o", "9.o");
			break;
		case 6: /* 1000 c */
			CC(0);
			LINK("0.o", "1.o", "2.o", "3.o", "4.o", "5.o", "6.o", "7.o", "8.o", "9.o");
			break;
		case 7: /* 1000 h */
			ipath = "-I../../..";
			chdir("mozilla/marf/marf");
			CC(999);
			CC(998);
			CC(997);
			CC(996);
			CC(995);
			CC(994);
			LINK("990.o", "991.o", "992.o", "993.o", "994.o", "995.o", "996.o", "997.o", "998.o", "999.o");
			ipath = "-I.";
			chdir("../../..");
			CC(0);
			LINK("0.o", "1.o", "2.o", "3.o", "4.o", "5.o", "6.o", "7.o", "8.o", "9.o");
			break;
		case 8: /* 10000 c */
			CC(0);
			LINK("0.o", "1.o", "2.o", "3.o", "4.o", "5.o", "6.o", "7.o", "8.o", "9.o");
			break;
		case 9: /* 10000 h */
			ipath = "-I../../../..";
			chdir("linux/marf/marf/marf");
			CC(9999);
			CC(9998);
			CC(9997);
			CC(9996);
			CC(9995);
			CC(9994);
			LINK("3740.o", "3741.o", "3742.o", "3743.o", "3744.o", "3745.o", "3746.o", "3747.o", "3748.o", "3749.o", "9990.o", "9991.o", "9992.o", "9993.o", "9994.o", "9995.o", "9996.o", "9997.o", "9998.o", "9999.o");
			ipath = "-I.";
			chdir("../../../..");
			CC(0);
			LINK("0.o", "1.o", "2.o", "3.o", "4.o", "5.o", "6.o", "7.o", "8.o", "9.o");
			break;
		case 10: /* 100000 c */
			CC(0);
			LINK("0.o", "1.o", "2.o", "3.o", "4.o", "5.o", "6.o", "7.o", "8.o", "9.o");
			break;
		case 11: /* 100000 h */
			ipath = "-I../../../../../..";
			chdir("mozilla/usr/marf/marf/marf/marf");
			CC(99999);
			CC(99998);
			CC(99997);
			CC(99996);
			CC(99995);
			CC(99994);
			LINK("99990.o", "99991.o", "99992.o", "99993.o", "99994.o", "99995.o", "99996.o", "99997.o", "99998.o", "99999.o");
			ipath = "-I.";
			chdir("../../../../../..");
			CC(0);
			LINK("0.o", "1.o", "2.o", "3.o", "4.o", "5.o", "6.o", "7.o", "8.o", "9.o");
			break;
	}
	return 0;
}
