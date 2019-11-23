/* A UTMP reading demo hack.. */

#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <utmp.h>
#include <fcntl.h>

#define UTMP_FILE  "/etc/utmp"
#define LINESIZE 512

static void
list_users(Faddress, Taddress, UserName)
char	*Faddress, *Taddress, *UserName;
{
	char	line[LINESIZE];
	int	fd, rc, lines;
	struct utmp	Utmp;
	struct passwd	*pwd;

	if ((fd = open(UTMP_FILE,O_RDONLY,0600)) < 0) {
	  sprintf(line,"Can't open  `%s'  for reading.  No CPQ USER commands now.",UTMP_FILE);
	} else {
	  lines = 0;
	  while (read(fd,&Utmp,sizeof Utmp) == sizeof Utmp) {
	    if (*Utmp.ut_name == 0) continue; /* Try next */
	    pwd = getpwnam(Utmp.ut_name);
	    if (pwd == NULL) continue;	/* Hmm ??? */
	    ++lines;
	    if (1 == lines) {
	      strcpy(line,"Login     Name");
	      printf("%s\n",line);
	    }
	    sprintf(line,"%-8s  %s",Utmp.ut_name,pwd->pw_gecos);
	    printf("%s\n",line);
	  }
	  close (fd);
	}
}

main()
{
  list_users(0,0,0);
  return 0;
}
