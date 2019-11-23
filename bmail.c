/*   BMAIL.C	V1.2 - mea-1.4
 | Copyright (c) 1988,1989,1990 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 | Copyright (c) 1991, 1993 by
 | Finnish University and Research Network, FUNET
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is given without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.
 |
 | Calling sequence:  BMAIL from to LinkName
 | or, for BSMTP: BMAIL from to LinkName -b GateWay OurName
 |
 | V1.1 - 22/3/90 - Upcase From, To and LinkName as lower case might cause
 |        problems to IBMs.
 | V1.2 - 4/4/90 - Correct the Usage note.
 | V1.3 - 10-Sep-93 - Some transmission finesses..
 | V1.4 - 10-Oct-93 - Teached it to transmit NETDATA
 */


#include "prototypes.h"
#include "clientutils.h"
#include "ndlib.h"
#include <sysexits.h>

#ifndef MAILERNAME	/* Name of the user that "SENDS" email */
#define MAILERNAME "MAILER"
#endif

#define	LSIZE	256			/* Maximum line length */

char LOCAL_NAME   [10];
char BITNET_QUEUE [80];
char LOG_FILE     [80] = "-";	/* STDERR as the default */
char mailername   [10];

int LogLevel = 1;
FILE *LogFd = NULL;

unsigned char linehdr[2] = { 0x80, 80 };

void
usage(str)
char *str;
{
  fprintf(stderr, "Usage: bmail [-u origuser] [-v] [-tag RSCS-TAG] [-M mailername]\n             [-nd Gateway-address | -b(smtp) Gateway-address]\n             FromUser@FromNode ToUser@ToNode [ToUser@ToNode...]\n");
  fprintf(stderr, "  File is taken from the standard input\n");
  if (str)
    fprintf(stderr,"ErrMsg: %s\n",str);
  exit(1);
}

