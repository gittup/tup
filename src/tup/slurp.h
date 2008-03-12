#ifndef tup_slurp_h
#define tup_slurp_h

#include "buf.h"

/* Does an fslurp after opening the file */
int slurp(const char *filename, struct buf *b);

/* Malloc an approriate sized buffer to hold the entire contents of the file
 * referenced by fd, and read in the whole file into the buffer. If successful,
 * the memory pointed to by b->s should be freed by the caller. Note that
 * unless the file is nul-terminated, don't expect b->s to be.
 */
int fslurp(int fd, struct buf *b);

#endif
