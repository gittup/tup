#ifndef tup_if_h
#define tup_if_h

struct if_stmt {
	unsigned char ifness;
	unsigned char level;
};

void if_init(struct if_stmt *ifs);
int if_add(struct if_stmt *ifs, int is_true);
int if_else(struct if_stmt *ifs);
int if_endif(struct if_stmt *ifs);
int if_true(struct if_stmt *ifs);
int if_check(struct if_stmt *ifs);

#endif
