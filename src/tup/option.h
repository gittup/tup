#ifndef tup_option_h
#define tup_option_h

int tup_option_init(void);
void tup_option_exit(void);
int tup_option_get_int(const char *opt);
int tup_option_get_flag(const char *opt);
const char *tup_option_get_string(const char *opt);
int tup_option_show(void);

#define TUP_OPTIONS_FILE ".tup/options"

#endif
