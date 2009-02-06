struct buf {
	char *s;
	int len;
};

int fslurp(int fd, struct buf *b);
