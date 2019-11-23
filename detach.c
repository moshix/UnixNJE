/*
 * Detach a daemon process from whoever/whatever started it.
 * Mostly lifted from an article in the July/August 1987 ;login:,
 * by Dave Lennert (hplabs!hpda!davel). Blame bugs on me.
 */
/*
 |  [mea@nic.funet.fi]  Lifted this from Rayan Zachariassens
 |			ZMailer support library.  Handy.
 */

#include "prototypes.h"
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#if	defined(sun) || defined(BSD) || defined(__svr4__)
#define USE_BSDSETPGRP
#endif



void
detach()
{
	/*
	 * If launched by init (process 1), there's no need to detach.
	 *
	 * Note: this test is unreliable due to an unavoidable race
	 * condition if the process is orphaned.
	 */
	if (getppid() == 1)
		goto out;
	/* Ignore terminal stop signals */
#ifdef	SIGTTOU
	(void) signal(SIGTTOU, SIG_IGN);
#endif	/* SIGTTOU */
#ifdef	SIGTTIN
	(void) signal(SIGTTIN, SIG_IGN);
#endif	/* SIGTTIN */
#ifdef	SIGTSTP
	(void) signal(SIGTSTP, SIG_IGN);
#endif	/* SIGTSTP */
	/*
	 * Allow parent shell to continue.
	 * Ensure the process is not a process group leader.
	 */
	if (fork() != 0)
		exit(0);	/* parent */
	/* child */
	/*
	 * Disassociate from controlling terminal and process group.
	 *
	 * Ensure the process can't reacquire a new controlling terminal.
	 * This is done differently on BSD vs. AT&T:
	 *
	 *	BSD won't assign a new controlling terminal
	 *	because process group is non-zero.
	 *
	 *	AT&T won't assign a new controlling terminal
	 *	because process is not a process group leader.
	 *	(Must not do a subsequent setpgrp()!)
	 */
#ifdef	_POSIX_SOURCE
	(void) setsid();
	{
	  int fd;			/* file descriptor */

	  if ((fd = open("/dev/tty", O_RDWR, 0)) >= 0) {
	    ioctl(fd, TIOCNOTTY, 0);	/* lose controlling terminal */
	    close(fd);
	  }
	}
#else	/* !_POSIX_SOURCE */
#ifdef	USE_BSDSETPGRP
#if	defined(__svr4__)
	setpgrp();
#else	/* not __svr4__ */
	(void) setpgrp(0, getpid());	/* change process group */
	{
	  int fd;			/* file descriptor */

	  if ((fd = open("/dev/tty", O_RDWR, 0)) >= 0) {
	    ioctl(fd, TIOCNOTTY, 0);	/* lose controlling terminal */
	    close(fd);
	  }
	}
#endif /* not __svr4__ */
#else	/* !USE_BSDSETPGRP */
	/* lose controlling terminal and change process group */
	setpgrp();
	signal(SIGHUP, SIG_IGN);	/* immunge from pgrp leader death */
	if (fork() != 0)		/* become non-pgrp-leader */
	  exit(0);	/* first child */
	/* second child */
#endif	/* USE_BSDSETPGRP */
#endif	/* !_POSIX_SOURCE */

out:
	(void) close(0);
	(void) close(1);
	(void) close(2);
	(void) umask(022); /* clear any inherited file mode creation mask */

	/* Clean out our environment from personal contamination */
/*	cleanenv(); */

#if	defined(USE_RLIMIT) || defined(sun) || defined(__svr4__)
	/* In case this place runs with cpu limits, remove them */
	{
	  struct rlimit rl;
	  rl.rlim_cur = RLIM_INFINITY;
	  rl.rlim_max = RLIM_INFINITY;
	  setrlimit(RLIMIT_CPU, &rl);
	}
#endif	/* USE_RLIMIT */
	return;
}


#ifdef	USE_NOFILE
#ifndef	NOFILE
#define	NOFILE 20
#endif	/* NOFILE */

int
getdtablesize()
{
	return NOFILE;
}

#endif	/* USE_NOFILE */
