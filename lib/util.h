#ifndef NWPROG_LINZHIQI_UTIL_H
#define NWPROG_LINZHIQI_UTIL_H
typedef	void	Sigfunc(int);	/* for signal handlers */

int daemon_init(const char *pname, int facility);
int ignoresig(int signo);
int list_file_info(const char * dir_path, char * info, int size);
Sigfunc *Signal(int signo, Sigfunc *func);
double getTimeElapsed(struct timeval end, struct timeval start);
int file_exist(const char *filename);
int file_size(const char *filename);
#endif
