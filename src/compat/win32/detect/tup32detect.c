#include <windows.h>
#include <stdio.h>

int main(void)
{
	int ll = (int) LoadLibraryA;
	int gpa = (int) GetProcAddress;
	printf("%x-%x\n", ll, gpa);
	ExitProcess(0);
}
