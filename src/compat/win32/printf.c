#include <stdio.h>
#include <stdarg.h>

int __wrap___mingw_vprintf(const char *format, va_list ap);
int __real___mingw_vprintf(const char *format, va_list ap);

int __wrap___mingw_vprintf(const char *format, va_list ap)
{
	int rc;
	rc = __real___mingw_vprintf(format, ap);
	fflush(stdout);
	return rc;
}
