#ifndef tup_fslurp_h
#define tup_fslurp_h

struct buf {
	char *s;
	int len;
};

int fslurp(int fd, struct buf *b);
int fslurp_null(int fd, struct buf *b);

#endif
