#ifndef file_h
#define file_h

struct access_event;
void handle_file(const struct access_event *event);
void write_files(void);

#define SERVER_NAME "tup_master"

#endif
