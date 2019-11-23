/*   SENDFILE.C	V1.5-mea
 |
 | Copyright (c) 1990,1991,1993 by
 | Finnish University and Research Network, FUNET
 |
 | Calling sequence:  SENDFILE - see usage()...
 |
 | V1.0 [mea]  Modified BMAIL to SENDFILE... More is needed...
 |		(like NETDATA et.al. processings)
 | V1.1 [mea] - 9-May-91
 |		Added somehow working -print, but no -fortran options done...
 | V1.2 [mea] - 5-Jul-91
 |		Plenty of bug fixes. Getting BITNET mail to work.
 | V1.3 [mea] - 18-Sep-93
 |		RSCS TAG, and some cleanups.
 | V1.4 [mea] - 9-Oct-93
 |		Integrating netdata generation code into this module.
 | V1.5 [mea] - 10-Oct-93
 |		Start-name affects operation mode:
 |			punch, print, submit, sendfile
 */


#include "prototypes.h"
#include "clientutils.h"
#include "ndlib.h"
#include <sysexits.h>

#define	LSIZE	256			/* Maximum line length */

extern char DefaultForm[9];
char LOCAL_NAME   [10];
char BITNET_QUEUE [80];
char LOG_FILE     [80] = "-";

int LogLevel = 1;
FILE *LogFd = NULL;

enum opmodes { UnKnown, Punch, Sendfile, Print, Submit };
enum opmodes OpMode = UnKnown;

char *cmdname = "<sendfile>";

/* ================ usage() ================ */

void
usage(explain)
char *explain;
{
  fprintf(stderr, "punch/print/sendfile/submit:\n");
  fprintf(stderr, "Usage: %s ToUser@ToNode [-u origuser] [-class SpoolClass]\n",cmdname);
  fprintf(stderr, "            [-fix|-var]");
  if (OpMode == Sendfile)
    fprintf(stderr, " [-bin]");
  if (OpMode != Submit)
    fprintf(stderr, " [-fn fname ftype] [-form FormName]\n            [-dist Distribution] [-tag RSCS-TAG]");
  if (OpMode == Print || OpMode == Sendfile)
    fprintf(stderr, " [-asa]");
  if (OpMode == Punch)
    fprintf(stderr, " [-rawpun]");
/* Sendfile: -lrecl !! */
  fprintf(stderr, " [filepath]\n");
  if (explain) {
    fputs(explain,stderr);
    fputc('\n',stderr);
  }
  exit(EX_USAGE);
}


/* ================ main() ================ */

