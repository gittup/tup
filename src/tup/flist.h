/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2005-2018  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef tup_flist_h
#define tup_flist_h

/** @file
  * Some simple file functions.
  *
  * This file adds the abililty to iterate through a list of files to the
  * utility library.
  */

#ifdef _WIN32

#include <windows.h>
struct flist {
	char filename[PATH_MAX];
	HANDLE *_h;
	WIN32_FIND_DATA _dat;
};

/* NOTE: Path is ignored on Windows, we just assume "*" */
/* NOTE: This skips the first file "." */
#define flist_foreach(f, p) \
	for((f)->_h=FindFirstFile(L"*", &(f)->_dat);\
		((f)->_h!=0 &&\
		 (FindNextFile((f)->_h, &(f)->_dat)) &&\
		 WideCharToMultiByte(CP_UTF8, 0, (f)->_dat.cFileName, -1, (f)->filename, PATH_MAX, NULL, NULL)) ||\
		((f)->_h!=0 && FindClose((f)->_h) && 0);)

#define FLIST_INITIALIZER {"", NULL, {0}}

#else

#include <dirent.h>

/** Used to iterate through a list of files
  */
struct flist {
	const char *filename; /**< The file name, does not include directory */
	struct dirent *_ent;  /**< Internal - struct dirent * */
	DIR *_d;              /**< Internal - DIR * */
};

/** Iterates through all the files in path @a p.
  * @param f struct flist *: The struct flist to put names into
  * @param p const char *: The path to grab a list of files from
  *
  * @par Example:
  * @code
  * #include "flist.h"
  *
  * struct flist f;
  * flist_foreach(&f, "dir") {
  * 	printf("File %s is in dir\n", f.filename);
  * }
  * @endcode
  */
#define flist_foreach(f, p) \
	for((f)->_d=opendir(p);\
		((f)->_d!=0 &&\
		 ((f)->_ent=readdir((f)->_d))!=0 &&\
		 ((f)->filename=(f)->_ent->d_name)!=0) ||\
		((f)->_d!=0 && closedir((f)->_d) && 0);)

#define FLIST_INITIALIZER {0, 0, 0}

#endif

#endif
