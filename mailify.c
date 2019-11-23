/*------------------------------------------------------------------------*
 |		MAILIFY		(special version of RECEIVE)		  |
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
   FIX:
	- Processing for PROFS NOTE, and DEFRT1 -kinds..

   Thoughts about developement:
	- can be invoced with multiple names to default several functions.
	- need to learn about "CARD" format (Cornell ?)
	- need to learn about NETDATA MAIL format (PROFS NOTEs)
 */


#include "prototypes.h"
#include "clientutils.h"
#include "ebcdic.h"
#include "ndlib.h"
#include <utime.h>
#ifdef	USE_ZMAILER
#include <zmailer.h> /* Often in /usr/local/include/.. Use cc's -I accordingly */
#endif

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

char *pname = "<MAILIFY>";
#ifdef	USE_ZMAILER
char *progname = NULL;
#endif

struct ndparam ndparam;	/* For NETDATA parser */

typedef enum bsmtp_states { smtp_helo, mail_from, rcpt_to,
			      data, data_dot, smtp_quit }       bsmtp_states;

typedef struct mailifyparam {
	int	linecnt;
	int	is_smtp;
	bsmtp_states	state;
	FILE	*logfile;
	FILE	*mailout;
	char	*heloaddr;
	char	*mailfrom;
	int	rcptcnt;
	char	**rcptto;
} MailifyParam;
MailifyParam mailifyparam = {
	0,
	0,
	smtp_helo,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	NULL
};

#ifndef	USE_ZMAILER /* When not with Zmailer interface, use sendmail! */

int sendmail_pid = 0;

extern FILE	*sendmail_open __((MailifyParam *mfp));
FILE	*
sendmail_open(mfp)
MailifyParam *mfp;
{
	int pipes[2];
	int i;
	int pid;
	char *path;
	char *argv[300];
	char *envp[8];
	int argc;

	if (pipe(pipes) == -1) return NULL;
	pid = fork();
	if (pid < 0) return NULL;

	if (pid > 0) { /* Parent.. */
	  close(pipes[0]); /* Child reads this */
	  sendmail_pid = pid;
	  signal(SIGPIPE,SIG_IGN);
#ifdef	SIGPOLL
	  signal(SIGPOLL,SIG_IGN);
#endif
	  return fdopen(pipes[1],"w");
	}

	/* Now the child.. */
	close(pipes[1]);
	dup2(pipes[0],1); /* re-define STDOUT */
	close(0);
	for (i = getdtablesize(); i > 1; --i)
	  close(i);

	path = "/usr/lib/sendmail";
	argv[0] = path;
	argv[1] = "-bm";
	argv[2] = (char*)malloc(strlen(mfp->mailfrom)+15);
	if (!argv[2]) _exit(99); /* Malloc failure! */
	if (*mfp->mailfrom)
	  sprintf(argv[2],"-f%s",mfp->mailfrom);
	else
	  strcpy(argv[2],"-fpostmaster");
	for (i = 0; i < mfp->rcptcnt; ++i)
	  argv[i+3] = mfp->rcptto[i];
	argc = 3+mfp->rcptcnt;
	argv[argc] = NULL;

	envp[0] = "IFS= \t\n";
	envp[1] = "SH=/bin/sh";
	envp[2] = NULL;

	execve(path,argv,envp);
	_exit(101);
}

extern void	sendmail_abort __((FILE *ofp, MailifyParam *mfp));
void
sendmail_abort(ofp,mfp)
FILE *ofp;
MailifyParam *mfp;
{
	int status = 0, hispid;
	if (sendmail_pid && ofp) {
	  /* Kill it, and wait for the death status.. */
	  kill(sendmail_pid,SIGINT);
	  while ((hispid = wait(&status)) != sendmail_pid);
	  /* This close can cause a SIGPIPE, but we ignored it.. */
	  fclose(ofp);
	}
	if (sendmail_pid)
	sendmail_pid = 0;
	mfp->mailout = NULL;
}

extern int	sendmail_close __((FILE *ofp, MailifyParam *mfp));
int
sendmail_close(ofp,mfp)
FILE *ofp;
MailifyParam *mfp;
{
	int status = 0, hispid;
	if (ofp) {
	  fclose(ofp);
	}
	if (sendmail_pid)
	  while ((hispid = wait(&status)) != sendmail_pid);
	sendmail_pid = 0;
	mfp->mailout = NULL;
	return status;
}

#endif

/* ---------------------------------------------------------------- */

