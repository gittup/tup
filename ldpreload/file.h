/* 'file' is the full filename, rw is 1 for write, 0 for read */
void handle_file(const char *file, int rw, const char *func);

int start_server(void);
void stop_server(void);
int is_server(void);
const char *get_server_name(void);
void set_server_name(const char *name);
