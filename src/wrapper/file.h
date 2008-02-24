#ifndef file_h
#define file_h

struct access_event;
int handle_file(const struct access_event *event);
int write_files(int argc, char **argv);

#endif