void mailify_bsmtp(buffer,size,ndp,mfp)
void const *buffer;
int size;
struct ndparam *ndp;
MailifyParam *mfp;
{
	char	key4[6];
	char	*bp = (char *)buffer;

	while (size > 0 && bp[size-1] == ' ') --size;
	if (mfp->state != data_dot) {
	  /* Recognize keywords:  MAIL FROM:.., RCPT TO:.., VERB, TICK, HELO */
	  strncpy(key4,buffer,5);
	  key4[size < 5 ? size : 5] = 0;
	  if (mfp->logfile)
	    fprintf(mfp->logfile,"%s\n",bp);
	  if (strcmp(key4,"HELO ")==0) {
	    mfp->heloaddr = strsave(bp+5);
	    if (mfp->logfile)
	      fprintf(mfp->logfile,
		      "250 How do you do, %s\n",bp+5);
	  } else if (strcmp(key4,"VERB")==0) {
	    if (mfp->logfile)
	      fprintf(mfp->logfile,"250 VERB accepted, but no effect\n");
	  } else if (strcmp(key4,"TICK")==0) {
	    if (mfp->logfile)
	      fprintf(mfp->logfile,"250 TICK accepted, but no effect\n");
	  } else if (strcmp(key4,"MAIL ")==0) {
	    char *p = strchr(bp,':');
	    if (!p || (p && p[1] != '<')) {
	      if (mfp->logfile)
		fprintf(mfp->logfile,"500 MAL-FORMED  `MAIL FROM:' -entry!\n");
	      exit(9);
	    }
	    ++p;++p; /* ":<" */
	    if (mfp->mailfrom) {
	      if (mfp->logfile)
		fprintf(mfp->logfile,"500 Multiple active `MAIL FROM:' -entries!\n");
	      exit(9);
	    }
	    mfp->mailfrom = strsave(p);
	    if (!mfp->mailfrom) {
	      if (mfp->logfile)
		fprintf(mfp->logfile,"500 MALLOC FAILURE!\n");
	      exit(99);
	    }
	    if (!(p = strrchr(mfp->mailfrom,'>'))) {
	      /* Hmm... */
	      if (mfp->logfile)
		fprintf(mfp->logfile,"500 MAL-FORMED  `MAIL FROM:' -entry!\n");
	      exit(9);
	    } else
	      *p = 0; /* Zap the trailing '>' */

	  } else if (strcmp(key4,"RCPT ")==0) {
	    char *p = strchr(bp,':');
	    if (!p || (p && p[1] != '<')) {
	      if (mfp->logfile)
		fprintf(mfp->logfile,"500 MAL-FORMED  `RCPT TO:' -entry!\n");
	      exit(9);
	    }
	    ++p;++p; /* ":<" */
	    if (mfp->rcptto == NULL) {
	      mfp->rcptto = (char**)malloc(sizeof(char*)*(mfp->rcptcnt+1));
	    } else {
	      mfp->rcptto = (char**)realloc(mfp->rcptto,
					    sizeof(char*)*(mfp->rcptcnt+1));
	    }
	    if (!mfp->rcptto) {
	      if (mfp->logfile)
		fprintf(mfp->logfile,"500 MALLOC FAILURE!\n");
	      exit(99);
	    }
	    mfp->rcptto[mfp->rcptcnt] = strsave(p);
	    if (!mfp->rcptto[mfp->rcptcnt]) {
	      if (mfp->logfile)
		fprintf(mfp->logfile,"500 MALLOC FAILURE!\n");
	      exit(99);
	    }
	    if (!(p = strrchr(mfp->rcptto[mfp->rcptcnt],'>'))) {
	      /* Hmm... */
	      if (mfp->logfile)
		fprintf(mfp->logfile,"500 MAL-FORMED  `MAIL FROM:' -entry!\n");
	      exit(9);
	    } else
	      *p = 0; /* Zap the trailing '>' */
	    mfp->rcptcnt += 1;

	  } else if (strcmp(key4,"DATA")==0) {
	    /* XX: Open up the  mailout  channel! */
	    int i;
	    if (mfp->mailfrom == NULL || mfp->rcptcnt == 0) {
	      if (mfp->logfile)
		fprintf(mfp->logfile,"Either MAIL FROM:<>, or RCPT TO:<> addresses are missing!\n");
	      exit(9);
	    }

#ifdef	USE_ZMAILER
	    /* Use Zmailer's mail(3) -library */
	    mfp->mailout = mail_open(MSG_RFC822);
	    if (mfp->mailout == NULL) {
	      if (mfp->logfile)
		fprintf(mfp->logfile,
			" mail_open(MSG_RFC822) failed!, errno=%d\n",errno);
	      exit(9);
	    }
	    if (!ferror(mfp->mailout))
	      fprintf(mfp->mailout,"external\n");
	    if (!ferror(mfp->mailout))
	      fprintf(mfp->mailout,"rcvdfrom %s\n",mfp->heloaddr);
	    if (!ferror(mfp->mailout))
	      fprintf(mfp->mailout,"with BSMTP\n");
	    if (!ferror(mfp->mailout)) {
	      if (*mfp->mailfrom == 0)
		fprintf(mfp->mailout,"channel error\n");
	      else
		fprintf(mfp->mailout,"from <%s>\n",mfp->mailfrom);
	    }
	    for (i = 0; i < mfp->rcptcnt; ++i)
	      if (!ferror(mfp->mailout))
		fprintf(mfp->mailout,"to <%s>\n",mfp->rcptto[i]);
	    if (!ferror(mfp->mailout))
	      fflush(mfp->mailout);
	    if (ferror(mfp->mailout)) {
	      /* can happen! */
	      mail_abort(mfp->mailout);
	      if (mfp->logfile)
		fprintf(mfp->logfile,
			" mail header writing failed!  errno=%d\n",errno);
	      exit(9);
	    }
#else
	    /* `Normal' pipe-open to /usr/lib/sendmail with some suitable args.. */
	    mfp->mailout = sendmail_open(mfp);
#endif
	    mfp->state = data_dot;
	    if (mfp->logfile)
	      fprintf(mfp->logfile,
		      "354 Start mail input; end with <CRLF>.<CRLF>\n");
	  } else if (strcmp(key4,"QUIT")==0) {
	    if (mfp->logfile)
	      fprintf(mfp->logfile,"221 Ok, I quit too.\n");
	  } else {
	    if (mfp->logfile)
	      fprintf(mfp->logfile,"500 Unknown BSMTP command!\n");
	  }
	} else {
	  /* SMTP DATA contents */
	  if (!(size == 1 && bp[0] == '.')) {
	    /* If not the lonely `.' on a line.. */
	    if (size > 1 && bp[0] == '.') {
	      ++bp; --size; /* Drop the first '.' */
	    }
	    if (!ferror(mfp->mailout))
	      fwrite(bp,size,1,mfp->mailout);
	    if (!ferror(mfp->mailout))
	      putc('\n',mfp->mailout);
	    if (ferror(mfp->mailout)) {
#ifdef	USE_ZMAILER
	      mail_abort(mfp->mailout);
#else
	      sendmail_abort(mfp->mailout,mfp);
#endif
	      if (mfp->logfile) {
		fprintf(mfp->logfile,
			" mail body write failed, abort!  errno=%d\n",errno);
		exit(9);
	      }
	    }
	  } else {
	    /* Got the lonely `.'  ( <CRLF> "." <CRLF> ) */
	    int i;
#ifdef	USE_ZMAILER
	    if (mail_close(mfp->mailout) == EOF) {
	      if (mfp->logfile)
		fprintf(mfp->logfile,
			" mail_close() failed!, errno=%d\n",errno);
	      exit(9);
	    }
#else
	    if (sendmail_close(mfp->mailout,mfp) != 0) {
	      /* Submission failed! */
	      exit(9);
	    }
#endif	    
	    mfp->mailout = NULL;
	    for (i = 0; i < mfp->rcptcnt; ++i)
	      free(mfp->rcptto[i]);
	    free(mfp->rcptto);
	    mfp->rcptto = NULL;
	    free(mfp->mailfrom);
	    mfp->mailfrom = NULL;
	    mfp->state = smtp_quit;
	  }
	}
}

