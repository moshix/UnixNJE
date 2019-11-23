/* Quick DF
   -- Matti Aarnio  <mea@nic.funet.fi> 1993, 1994
   -- Pekka Kytolaakso <netmgr@csc.fi> 1994

   A Quicker way to do the DF -- system provided DF is fast,
   once the initial `sync(2)' -call has finished...
   (At least on SunOS systems, seems to happen elsewere too :-/  )

   This is NOT a `df', as we wrote this for finding out, how
   much free-space there is for the given file/directory on
   an extremely busy system with 30+ GB disks which are in
   high-activity state all the time..  Try `sync()' on that..

   Cnews news-inject is one such program, which actively peeks
   at system free disk space, and does it with `df' :-(

 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sysexits.h>

	/* Now the important detail, what is your system's
	   disk-space query method ? */
#if	defined(sgi) || defined(USG)
	/* SYSV thingies have  <sys/statfs.h> */
#include <sys/statfs.h>
#define SYSV_STATFS
#else
	/* BSD systems have (usually)  <sys/vfs.h> */
#include <sys/vfs.h>
#endif

/* Actually adding facility to analyze existing
   mount-point list -- to make this a full `df'
   seems to be non-portable..

   Some hints from machines on which [mea] has an
   access to... if somebody wants to do it:
     SysVr3:	#include <mnttab.h>
		no apparent library support
     SunOS 4.1:	#include <mntent.h>
		getmntent(3) -library stuff

#define _PATH_MTAB "/etc/mtab"

 */

#ifndef	S_ISDIR
# define S_ISDIR(x) (((x) & S_IFMT)==S_IFDIR)
# define S_ISREG(x) (((x) & S_IFMT)==S_IFREG)
#endif

char *progname = "quickdf";
extern int errno;

main(argc,argv)
int argc;
char *argv[];
{
	struct statfs fsstat;
	struct stat   st;
	int fd;
	int rc;
	int i;

	if (argc < 2) {
	  fprintf(stderr,"%s: missing argument: filesystem mountpoint, or any file/dir in it.\n",progname);
	  exit(EX_USAGE);
	}
	
	for (i = 1; (i < argc) ; i++) {
		fd = open(argv[i],O_RDONLY|O_NOCTTY,0);
		if (fd < 0) {
		  fprintf(stderr,"%s: argument not an openable file!   errno=%d\n",progname,errno);
		  exit(EX_DATAERR);
		}

		if (fstat(fd,&st) < 0) {
		  fprintf(stderr,"%s: can't take  fstat(2) from opened file???  errno=%d\n",progname,errno);
		  exit(EX_OSERR);
		}
		if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode)) {
		  fprintf(stderr,"%s: Must be a regular file, or dir! `.' is just fine!\n",progname);
		  exit(EX_USAGE);
		}
	
	
		/* SysV statfs(2) differs from used BSD semantics.. */
#ifdef SYSV_STATFS
		if (fstatfs(fd,&fsstat,sizeof (struct statfs) , 0) < 0) {
#else
		if (fstatfs(fd,&fsstat) < 0) {
#endif
		  fprintf(stderr,"%s: ???  Could not take  fstatfs() from opened file ??  Not a regular file object ?  errno=%d\n",progname,errno);
		  exit(EX_OSERR);
		}
	
		close(fd); /* We have the data, close the file.. */

		printf("File(system) `%s' size %d kB, used %d kB, avail %d kB %d%%\n",
		       argv[i],
		        (fsstat.f_blocks * fsstat.f_bsize)/1024,
		       ((fsstat.f_blocks - fsstat.f_bfree) * fsstat.f_bsize)/1024,
#ifdef SYSV_STATFS
/* no fsstat.f_bavail on SGI -- or other SysV(r3)'s */
		        (fsstat.f_bfree * fsstat.f_bsize)/1024,
		       (100*fsstat.f_bfree)/fsstat.f_bfree);
#else
		        (fsstat.f_bavail * fsstat.f_bsize)/1024,
		       (100*fsstat.f_bavail)/fsstat.f_bfree);
#endif
	};
	exit (0);
}
