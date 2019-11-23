/*  SITE_CONSTS..H
 |   The Hebrew University of Jerusalem, HUJIVMS.
 |
 | BEFORE COMPILATION: You must tailor this file to your system. The following
 | definitions are available (#define):
 | UNIX - This is compiled on a UNIX system.
 | NETWORK_ORDER - The internal number representation is the same as the
 |                 network's one. On VAXes do not define it.
 		[mea] Symbol is now obsolete!

 | VMS  - This program is compiled on VMS systems.
 | MULTINET - If you are going to use Multinet TcpIp package.
 | EXOS - If you are going to use EXOS TcpIp package.
 | MAX_LINES - How many lines you can define in the line's database.
 | MAX_INFORM - The maximum number (+1) of the users listed in INFORM command.
 | GDBM - If you use GDBM database instead of DBM. (recommended)
 |        (You can have GDBM in Makefile as well.)
 |
 | Parameters that you might want to change (in CONSTS.H):
 | MAX_BUF_SIZE - The maximum buffer size that can be defined for a line.
 | MAX_ERRORS - After consequtive MAX_ERROR errors a line is restarted.
 |
 */

#define	MAX_INFORM	8
#define	ABSMAX_LINES	10	/* Maximum 10 lines at present */

/* BITNET_HOST is a string needed in bmail.c */
/* #define	BITNET_HOST	"HUJIVMS"	*/
/* The  read_config() routine gets this to  LOCAL_NAME string from config file */


/* GMT_OFFSET is 5 character string telling local time offset from GMT */
/* #define	GMT_OFFSET	"+0200"		*/
/* Only needed on VMS version.. */

/*  s_addr  is needed if 's_addr' isn't known entity in  <netinet/in.h>
   file  'struct in_addr' structure.  S_un.S_addr is obsolete old style
   format of arguments... (an union) 					*/
/* That file is included in UNIXes in  consts.h -file.  */
/* #define s_addr S_un.S_addr */

/*  Now: <dirent.h>  vs. <sys/dir.h> vs. ...
    Also 'struct dirent' vs. 'struct direct' vs. possible others...
  Examples:
   #define DIRENTFILE  <sys/dir.h>
   #define DIRENTTYPE  struct direct					*/
/* Note: SystemV r 3.2 compability relies that 'DIRBUF' preproc. symbol isn't
	 found in BSD compatible systems (like Suns) */

#ifdef sun
#define DIRENTFILE  <dirent.h>
#define DIRENTTYPE  struct dirent
#else  /* Well, [mea@nic.funet.fi] tries to get easy with two system environment.. */
#define DIRENTFILE  <dirent.h>
#define DIRENTTYPE  struct dirent
#endif

/* These can be used to override variables othervise defined in  consts.h:
#define CONFIG_FILE "/path/to/hujinje.cf"
#define PID_FILE    "/path/to/hujinje.pid"
*/