/* ---------------------------------------------------------------- */

void mailify_note(buffer,size,ndp,mfp)
void const *buffer;
const int size;
struct ndparam *ndp;
MailifyParam *mfp;
{
	if (mfp->logfile)
	  fprintf(mfp->logfile,"** Got PROFS NOTE, NO CODE TO MAILIFY IT!\n");
	exit(10);
}

/* ---------------------------------------------------------------- */

void mailify_defrt1(buffer,size,ndp,mfp)
void const *buffer;
const int size;
struct ndparam *ndp;
MailifyParam *mfp;
{
	if (mfp->logfile)
	  fprintf(mfp->logfile,"** Got DEFRT1 MAIL, NO CODE TO MAILIFY IT!\n");
	exit(10);
}

/* ---------------------------------------------------------------- */

void mailify(buffer,size,ndp)
void const *buffer;
const int size;
struct ndparam *ndp;
{
	MailifyParam *mfp = (MailifyParam*)ndp->outfile;
	((char*)buffer)[size] = 0;
	/* if (mfp->logfile)
	   fprintf(mfp->logfile,"%d: %s\n",mfp->linecnt,buffer); */
	if (mfp->linecnt == 0) {
	  mfp->is_smtp = 0;
	  if (ndp->profsnote) {
	    /* IBM PROFS NOTE.. */
	  } else {
	    /* Something else, see if it starts with "HELO " ? */
	    if (strncmp(buffer,"HELO ",5)==0) {
	      mfp->is_smtp = 1;
	      mfp->state   = smtp_helo;
	    } else {
	      /* Well, it must be a plain PUNCH a'la DEFRT1.. */
	    }
	  }
	}
	mfp->linecnt += 1; /* Increment the line cnt.. */
	if (mfp->is_smtp)
	  mailify_bsmtp(buffer,size,ndp,mfp);
	else if (ndp->profsnote)
	  mailify_note(buffer,size,ndp,mfp);
	else
	  mailify_defrt1(buffer,size,ndp,mfp);
}


