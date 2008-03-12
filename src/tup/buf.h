#ifndef tup_buf_h
#define tup_buf_h

struct buf {
	char *s;
	int len;
};

/* Returns 0 if the buf is equivalent to the C-string s */
int buf_cmp(const struct buf *b, const char *s);

/* Returns a newly allocated and nul-terminated string equivalent to the
 * buf b.
 */
char *buf_to_string(const struct buf *b);

#endif
