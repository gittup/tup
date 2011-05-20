#ifndef tup_colors_h
#define tup_colors_h

void color_disable(void);
const char *color_type(int type);
const char *color_append_normal(void);
const char *color_append_reverse(void);
const char *color_reverse(void);
const char *color_end(void);
const char *color_final(void);
const char *color_error(void);

#endif
