/*------------------------------------------------------------------------*
 |		RECEIVE		cat out given file from user queue	  |
 |	For FUNET-NJE by Matti Aarnio  <mea@nic.funet.fi>		  |
 |									  |
 |  Options:								  |
 |	-a		(Translate to ASCII - default, unless CLASS N)	  |
 |	-b		(binary  length(byte)+contents  format)		  |
 |      -d              (debug dump of spool file contents)		  |
 |	-n		(Don't delete upon successfull reception of ND)	  |
 |	-o filepath	(Output file name)				  |
 |	-asa		(print files with 'Fortran style' carriage ctrl)  |
 |	-rawpun		(Plain 80/132 bytes per record fixed size)	  |
 |	-bitraw
 |	-bcat
 |	-u username
 |	filepath	(point to file to search)			  |
 |									  |
 |									  |
 *------------------------------------------------------------------------*/

/*
   Thoughts about developement:
	- can be invoced with multiple names to default several functions.
	- need to learn about "CARD" format (Cornell ?)
	- need to learn about NETDATA MAIL format (PROFS NOTEs)
	- A generic output channel/procedure arg  in place of outfile
	  (to make this be an usable base for writing a single-module
	   mailify)

 */


#include "headers.h"
#include "prototypes.h"
#include "clientutils.h"
#include "ndlib.h"
#include <utime.h>

char LOCAL_NAME   [10];
char BITNET_QUEUE [80];
char LOG_FILE     [80] = "-";

extern char DefaultSpoolDir[80];

int LogLevel = 2;
FILE *LogFd = NULL;

extern	FILE	*dump_header __(( char const *path, const int debugdump, int *binary ));
extern	void	debug_headers();
extern	void	pad_to_size();
extern	void	dump_punchline __((struct ndparam *ndp, unsigned char *buf, int len, int reclen, const int style));
extern	void	dump_printmccline __((struct ndparam *ndp, unsigned char *buf, int len, int reclen, const int style));
extern	void	dump_printasaline __((struct ndparam *ndp, unsigned char SRCB, unsigned char *buf, int len, int reclen, const int style));
extern	char	*Post;
extern	int	dump_file __(( char const *path, char const *outpath, int binary, const int dumpstyle, const int debugdump ));
extern	void	usage __(( void ));

char *pname = "<RECEIVE>";

struct ndparam ndparam;	/* For NETDATA parser */


int
main(argc,argv)
     int argc;
     char *argv[];
{
	int debugdump = 0;
	int dumpstyle = 1;	/* default style: ASCII output */
	int binary = 0;
	int rc = 0;
	int nodelete = 0;
	int rawpun = 0;
	int rawin = 0;
	int bitraw;
	const	char *path;
	char	*outpath = NULL;
	char	HomeDir[256];
	char	WhoAmI[10];
	char	WhoAmIl[10];	/* Lowercase copy.. */
	char	StudyDir[256];
	
	strcpy(LOG_FILE,"-");
	WhoAmI[0] = 0;

	pname = strrchr(*argv,'/');
	if (pname) ++pname;
	else pname = *argv;

	if(argc < 2) usage();

	read_configuration();
	read_etable();

	++argv;
	while (*argv) {
	  if (strcmp(*argv,"-n")==0) {
	    nodelete = 1;
	  } else if (strcmp(*argv,"-a")==0) {
	    binary = -1;
	  } else if (strcmp("-asa",*argv)==0) {
	    if (dumpstyle != 1) usage();
	    dumpstyle = 2;	/* To ascii, show ASA codes */
	  } else if (strcmp(*argv,"-b")==0) {
	    if (dumpstyle != 1) usage();
	    dumpstyle = 0;	/* binary style... */
	    binary = 1;
	  } else if (strcmp(*argv,"-bcat")==0) {
	    binary = 2;
	  } else if (strcmp(*argv,"-bitraw")==0) {
	    rawpun = 1;
	    bitraw = 1;
	  } else if (strcmp("-d",*argv)==0) {
	    debugdump = 1;
	  } else if (strcmp(*argv,"-o")==0) {
	    ++argv;
	    if (!*argv) usage();
	    outpath = *argv;
	  } else if (strcmp(*argv,"-u")==0) {
	    ++argv;
	    if (!*argv) usage();
	    strncpy(WhoAmI,*argv,8);
	  } else if (strcmp(*argv,"-rawin")==0) {
	    rawin = 1;
	  } else if (strcmp(*argv,"-rawpun")==0) {
	    if (dumpstyle != 1) usage();
	    dumpstyle = -1;	/* Binary style - sort of */
	  } else if (**argv == '-')
	    usage();
	  else
	    break;
	  ++argv;
	}
	path = *argv;
	if (path == NULL && rawin)
	  path = "-";
	if (path == NULL)
	  usage();
	if (access(path,R_OK|F_OK) != 0) {
	  /* Well, not a local file, try expand it with
	     default spooldir, etc.  Thus allow using `spoolid'
	     argument as is. */

	  struct passwd *passwdent;
	  char *pathend;

	  if (*WhoAmI == 0) {
	    char *p = getenv("LOGNAME"); /* BSD/SunOS/... alike */
	    WhoAmI[8] = 0;
	    if (!p) {
	      p = getenv("USER"); /* SysV */
	      if (!p) {
		passwdent = getpwuid(getuid());
		if (passwdent)
		  strncpy(WhoAmI,passwdent->pw_name,8);
		else {
		  fprintf(stderr,"RECEIVE: Can't figure out who I am.\n");
		  exit(2);
		}
	      } else
		strncpy(WhoAmI,p,8);
	    } else
	      strncpy(WhoAmI,p,8);
	  }
	  strcpy(WhoAmIl,WhoAmI);
	  upperstr(WhoAmI);

	  strcpy(StudyDir,DefaultSpoolDir);
	  if ((passwdent = getpwnam( WhoAmIl )) != NULL) {
	    strcpy(HomeDir,passwdent->pw_dir);
	  } else {
	    logger(1,"RECEIVE: Can't figure home dir of `%s' -- PseudoUser ?\n",
		   WhoAmI);
	    strcpy(HomeDir,"/");
	  }
	  ExpandHomeDir(DefaultSpoolDir,HomeDir,WhoAmI,StudyDir);

	  pathend = StudyDir+strlen(StudyDir);
	  strcpy(pathend,*argv);

	  if (access(StudyDir,R_OK|F_OK)!=0) {
	    /* Doesn't exist with that name, see if it is numeric.. */
	    int spoolid = atoi(*argv);
	    if (spoolid >= 1 && spoolid <= 9900)
	      sprintf(pathend,"%04d",spoolid);
	    /* Fails, or not, dump_file() will report.. */
	  }

	  path = StudyDir;
	}

	rc = dump_file(path,outpath,binary,dumpstyle,debugdump);

	if (!nodelete && !rc)
	  unlink(path);

	return rc;
}

