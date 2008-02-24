#ifndef mkdirhier_h
#define mkdirhier_h

/** Make a directory hierarchy to support the given filename. For example,
 * if given a/b/c/foo.txt, this makes sure the directories a/, a/b/, and a/b/c/
 * exist - if not, it creates them.
 *
 * The data pointed to by 'filename' is temporarily modified, but left
 * unchanged upon.
 */
int mkdirhier(char *filename);

#endif
