/* copied from course example Lecture5/daemon_init.c
 * Daemonizing a server
 * modified from W.R. Stevens example in UNPe1v3, Fig. 13.4, lib/daemon_init.c
 */
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

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