extern char FileName[], FileExt[];


int
dump_file(path,outpath,binary,dumpstyle,debugdump)
     char const *path;
     char const *outpath;
     int binary;
     const int dumpstyle;
     const int debugdump;
{
	unsigned char	line[1005];
	FILE	*fd;
	int NewSize;
	int recs = 0;
	int linelen;
	int is_netdata = 0;
	int known_format = 0;
	int rc = 0;
	char defaultoutname[33];
	struct utimbuf times;

	/* Setup netdata parser */
	ndparam.outfile = NULL;
	ndparam.outname = outpath;
	ndparam.defoutname = defaultoutname;
	ndparam.profsnote = 0;
	ndparam.ackwanted = 0;
	ndparam.outproc  = NULL;
	ndparam.DSNAM[0] = 0;
	ndparam.ftimet   = 0;
	ndparam.ctimet   = 0;
	ndparam.RecordsCount = 0;

	Post = NULL;

	if (strcmp(path,"-")==0) {
	  fd = stdin;
	  strcpy(FileName,"standard");
	  strcpy(FileExt, "input");
	} else
	  fd = dump_header(path,debugdump,&binary);
	if (!fd) return 3;

	if (binary < 0) binary = 0; /* ASCII xlate */

	sprintf(defaultoutname,"%s.%s",FileName,FileExt);
	lowerstr(defaultoutname);


	while ((NewSize = Uread(line,sizeof line, fd)) != 0) {
	  ++recs;
	  if (debugdump) {
	    debug_headers(line,NewSize);
	    trace(line,NewSize,1);
	  }
	  switch (*line) {

	    case CC_NO_SRCB:
		/* Can also be NETDATA; test if format is not yet know,
		   and if so, test for ND.  If it is ND, proceed with
		   handling it.. */

		linelen = *(line+1);

		if (!known_format) {
		  /* XX: Knows only about NETDATA, how about
		     CORNELL CARD, et.al ?				*/
		  if (memcmp("\311\325\324\331\360\361",line+4,6)==0)
		    is_netdata = 1; /* Detected INMR01 at the begin
				       of a record */
		  known_format = 1;
		}

		if (is_netdata) {
		  pad_to_size(line+2,NewSize-2,linelen,E_SP);
		  if ((rc = parse_netdata(line+2,linelen,
					  &ndparam,
					  binary,debugdump))) {
		    if (rc == -1)
		      rc = 0;	/* EOF, all ok */
		    else
		      printf("error from parse_netdata() rc=%d, recs=%d\n",
			     rc,recs);
		  }
		} else {
		  if (debugdump) break;

		  if (ndparam.outfile == NULL) {
		    if (outpath == NULL)
		      outpath = defaultoutname;
		    if (strcmp("-",outpath)==0) ndparam.outfile = stdout;
		    else if ((ndparam.outfile = fopen(outpath,"w")) == NULL) {
		      fprintf(stderr,"RECEIVE: outpath `%s' isn't openable.  Maybe you should try -o option ?\n",outpath);
		      return 2;
		    }
		    setbuf(ndparam.outfile,NULL);
		  }

		  /* ----- dump punch line raw contents ----- */
		  if (dumpstyle <= 0) { /* Raw style */
		    pad_to_size(line+2,NewSize-2,linelen,E_SP);
		    dump_punchline(&ndparam,line+1,linelen,linelen,dumpstyle);
		  } else	/* ASCII conversion... */
		    dump_punchline(&ndparam,line+1,linelen,NewSize-2,dumpstyle);
		}
		break;

	    case CC_MAC_SRCB:

		if (debugdump) break;
		
		if (ndparam.outfile == NULL) {
		  if (outpath == NULL)
		    outpath = defaultoutname;
		  if (strcmp("-",outpath)==0) ndparam.outfile = stdout;
		  else if ((ndparam.outfile = fopen(outpath,"w")) == NULL) {
		    fprintf(stderr,"RECEIVE: outpath `%s' isn't openable.  Maybe you should try -o option ?\n",outpath);
		    return 2;
		  }
		  setbuf(ndparam.outfile,NULL);
		}

		known_format = 1; /* Won't be NETDATA -- I hope.. */

		linelen = *(line+1);
		if (dumpstyle <= 0) { /* Raw style */
		  pad_to_size(line+2,NewSize-2,linelen,E_SP);
		  dump_printmccline(&ndparam,line+1,linelen,linelen,dumpstyle);
		} else	/* ASCII conversion... */
		  dump_printmccline(&ndparam,line+1,linelen,NewSize-3,dumpstyle);
		break;

	    case CC_ASA_SRCB:
	    case CC_CPDS_SRCB:

		if (debugdump) break;

		if (ndparam.outfile == NULL) {
		  if (outpath == NULL)
		    outpath = defaultoutname;
		  if (strcmp("-",outpath)==0) ndparam.outfile = stdout;
		  else if ((ndparam.outfile = fopen(outpath,"w")) == NULL) {
		    fprintf(stderr,"RECEIVE: outpath `%s' isn't openable.  Maybe you should try -o option ?\n",outpath);
		    return 2;
		  }
		  setbuf(ndparam.outfile,NULL);
		}

		/*  ----- dump print line raw contents ----- */
		known_format = 1; /* Won't be NETDATA -- I hope.. */
		linelen = *(line+1);
		if (dumpstyle <= 0) { /* Raw style */
		  pad_to_size(line+2,NewSize-2,linelen,E_SP);
		  dump_printasaline(&ndparam,*line, line+1,linelen,linelen,
				    dumpstyle);
		} else		/* ASCII conversion...	*/
		  dump_printasaline(&ndparam,*line, line+1,linelen,NewSize-3,
				    dumpstyle);
		break;

	    default:		/* All others		*/
		/* Surprise: Forget it */
		break;
	    }
	}

	/* Add the trailing newline, if need be.. */
	if (dumpstyle == 1 && Post && !ndparam.outproc && ndparam.outfile)
	  fputs(Post,ndparam.outfile);
	Post = NULL;

	if (!ndparam.outproc && ndparam.outfile)
	  fclose(ndparam.outfile);
	fclose(fd);

	if (!ndparam.outproc && ndparam.outfile) {
	  /* Pick the original file date -- from INMCREAT, if present,
	     or from INMFTIME, if present.  If neither is present, don't
	     change times.. */
	  times.actime  = time(NULL);
	  times.modtime = ndparam.ctimet;

	  if (times.modtime == 0 || times.modtime == (time_t) -1)
	    times.modtime = ndparam.ftimet;

	  if (times.modtime != 0 && times.modtime != (time_t) -1)
	    utime(ndparam.outname,&times);

	  /* printf("NETDATA: CTIMES='%s', ctimet=%d, FTIMES=%d, ftimet=%d, time=%s",
	     ndparam.CTIMES,ndparam.ctimet,ndparam.FTIMES,ndparam.ftimet,
	     ctime(&ndparam.ftime)); */
	}

	if (debugdump) {
	  debug_headers("\0\0\0\0\0\0\0\0",8);
	  printf("\n%d records.\n",recs);
	}

	return rc;

}

volatile void
bug_check(string)
char const      *string;
{
	fprintf(stderr, "RECEIVE: Bug check: %s\n", string);
        exit(0);
}


void
usage()
{
	fprintf(stderr,"%s:  [-d] [-b | -rawpun] filename\n",pname);
	fprintf(stderr,"%s:  -a       output converted to ASCII (default).\n",pname);
	fprintf(stderr,"%s:  -asa     output contains ASA carriage control.\n",pname);
	fprintf(stderr,"%s:  -b       output raw (binary) of bitcat style.\n",pname);
	fprintf(stderr,"%s:  -d       debug mode\n",pname);
	fprintf(stderr,"%s:  -n       don't delete the spool file on successfull processing.\n",pname);
	fprintf(stderr,"%s:  -o file  output to named filed.\n",pname);
	fprintf(stderr,"%s:  -rawpun  output raw nominal size record contents. 80 for punch, etc.\n",pname);
	fprintf(stderr,"%s: AND OTHERS -- to be defined/documented!\n",pname);
	exit(1);
}