int
main(argc, argv)
int	argc;
char	*argv[];
{
	unsigned char	From[LSIZE], To[LSIZE], 
			fname[20], ftype[20], Forms[9], Dist[9], FrUser[9];
	char	FileName[LSIZE], tFileName[LSIZE];
	char	FileClass = 0;
	int	binary = 0;
	long	reccount = 0;
	int	len, i;
	unsigned char	ebcdicline[132+2+3];
	unsigned char	outbuf[150];
	unsigned char	line[1000], *p;
	FILE	*fd, *infile;
	int	rawpun = 0;
	int	asaform = 0;
	int	fnametypeset = 0;
	struct stat stats;
	char	*Tag = NULL;
	char	*AskAck = NULL;
	int	FileSize = 0;
	int	rc;
	long	recpos = 0;
	int	fixedwidth = 0;
	int	lrecl = 0;
	int	recfm = ND_VARY; /* Alternate: ND_FIXED ?? */

#if 0 /* Some temporary debug code.. */
{ char **aa = argv;
  fprintf(stderr,"sendfile: argc=%d, argv=[\"%s\"",argc,*aa++);
  for (; *aa; ++aa) fprintf(stderr,",\"%s\"",*aa);
  fprintf(stderr,"]\n");
}
#endif

	cmdname = strrchr(*argv,'/');
	*To = 0;

	if (cmdname == NULL) cmdname = *argv;
	else ++cmdname;
	if (strcmp(cmdname,"sendfile") == 0) {
	  OpMode = Sendfile;
	} else if (strcmp(cmdname,"sf") == 0) {
	  OpMode = Sendfile;
	} else if (strcmp(cmdname,"print") == 0) {
	  OpMode = Print;
	} else if (strcmp(cmdname,"bitprt") == 0) {
	  OpMode = Print;
	} else if (strcmp(cmdname,"punch") == 0) {
	  OpMode = Punch;
	} else if (strcmp(cmdname,"submit") == 0) {
	  OpMode = Submit;
	} else {
	  OpMode = UnKnown;
	  fprintf(stderr,"`sendfile' invoked with name (%s) that is not proper OpMode for it!\n",cmdname);
	  exit(EX_USAGE);
	}

	*From = 0;
	*Dist = 0;
	memset( ebcdicline, 0, sizeof ebcdicline );

	if (argc < 2) usage(NULL);

	strcpy( fname,"STANDARD" );
	strcpy( ftype,"INPUT"    );

	/* Read our name and the BITnet queue directory */
	read_configuration();
	read_etable();

	strcpy( Forms, DefaultForm);

	++argv;
	--argc;

	/* ================ Options ================ */

	while (*argv) {

	  /*fprintf(stderr,"Checking option: `%s'\n",*argv);*/

	  if (strcmp (*argv,"-rawpun") ==0 ) {

	    if (OpMode != Punch)
	      usage("  Bad usage of `-rawpun'");
	    rawpun = 1;

	  } else if (strcmp(*argv, "-asa") == 0) {

	    if (OpMode != Print && OpMode != Sendfile)
	      usage("  Bad usage of `-asa'");
	    asaform = 1;

	  } else if (strcmp(*argv, "-fix") == 0) {

	    fixedwidth = 1;
	    recfm = ND_FIXED;	/* For the NETDATA format */

	  } else if (strcmp(*argv, "-var") == 0) {

	    fixedwidth = 0;

	  } else if (strcmp(*argv, "-class") == 0) {

	    ++argv;
	    if (!*argv)
	      usage("  -class option requires ONE parameter!");
	    --argc;
	    upperstr(*argv);
	    FileClass = **argv;

	  } else if (strcmp(*argv, "-fn") == 0) {

	    ++argv;
	    if (OpMode == Submit)
	      usage("  -fn is improper option for SUBMIT!");
	    if (!*argv || !*(argv+1))
	      usage("  -fn option requires TWO parameters!");
	    strncpy(fname, *argv,sizeof fname -1);
	    strncpy(ftype, *++argv,sizeof ftype -1);
	    fnametypeset = 1;
	    argc -= 2;

	  } else if (strcmp(*argv, "-form") == 0) {

	    ++argv;
	    if (OpMode == Submit)
	      usage("  -form is improper option for SUBMIT!");
	    if (!*argv)
	      usage("  -form  option requires one parameter!");
	    strncpy(Forms,*argv,sizeof Forms -1);
	    argc -= 1;

	  } else if (strcmp(*argv, "-dist") == 0) {

	    ++argv;
	    if (OpMode == Submit)
	      usage("  -dist is improper option for SUBMIT!");
	    if (!*argv)
	      usage("  -dist  option requires one parameter!");
	    strncpy(Dist,*argv,sizeof Dist -1);
	    argc -= 1;

	  } else if (strcmp(*argv,"-tag")==0) {

	    if (OpMode == Submit)
	      usage("  -tag is improper option for SUBMIT!");
	    if (!*++argv)
	      usage("  -tag  option requires one parameter!");
	    Tag = *argv;
	    argc -= 1;

	  } else if (strcmp(*argv,"-u")==0) {

	    char *s;

	    if (!*++argv)
	      usage("  -u  option requires one parameter!");
	    --argc;
	    strncpy(From,*argv,sizeof(From)-1);
	    From[sizeof(From)-1] = 0;
	    if ((s = strchr(From,'@')) == NULL) {
	      if (strlen(From) > 8)
	    fromaddrerror:
		usage("  -u option defined username components must be at most 8 chars long!");
	      strcat(From,"@");
	      strcat(From,LOCAL_NAME);
	    } else {
	      *s = 0;
	      if (strlen(From) > 8)
		goto fromaddrerror;
	      *s = '@';
	      if (strlen(s+1) > 8)
		goto fromaddrerror;
	    }
	    if (strchr(From,'.') != NULL)
	      usage(" -u -defined source address may not contain '.' chars in it");
	  } else if (strcmp(*argv,"-lrecl")==0) {

	    if (OpMode != Sendfile)
	      usage("  -lrecl  is usable only with  sendfile!");
	    if (!*++argv)
	      usage("  -lrecl  option requires one parameter!");
	    --argc;
	    if ((lrecl = atoi(*argv)) < 10 || lrecl > 65535)
	      usage("  Bad value of  -lrecl  parameter!  Must be within 10 - 65535.");

	  } else if (strcmp(*argv,"-bin") == 0) {

	    if (OpMode != Sendfile)
	      usage("  -bin  is usable only with  sendfile!");

	    binary = 1;
	    if (FileClass == 0)
	      FileClass = 'N';
	    if (lrecl == 0)
	      lrecl = 8192; /* BINARY is wider.. */

	  } else if (**argv != '-' && *To == 0) {
	    char *s;
	    strncpy(To, *argv, sizeof(To)-1);
	    if ((s = strchr(To,'@'))==NULL) {
	      if (strlen(To) > 8) {
	    targetaddrerror:
		usage(" Target address has components of more than 8 chars long");
	      }
	      /* To[8] = 0; */
	      sprintf(To+strlen(To),"@%.8s",LOCAL_NAME);
	    } else {
	      *s = 0;
	      if (strlen(To) > 8)
		goto targetaddrerror;
	      *s = '@';
	      if (strlen(s+1) > 8)
		goto targetaddrerror;
	    }
	    if (strchr(To,'.') != NULL)
	      usage(" Target address may not contain '.' in its components");
	  } else
	    break;

	  ++argv; --argc;
	} /* While options.. */


	if (*To == 0) {
	  usage("   Missing recipient address!");
	}

	if (FileClass == 0)
	  FileClass = 'A';

	infile = stdin;
#if 0 /* Some ARGV processing debug code */
fprintf(stderr," figuring source file,  *argv='%s'\n",*argv);
#endif
	if (*argv) { /* extra file path... ? */
	  if ((infile = fopen(*argv,"r")) == NULL) {
	    fprintf(stderr,"Can't open `%s' -file for reading!\n",*argv);
	    exit(3);
	  }
	  /* fclose(stdin); */
	  /* Don't close it! There are surprises around
	     to wait it..  Freshly open file may disappear.. */

	  if (!fnametypeset) {
	    /* No fname+ftype set yet (aside of default), set here */
	    /* Find base name - after last `/' that is. */
	    char *fna = strrchr(*argv,'/');
	    char *s, *s2;
	    if (!fna)
	      fna = *argv;
	    else
	      ++fna;

	    /* Locate the first `.' in it */
	    s = strchr(fna,'.');
	    s2 = s;
	    while (s2) {
	      /* And turn other `.'s to `-'es.. */
	      s2 = strchr(s2+1,'.');
	      if (s2) *s2 = '-';
	    }
	    /* And chop them to pieces.. */
	    if (s) {
	      *s = 0;
	      ++s;
	    }
	    else
	      s = "";
	    /* fna points to fname part, s to ftype part */
	    strncpy(fname, fna, sizeof fname -1);
	    fname[12] = 0;
	    strncpy(ftype, s, sizeof ftype -1);
	    ftype[12] = 0;
	  }
	  ++argv;
	}
	if (*argv) usage(" extra arguments/unknown options?");
	

	if (getuid() >= LuserUidLevel) {
	  if (*From) {
	    fprintf(stderr," -u option can't be used unless sufficiently priviledged!\nYour UID=%d\n",(int)getuid());
	    exit(9);
	  }
	  *From = 0;
	}
	/* Not yet set, set it ! */
	if (*From == 0) {
	  if (mcuserid(From) == NULL) {
	    fprintf(stderr,"Can't determine who you are. Aborting!\n");
	    exit(2);
	  }
	  strncpy(FrUser,From,8);
	  FrUser[8] = 0;
	  strcat(From,"@");
	  strcat(From,LOCAL_NAME);
	}

/* Upcase From and To to prevent problems with RSCS */

	upperstr(fname);
	upperstr(ftype);
	upperstr(From);
	upperstr(To);
	upperstr(FrUser);

#define CHOP_BITNET(VarName) \
	i = strlen((VarName)); \
	if (i > 8 && strcasecmp((VarName)+i-7,".BITNET") == 0) \
	  (VarName)[i-7] = 0
	CHOP_BITNET(From);
	CHOP_BITNET(To);


	/* Ok, SGID program to send files... */
	setgid(getegid());


	sprintf(tFileName, "%s/.BIT_%05d", BITNET_QUEUE, getpid());
	if (*tFileName == '\0') {
		fprintf(stderr, "Can't create unique filename\n");
		exit(1);
	}

	umask(077); /* THIS IS NOT A PUBLIC FILE! */
	if ((fd = fopen(tFileName, "w")) == NULL) {
		perror(tFileName); exit(1);
	}
	fstat(fileno(fd),&stats);

	
	/* The RSCS envelope for our NJE emulator */

	/* Form up the initial block */
	fprintf(fd, "%-506s\nEND:\n","*");
	fflush(fd);
	fseek(fd,0,0);

	fprintf(fd, "FRM: %-17s\n", From);
	fprintf(fd, "FMT: BINARY\n");
	if (OpMode == Submit)
	  fprintf(fd, "TYP: JOB\n");
	else if (OpMode == Print)
	  fprintf(fd, "TYP: PRINT\n");
	else	/* OpModes  Sendfile, and Punch look similar.. */
	  fprintf(fd, "TYP: PUNCH\n");
	fprintf(fd, "FNM: %-12s\nEXT: %-12s\n",fname,ftype);
	fprintf(fd, "TOA: %-17.17s\nFOR: %-8s\nCLS: %c\n", To,Forms,FileClass);
	fprintf(fd, "FID: 0000\nOID: 0000\n");
	if (*Dist != 0)
	  fprintf(fd, "DIS: %-8s\n", Dist);
	  else
	  fprintf(fd, "DIS: %-8s\n", FrUser);
	if (Tag)
	  fprintf(fd, "TAG: %-136.136s\n",Tag);
	recpos = ftell(fd);
	fprintf(fd, "REC: %8d",1); /* NO \n ! */
	fflush(fd);

	fseek(fd,0,2); /* To the end.. */

	/* The message itself - copy it as-is */

	if (OpMode == Sendfile) {

	  /* ================ Sendfile ================ */

	  struct puncher	PUNCH;

	  PUNCH.buf[0]   = CC_NO_SRCB;
	  PUNCH.buf[1]   = 80;
	  PUNCH.fd       = fd;
	  PUNCH.len      = 0;
	  PUNCH.punchcnt = 0;

	  if (lrecl == 0)
		lrecl = 132;	/* set default lrecl */	

	  if (!do_netdata(infile,&PUNCH,From,To,
			  fname,ftype,lrecl,recfm,
			  binary,AskAck)) {
	    unlink(tFileName);
	    fprintf(stderr," write of bitnet spool file failed!\n");
	    exit(EX_TEMPFAIL);
	  }

	  reccount += PUNCH.punchcnt;

	} else if (OpMode == Punch && rawpun) {

	  while ((len = fread(line,1,80,infile)) != 0) {
	    if (len <80)	/* Pad with zero */
	      memset(line+len,0,80-len);
	    outbuf[0] = 0x80;
	    outbuf[1] =   80;
	    memcpy( outbuf+2,line,80 );
	    if (!Uwrite( fd,outbuf,82 )) {
	      unlink(tFileName);
	      fprintf(stderr,"PUNCH/rawpun: write of bitnet spool file failed!\n");
	      exit(EX_TEMPFAIL);
	    }
	    ++reccount;
	  }

	} else {

	  /* ================ Punch, Print, Submit ================ */

	  int maxlen = 80;
	  int rectype = CC_NO_SRCB;
	  int cc_space = 0;

	  if (OpMode == Print) {
	    maxlen = 132;
	    rectype = CC_MAC_SRCB;
	    cc_space = 1;	/* +1 for carriage control */
	    
	    if (asaform) {	/* PRINT ASA format */	
	      maxlen = 133;
	      rectype = CC_ASA_SRCB;
	      cc_space = 0;	/* +1 for carriage control */
	    }
	  }

	  while (fgets(line, sizeof line, infile) != NULL) {

	    len = strlen(line);

	    p = line + len;
	    if(len>0)		/* Zap the \n! */
	      if (*(p-1)=='\n')
		--len;
	    if (len==0) { *line=' '; len=1; } /* At least 1 byte! */
	    if (len > maxlen) len = maxlen;
	    if (fixedwidth)
	      for (;len < maxlen; ++len)
		line[len] = ' ';

	    ebcdicline[2] = rectype;
	    if (asaform)
	      ebcdicline[3] = maxlen-1;
	    else
	      ebcdicline[3] = maxlen;
	    p = ebcdicline + 4;
	    if (OpMode == Print && !asaform) {
	      /* We know NOTHING about carriage control! */
	      *p++ = 0x09;	/* Until we know, lets put 0x09 */
	    }
	    ASCII_TO_EBCDIC( line,p,len );
/*	    if (OpMode == Print && asaform) {
	      --len;
	    } */
	    if (!Uwrite( fd, ebcdicline+2, 2+cc_space+len )) {
	      unlink(tFileName);
	      fprintf(stderr,"PUNCH/PRINT/SUBMIT: write of bitnet spool file failed!\n");
	      exit(EX_TEMPFAIL);
	    }
	    ++reccount;
	  }
	}
	FileSize = ftell(fd); /* Get its size in bytes */

	fflush(fd);
	fseek(fd, recpos, 0); /* Into the old place */
	fprintf(fd, "REC: %8ld",reccount);

	fclose(fd);

	sprintf(FileName, "%s/BIT%ld", BITNET_QUEUE, (long)stats.st_ino);
	rename(tFileName,FileName);

	/* Queue it to the NJE emulator */
	rc = submit_file(FileName, FileSize);
	if (rc)
	  unlink(FileName);
	exit (rc);
}
