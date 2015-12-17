/* tmpfile() on Windows will occasionally fail with "Permission denied" because
 * it seems not to be implemented properly. This is a work-around that creates
 * a temporary file in the .tup/tmp directory with a sequential ID. It maintains
 * the delete-on-close semantics of tmpfile() by setting the appropriate flag in
 * CreateFile().
 */
#include <stdio.h>
#include "tup/config.h"
#include "compat/dir_mutex.h"

FILE *__wrap_tmpfile(void);

FILE *__wrap_tmpfile(void)
{
	static int num = 0;
	int fd;
	char filename[64];
	wchar_t wfilename[64];
	FILE *f = NULL;
	HANDLE h;

	dir_mutex_lock(tup_top_fd());

	snprintf(filename, sizeof(filename), ".tup/tmp/tmpfile-%i", num);
	filename[sizeof(filename)-1] = 0;
	num++;

	/* Need to use CreateFile to be able to set it delete-on-close */
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, PATH_MAX);
	h = CreateFile(wfilename, GENERIC_WRITE | GENERIC_READ, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if(h == INVALID_HANDLE_VALUE)
		goto err_out;

	/* Convert from HANDLE to FILE* */
	fd = _open_osfhandle((intptr_t)h, 0);
	if(fd < 0)
		goto err_out;
	f = fdopen(fd, "w+");
	if(!f) {
		if(!close(fd)) {
			perror("close(fd) in tmpfile()");
			goto err_out;
		}
	}
err_out:
	dir_mutex_unlock();

	return f;
}
