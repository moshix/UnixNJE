/* NMR.C    V2.7-mea
 | Copyright (c) 1988,1989,1990 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.

 | UNIX environment specific versions of some routines.
 */

/*
 | Loop over all users in the system.   Look for those that are interactive.
 | from those either list all of them (if UserName points to Null), or look for
 | a  specific username (if UserName points to some string).
 | If a list of users is requested, we try to block 6 usernames in a single
 | line.
 */

#include "consts.h"
#include "prototypes.h"
#include <utmp.h>

#ifndef UTMP_FILE
#define UTMP_FILE  "/etc/utmp"
#endif


void
list_users(Faddress, Taddress, cmd, UserName)
const char	*Faddress, *Taddress;
      char	*UserName;
const char	 cmd;
{
	char	line[LINESIZE];
	int	fd, lines;
	struct utmp	Utmp;
	struct passwd	*pwd;
	struct stat     stats;
	time_t	now;
	char	tty[16];
	char	idle[8];
	int	idles;
	char	*s;
	int	users = 0;

	time(&now);

	if ((fd = open(UTMP_FILE,O_RDONLY,0600)) < 0) {
	  sprintf(line,
		  "Can't open `%s' for reading.  No CPQ USER commands now.",
		  UTMP_FILE);
	  send_nmr(Faddress, Taddress, line, strlen(line),ASCII, CMD_MSG);
	} else {
	  lines = 0;
	  while (read(fd,(void*)&Utmp,sizeof Utmp) == sizeof Utmp) {
	    char uname[9];
#ifdef LOGIN_PROCESS /* POSIX or what ?? */
	    if (Utmp.ut_type != LOGIN_PROCESS &&
		Utmp.ut_type != USER_PROCESS) continue;
#endif
	    if (*Utmp.ut_name == 0) continue; /* Try next */
	    strncpy(uname,Utmp.ut_name,8);
	    uname[8] = 0;
	    pwd = getpwnam(uname);
	    if (pwd == NULL) continue;	/* Hmm ??? */
	    ++users;
	    if (cmd == 'U' && (*UserName == 0 ||
			       strcasecmp(UserName,uname) != 0))
	      continue;		/* It's CPQ U <name> -command */
	    sprintf(tty,"/dev/%s",Utmp.ut_line);
	    stats.st_mode = 0;
	    stat(tty,&stats);
	    ++lines;
	    if (1 == lines) {
	      strcpy(line,"Login      Idle Terminal      Name");
	      send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	    }
	    *idle = 0;
	    idles = now - stats.st_atime;

	    if (get_gone_user(uname,&s) >= 0) {
	      strcpy(idle,"GONE");
	    } else if (idles > 86400) {
	      sprintf(idle,"%dd%dh",idles/86400,(idles % 86400) / 3600);
	    } else if (idles > 3599) {
	      sprintf(idle,"%d:%02d",idles/3600,(idles/60)%60);
	    } else if (idles > 59) {
	      sprintf(idle,"%d",idles/60);
	    }
	    sprintf(line, "%-8s %6s %c%-12.12s %s",
		    uname, idle,
		    (stats.st_mode & 020) == 0 ? '*':' ', /* mesg n ? */
		    Utmp.ut_line, pwd->pw_gecos);
	    send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	  }
	  if (cmd == 'U' && *UserName == 0) {
	    sprintf(line,"CPQ: %4d USERS LOGGED ON",users);
	    send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	  } else if (lines == 0) {
	    if (*UserName == 0)
	      strcpy(line,"No one is logged on.");
	    else
	      sprintf(line, "User %s not logged on.", UserName);
	    send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	  }
	  close (fd);
	}
}

/*
 | Send the  /etc/motd  file to the requestor.
 */
void
send_welcome_message(Faddress, Taddress)
const char	*Faddress, *Taddress;
{
	char	*p, line[LINESIZE];
	FILE	*fd;

	if ((fd = fopen("/etc/motd", "r")) == NULL) {
	  logger(1, "NMR, Can't open `/etc/motd' for CPQ LOG command\n");
	  sprintf(line, "LOG file not available");
	  send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	  return;
	}
	while (fgets(line, sizeof line, fd) != NULL) {
	  if ((p = strchr(line, '\n')) != NULL) *p = '\0';
	  send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	}
	fclose(fd);
}


/*
 | Send current time. (local time zone)
 */
void
send_cpu_time(Faddress, Taddress)
const char	*Faddress, *Taddress;
{
	char	line[LINESIZE];
	struct	tm	*tm, *localtime();
	time_t	clock;

	time(&clock);		/* Get the current time */
	tm = localtime(&clock);

	*line = 0;
	strftime(line,sizeof line,"CPQ: TIME IS %T %Z %A %D",tm);
	if (*line == 0) {
	  strcpy(line,"Can't format date string for CPQ TIME reply!");
	}

	send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
}
