#ifndef fsdep_h
#define fsdep_h

/** Write the dependency relation "file: depends_on" using the filesystem. */
int write_fsdep(const char *file, const char *depends_on);

#endif
