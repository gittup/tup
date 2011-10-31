#ifndef tup_progress_h
#define tup_progress_h

struct tup_entry;

void progress_init(void);
void tup_show_message(const char *s);
void tup_main_progress(const char *s);
void start_progress(int total);
void show_progress(struct tup_entry *tent, int is_error);
void show_active(int active);

#endif
