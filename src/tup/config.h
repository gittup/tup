#ifndef tup_config_h
#define tup_config_h

#include "tup/buf.h"

#define TUP_CONFIG ".tup/config"

struct tup_config {
	char *build_so;
};

int find_tup_dir(void);
const char *get_tup_top(void);
int get_tup_top_len(void);
const char *get_sub_dir(void);
int get_sub_dir_len(void);
int load_tup_config(struct tup_config *cfg);
int save_tup_config(const struct tup_config *cfg);
void print_tup_config(const struct tup_config *cfg, const char *key);
int tup_config_set_param(struct tup_config *cfg, struct buf *lval,
			  struct buf *rval);

#endif
