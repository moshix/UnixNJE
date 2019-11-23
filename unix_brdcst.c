/* UNIX_BRDCST.C (formerly UNIX_BROADCAST.C)	V1.1
 | Copyright (c) 1988,1989,1990 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.
 |
 | Sections:  COMM - Communication with user and console.
 |
 | V1.1 - 22/3/90 - When cannot send message return 0 instead of -1 in order
 |        to help the GONE section.
*/

#include "consts.h"
#include "prototypes.h"

#ifndef UTMP_FILE
#define UTMP_FILE    "/etc/utmp"
#endif

extern int alarm_happened;

/*
* Write the message msg to the user, on all ttys he is currently logged
* in.
* Returned value:
* 0 in case of error, number of messages sent otherwise.
* In case of error, errno can be examined.
*/
int
send_user(User, msg)
const char *User;
char *msg;
{
	int cnt = 0, fdutmp;
	int ftty;
	int msgsize;
	char buf[BUFSIZ];
	char user[40];
	int i, m, n = (BUFSIZ / sizeof(struct utmp));
	int bufsiz = n * sizeof(struct utmp);
	char tty[16];
	struct stat stats;

	strncpy( user,User,sizeof(user)-1 );
	user[8] = 0;	/* Sorry, UNIX max... */

	
	msgsize = strlen(msg);

	lowerstr(user);
	despace(user,strlen(user));

	if ((fdutmp = open(UTMP_FILE, O_RDONLY,0600)) <= 0)  {
	  return(0);
	}

	cnt = 0;

	while ((m = read(fdutmp, buf, bufsiz)) > 0)  {
	  m /= sizeof(struct utmp);
	  for (i = 0; i < m; i++)   {
	    struct utmp *utp = &((struct utmp*)buf)[i];
#ifdef LOGIN_PROCESS /* POSIX or what ?? */
	    if (utp->ut_type != LOGIN_PROCESS &&
		utp->ut_type != USER_PROCESS) continue;
#endif
	    if (utp->ut_name[0] == 0)
	      continue;
	    if (strncasecmp(user, utp->ut_name,8) != 0)
	      continue;
	    sprintf(tty, "/dev/%s", utp->ut_line);
	    alarm_happened = 0;
	    alarm(10); /* We are fast ? */
	    if ((ftty = open(tty, O_WRONLY,0600)) < 0)  {
	      alarm(0);
	      continue;		/* Some TTYs accept, some don't */
	    }
	    if (fstat(ftty,&stats)!=0 || (stats.st_mode & 022)==0){
	      close(ftty);	/* mesg -n! */
	      alarm(0);
	      continue;		/* Some TTYs accept, some don't */
	    }
	    if (write(ftty, msg, msgsize) < msgsize)  {
	      alarm(0);
	      close(ftty);
	      continue;		/* Some TTYs accept, some don't */
	    }
	    alarm(0);
	    close(ftty);
	    ++cnt;
	  }
	}
	close(fdutmp);

	if(cnt == 0)
	  cnt = send_gone(user, msg);
	return(cnt);
}