void mailifyclose(ndp)
struct ndparam *ndp;
{
	MailifyParam *mfp = (MailifyParam*)ndp->outfile;

	if (mfp->is_smtp) {
	  /* With SMTP the output closure must have happened earlier! */
	  if (mfp->mailout != NULL) {
#ifdef	USE_ZMAILER
	    mail_abort(mfp->mailout);
#else
	    sendmail_abort(mfp->mailout,mfp);
#endif
	    if (mfp->logfile) {
	      fprintf(mfp->logfile,
		      " mail body write failed, abort!  errno=%d\n",errno);
	      exit(9);
	    }
	    mfp->mailout = NULL;
	  }
	} else {
	  /* PROFS NOTE, and DEFRT1 will run to the end-of-buffer! */
	  int i;
#ifdef	USE_ZMAILER
	  if (mail_close(mfp->mailout) == EOF) {
	    if (mfp->logfile)
	      fprintf(mfp->logfile,
		      " mail_close() failed!, errno=%d\n",errno);
	    exit(9);
	  }
#else
	  if (sendmail_close(mfp->mailout,mfp) != 0) {
	    /* Submission failed! */
	    exit(9);
	  }
#endif	    
	  mfp->mailout = NULL;
	  for (i = 0; i < mfp->rcptcnt; ++i)
	    free(mfp->rcptto[i]);
	  free(mfp->rcptto);
	  mfp->rcptto = NULL;
	  free(mfp->mailfrom);
	  mfp->mailfrom = NULL;
	}
}

/* ---------------------------------------------------------------- */


extern char FileName[], FileExt[];


int	dump_file __(( char const *path, char const *outpath, int binary, const int dumpstyle, const int debugdump ));

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
	ndparam.outfile = (void*)&mailifyparam;
	ndparam.outname = outpath;
	ndparam.defoutname = defaultoutname;
	ndparam.profsnote = 0;
	ndparam.ackwanted = 0;
	ndparam.outproc   = mailify;
	ndparam.closeproc = mailifyclose;
	ndparam.DSNAM[0]  = 0;
	ndparam.ftimet    = 0;
	ndparam.ctimet    = 0;
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

	if (ndparam.outproc)
	  ndparam.closeproc(&ndparam);
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
	     ctime(&ndparam.ftimet)); */
	}

	if (debugdump) {
	  debug_headers("\0\0\0\0\0\0\0\0",8);
	  printf("\n%d records.\n",recs);
	}

	return rc;

}

/* ---------------------------------------------------------------- */

volatile void
bug_check(string)
char const      *string;
{
	fprintf(stderr, "RECEIVE: Bug check: %s\n", string);
        exit(0);
}

/* ---------------------------------------------------------------- */

extern void usage __((void));
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

/* ---------------------------------------------------------------- */

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
#ifdef	USE_ZMAILER
	progname = pname;
#endif

	if (argc < 2) usage();

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
	  } else if (strcmp(*argv,"-l")==0) {
	    ++argv;
	    if (!*argv) usage();
	    mailifyparam.logfile = fopen(*argv,"a");
	    if (!mailifyparam.logfile) {
	      fprintf(stderr,
		      "%s Can't open log-file `%s' for append!\n",pname,*argv);
	      exit(9);
	    }
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
		  fprintf(stderr,"MAILIFY: Can't figure out who I am.\n");
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
	    logger(1,"MAILIFY: Can't figure home dir of `%s' -- PseudoUser ?\n",
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