int
main(argc, argv)
int	argc;
char	*argv[];
{
	char	ebcdicline[LSIZE]; /* 80 needed, more available.. */
	char	outbuf[85];
	char	*From = NULL, FileName[LSIZE], tFileName[LSIZE],
		*GateWay = NULL, To[LSIZE];
	char	line[1000], *p, *q;
	char	NjeFrom[20];
	FILE	*fd;
	int	Netdata = 0, Bsmtp = 0, ForcedMailer = 0;
	int	FileSize, i, rc;
	struct stat stats;
	char	*Tag = NULL;
	long	reccount = 0;
	long	recpos = 0;
	int	verbose = 0;
	int	head = 1; /* Clear, when past the headers. P Bryant */
	char	*origuser = NULL;
	int	myuid = getuid();

	memset( ebcdicline, 0, 80 );
	strncpy(mailername,MAILERNAME,8);
	mailername[8] = 0;



	if (argc < 3) usage("Too few arguments");

/* Read our name and the BITnet queue */
	read_configuration();
	read_etable();

	++argv;
	while (*argv) {
	  if (strcmp(*argv, "-tag") == 0) {
	    ++argv;
	    if (!*argv) usage("-tag missing mandatory data");
	    Tag = *argv;
	  } else if (strcmp(*argv, "-nd") == 0) {
	    Bsmtp = 1;
	    Netdata = 1;
	    ++argv;
	    if (!*argv) usage("-nd (bsmtp) missing mandatory data");
	    GateWay = *argv;
	    upperstr(GateWay);
	  } else if (strcmp(*argv, "-u") == 0) {
	    ++argv;
	    if (!*argv) usage("-u missing mandatory data");
	    origuser = *argv;
	    upperstr(origuser);
	    if (!(myuid == 0 || myuid == 1)) {
	      /* FIXME: HARDWIRED ROOT AND DAEMON!
	                Daemon is usually the uid which BSD-sendmail
			uses when spawning a child to do transportation.. */
	      usage("-u can be used by 'root' and 'daemon' users only!");
	    }
	  } else if (strcmp(*argv, "-b") == 0 ||
		     strcmp(*argv, "-bsmtp") == 0) {
	    Bsmtp = 1;
	    ++argv;
	    if (!*argv) usage("-b(smtp) missing mandatory data");
	    GateWay = *argv;
	    upperstr(GateWay);
	  } else if (strcmp(*argv,"-M") == 0) {
	    ++argv;
	    if (!*argv) usage("-M missing mandatory data");
	    strncpy(mailername, *argv, 8);
	    mailername[8] = 0;
	  } else if (strcmp(*argv,"-v") == 0) {
	    verbose = 1;
	  } else break;
	  ++argv;
	}
	From  = *argv++;
	if (!From || !*argv) usage("No From, and/or To missing");
	if (!Bsmtp && argv[1]) usage("No BSMTP, and multiple recipients");

/* Upcase linkname, From and To to prevent from problems with RSCS */
	upperstr(From);

	/* If trailed with ".BITNET", chop it.. */
#define CHOP_BITNET(VarName) \
	i = strlen((VarName)); \
	if (i > 8 && memcmp((VarName)+i-7,".BITNET",7) == 0) \
	  (VarName)[i-7] = 0
	CHOP_BITNET(From);

	strncpy(To,*argv,sizeof(To)-1);
	upperstr(To);
	CHOP_BITNET(To);

	/* A SGID program.. */
	setgid(getegid());

	sprintf(tFileName, "%s/.MaiL%05d", BITNET_QUEUE,getpid());
	if (*tFileName == '\0') {
		fprintf(stderr, "Can't create unique filename\n");
		exit(1);
	}

	umask(077);
	if ((fd = fopen(tFileName, "w")) == NULL) {
		perror(tFileName); exit(1);
	}
	if (fstat(fileno(fd),&stats)) {
	  perror("Can't fstat(2) ??"); unlink(tFileName); exit(2);
	}

	fprintf(fd, "%-506s\nEND:\n","*");
	fflush(fd);
	fseek(fd,0,0);

	p = (char *)strchr(From,'@');
	if (!p || p > &From[8]) ForcedMailer = 1;
	if (p && (q = (char *)strchr(p,'.'))) ForcedMailer = 1;

/* The RSCS envelope for our NJE emulator */
	if (!origuser && (ForcedMailer || Bsmtp))
	  origuser = mailername;
	if (Bsmtp || ForcedMailer || origuser)
	  sprintf(NjeFrom, "%.8s@%.8s", origuser, LOCAL_NAME);
	else
	  strncpy(NjeFrom, From, 19);
	NjeFrom[20] = 0;
	fprintf(fd, "FRM: %-17.17s\n", NjeFrom);
	fprintf(fd, "FMT: BINARY\nTYP: MAIL\nEXT: MAIL\nCLS: M\nFOR: QUMAIL\n");
	if (Bsmtp || ForcedMailer)
	  fprintf(fd, "FNM: %-12s\n", mailername);	/* The `username' that sends it */
	else
	  fprintf(fd, "FNM: %-12s\n", mcuserid(0));	/* The username that sends it */
	if(Bsmtp)
		fprintf(fd, "TOA: %-17.17s\n", GateWay);
	else
		fprintf(fd, "TOA: %-17.17s\n", To);
	fprintf(fd, "FID: 0000\nOID: 0000\n");
	if (Tag)
	  fprintf(fd, "TAG: %-136.136s\n",Tag);
	recpos = ftell(fd);
	fprintf(fd, "REC: %8d",1); /* NO \n ! */

	fflush(fd);

	fseek(fd,0,2); /* To the end.. */

	if (Netdata) {

	  /* Netdata mode implies BSMTP, we figure out the lrecl.. */

	  int	c, c1;
	  int	lrecl = 80;	/* Lets assume at least this `little' */
	  int	lencnt;
	  struct puncher PUNCH;

	  /* We need an ASCII temporary file.. */
	  FILE *buffile = tmpfile();
	  
	  /* Prepare a puncher.. */

	  PUNCH.buf[0] = CC_NO_SRCB;
	  PUNCH.buf[1] = 80;
	  PUNCH.fd       = fd;
	  PUNCH.len      = 0;
	  PUNCH.punchcnt = 0;

	  /* -- Prepare BSMTP envelope file -- */

	  sprintf(line,"HELO %s.BITNET\n", LOCAL_NAME);
	  lrecl = strlen(line)-1;
	  fputs(line,buffile);
	  if (verbose)
	    fprintf(buffile,"VERB ON\n");
	  fprintf(buffile,"TICK %d\n",(int)time(NULL));
	  sprintf(line,"MAIL FROM:<%s>\n", From);
	  fputs(line,buffile);
	  lencnt = strlen(line)-1;
	  if (lencnt > lrecl) lrecl = lencnt;
	  while (*argv) {
	    sprintf(line,"RCPT TO:<%s>\n", *argv);
	    fputs(line,buffile);
	    lencnt = strlen(line)-1;
	    if (lencnt > lrecl) lrecl = lencnt;
	    ++argv;
	  }
	  fprintf(buffile,"DATA\n");
	  c1 = 0;

	  /* Copy the file over, find longest line's length.. */
	  while ((c = getc(stdin)) != EOF) {
	    putc(c,buffile);

	    /* If our write returns a failure, exit! */
	    if (ferror(buffile)) {
	      fprintf(stderr,"BMAIL/ND: write of BSMTP temp file failed!\n");
	      unlink(tFileName);
	      exit(EX_TEMPFAIL);
	    }

	    if (lencnt == 0 && c == '.')
	      putc(c,buffile);	/* Duplicate front '.' */
	    if (c == '\n') {
	      if (lencnt > lrecl) lrecl = lencnt;
	      lencnt = -1;
	    }
	    ++lencnt;
	    c1 = c;
	  }
	  if (c1 != '\n')
	    putc('\n',buffile);
	  fprintf(buffile,".\nQUIT\n");
	  fseek(buffile,0,0);	/* rewind() ! */

	  /* -- And output it.. -- */
	  if (lrecl < 79) lrecl = 79;
	  if (!do_netdata(buffile,&PUNCH,NjeFrom,To,
			  mailername,"MAIL",lrecl+1,ND_VARY,0,NULL)) {
	    fprintf(stderr,"BMAIL/ND: write of bitnet spool file failed!\n");
	    unlink(tFileName);
	    exit (EX_TEMPFAIL);
	  }

	  fclose(buffile);

	  reccount += PUNCH.punchcnt;

	} else {

#define eprintf(x,y) { 					\
	  sprintf(ebcdicline,x,y);  ebcdicline[80] = 0;	\
	  i = strlen(ebcdicline);			\
	  memcpy( outbuf+0,linehdr,2 );			\
	  ASCII_TO_EBCDIC( ebcdicline,outbuf+2,i );	\
	  if (!Uwrite( fd,outbuf,2+i )) failure = 1;	\
	  ++reccount;  }

	  int failure = 0;

	  if (Bsmtp) {
	    /* If this is a BSMTP message, add the BSMTP header: */
	    eprintf ("HELO %s.BITNET", LOCAL_NAME);
	    if (verbose)
	      eprintf("VERB %s","ON");
	    eprintf ("TICK %d",(int)time(NULL)); /* Not used actually... */
			/* WARNING: MAY FOLD OVER... NEED MORE CODE HERE... */
	    eprintf ("MAIL FROM:<%s>", From);
	    while (*argv) {
	      eprintf ("RCPT TO:<%s>", *argv);
	      ++argv;
	    }
	    eprintf ("%s","DATA");
	  }


	  /* The message itself - copy it as-is */
	  while (!failure && fgets(line+1, sizeof(line)-1, stdin) != NULL) {

	    /* If it is BSMTP, duplicate the dots at
	       the beginning of a line */

	    int len = strlen(line+1);
	    char *l;

	    p = line + len +1;
	    if (len>0)		/* Zap the \n! */
	      if (*(p-1)=='\n')
		*(--p) = 0;
	    l = line+1;
	    if (--len == 0) head = 0;	/* Blank line indicated end of header */
	    while (!failure) {	/* Fold long input.. */
	      int tempchar;
	      if (Bsmtp && (*l == '.')) {
		/* Duplicate the (sub)line starting `.' */
		--l;
		*l = '.';
		++len;
	      }
	      /* P Bryant 10/1/95 code to sort out folding */
	      p=l+len;
	      if (len > 80) {	/*must fold in header*/
		/* now serch back for separator */
		p=l+80;
		if (head != 0) while (*p != ' ' && p != l ) --p;
		/* if p=l we cannot fold so hope for best */
		if (p == l) p=l+80;
	      }
	      tempchar = *p;
	      *p = 0;		/* terminate record */
	      eprintf("%-80.80s",l);
	      *p = tempchar;
	      len = len -( p - l);
	      if (len <= 0) break;
	      l = p;
	      if (*l != ' ' && head != 0){ *(--l)= ' ';++len;}
	      /*	      eprintf("%-80.80s",l);
			      l   += 80;
			      len -= 80; */
	    }
	  } /* While fgets() -loop */

	  /* If BSMTP - add the .QUIT: */

	  if (!failure && Bsmtp != 0) {
	    eprintf("%s",".");
	    eprintf("%s","QUIT");
	  }
	  if (failure) {
	    fprintf(stderr,"BMAIL/%s: bitnet spool write failed!\n",
		    Bsmtp ? "BSMTP":"PUNCH");
	    unlink(tFileName);
	    exit (EX_TEMPFAIL);
	  }
	} /* End of !Netdata */
	
	FileSize = ftell(fd);	/* Get its size in bytes */

	fseek(fd, recpos, 0);
	fprintf(fd, "REC: %8ld",reccount);

	fclose(fd);

	sprintf(FileName, "%s/MaiL%ld", BITNET_QUEUE,(long)stats.st_ino);
	rename(tFileName,FileName);

	/* Queue it to the NJE emulator */
	
	rc = submit_file(FileName, FileSize);
	if (rc)
	  unlink(FileName);
	exit (rc);
}
