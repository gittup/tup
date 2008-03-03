/*
   Copyright (C) 2005 Mike Shal

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/** @file
  * Some simple file functions.
  *
  * This file adds the abililty to iterate through a list of files to the
  * utility library.
  */

#include <dirent.h>

/** Used to iterate through a list of files
  */
struct flist {
	const char *filename; /**< The file name, does not include directory */
	int dirfd;            /**< The directory file descriptor */
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
	for((f)->_d=opendir(p),(f)->dirfd=(f)->_d?dirfd((f)->_d):-1;\
		((f)->_d!=0 &&\
		 ((f)->_ent=readdir((f)->_d))!=0 &&\
		 ((f)->filename=(f)->_ent->d_name)!=0) ||\
		((f)->_d!=0 && closedir((f)->_d));)
