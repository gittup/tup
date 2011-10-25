#ifndef tup_colors_h
#define tup_colors_h

#include <stdio.h>

void color_init(void);
void color_set(FILE *f);
const char *color_type(int type);
const char *color_append_normal(void);
const char *color_append_reverse(void);
const char *color_reverse(void);
const char *color_end(void);
const char *color_final(void);

#endif
