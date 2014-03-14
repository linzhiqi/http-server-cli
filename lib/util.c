/* copied from course example Lecture5/daemon_init.c
 * Daemonizing a server
 * modified from W.R. Stevens example in UNPe1v3, Fig. 13.4, lib/daemon_init.c
 */
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h> 
#include <string.h>
#include "logutil.h"

typedef	void	Sigfunc(int);	/* for signal handlers */
#define	MAXFD	64

int daemon_init(const char *pname, int facility)
{
	int	i;
	pid_t	pid;

	/* Create child, terminate parent
	   - shell thinks command has finished, child continues in background
	   - inherits process group ID => not process group leader
	   - => enables setsid() below
	 */
	if ( (pid = fork()) < 0)
		return -1;  /* error on fork */
	else if (pid)
		exit(0);			/* parent terminates */

	/* child 1 continues... */

	/* Create new session
	   - process becomes session leader, process group leader of new group
	   - detaches from controlling terminal (=> no SIGHUP when terminal
	     session closes)
	 */
	if (setsid() < 0)			/* become session leader */
		return -1;

	/* Ignore SIGHUP. When session leader terminates, children will
	   will get SIGHUP (see below)
	   signal() is only portable for SIG_DFL and SIG_IGN. Use sigaction() for user defined signal handler
	 */
	signal(SIGHUP, SIG_IGN);

	/* Create a second-level child, terminate first child
	   - second child is no more session leader. If daemon would open
	     a terminal session, it may become controlling terminal for
	     session leader. Want to avoid that.
	 */
	if ( (pid = fork()) < 0)
		return -1;
	else if (pid)
		exit(0);			/* child 1 terminates */

	/* child 2 continues... */

	/* change to "safe" working directory. If daemon uses a mounted
	   device as WD, it cannot be unmounted.
	 */
	chdir("/");				/* change working directory */

	/* close off file descriptors (including stdin, stdout, stderr)
	 (may have been inherited from parent process) */
	for (i = 0; i < MAXFD; i++)
		close(i);

	/* redirect stdin, stdout, and stderr to /dev/null
	 Now read always returns 0, written buffers are ignored
	 (some third party libraries may try to use these)
	 alternatively, stderr could go to your log file */
	open("/dev/null", O_RDONLY); /* fd 0 == stdin */
	open("/dev/null", O_RDWR); /* fd 1 == stdout */
	open("/dev/null", O_RDWR); /* fd 2 == stderr */

	openlog(pname, LOG_PID, facility);

	return 0;				/* success */
}


int ignoresig(int signo){
    struct sigaction	act, oact;
    act.sa_handler=SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(signo, &act, &oact) < 0){
        perror("ignoresig");
        return -1;
    }
    return 0;
}

int list_file_info(const char * dir_path, char * info, int size){
  DIR           *d;
  struct dirent *dir;
  int len1=0, len2=0;
  char * ptr=info;
  d = opendir(dir_path);
  if (d!=NULL)
  {
    memset(info,0,size);
    while ((dir = readdir(d)) != NULL)
    {
      len1=strlen(info),
      len2=strlen(dir->d_name)+1;
      if((len1+len2)<=(size-1)){
        sprintf(ptr, "%s\n", dir->d_name);
        ptr+=len2;
      }else{
        snprintf(ptr, size-(len1+len2)-1, "%s\n", dir->d_name);
        break;
      }
    }

    closedir(d);
  }else{
    return errno;
  }

  return(0);
}

Sigfunc *
signal(int signo, Sigfunc *func)
{
	struct sigaction	act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (signo == SIGALRM) {
#ifdef	SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;	/* SunOS 4.x */
#endif
	} else {
#ifdef	SA_RESTART
		act.sa_flags |= SA_RESTART;		/* SVR4, 44BSD */
#endif
	}
	if (sigaction(signo, &act, &oact) < 0)
		return(SIG_ERR);
	return(oact.sa_handler);
}
/* end signal */

Sigfunc *
Signal(int signo, Sigfunc *func)	/* for our signal() function */
{
	Sigfunc	*sigfunc;

	if ( (sigfunc = signal(signo, func)) == SIG_ERR)
		log_error("Signal(): signal error. Signal number=%d\n", signo);
	return(sigfunc);
}
