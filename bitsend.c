/*----------------------------------------------------------------
  BITSEND  touser@tonode [-fn fname ftype] [filepath]

	WARNING: experimental version, not fully debugged!

  For FUNET-NJE 3.0;  (C) Finnish University and Research Network, FUNET
  ----------------------------------------------------------------*/

#define SENDFILECMD "/usr/local/bin/sendfile"


#include "consts.h"
#include "prototypes.h"
#include "clientutils.h"

#define TEMPDIR "/tmp/BitsXXXXXX"
#define TEMPPIECES "/tmp/BitpXXXXXX"
#define PIECESIZE_CHARS 60000
#undef  VERSION
#define VERSION "FUNET-NJE BITSEND Version 1.0"

#define LSIZE  256

static int	splitsrc __(( FILE *infile, char **pieces[],
			      char *piecebase, int piecesize ));
extern char DefaultForm[9];
char LOCAL_NAME   [10];
char BITNET_QUEUE [80];
char LOG_FILE     [80] = "-";

int LogLevel = 1;
FILE *LogFd = NULL;

int
main(argc,argv)
	int argc;
	char **argv;
{
	FILE	*bitctrl;
	char	**pieces = NULL;
	char	**pp;
	char	bitctrlname[LSIZE];
	char	bitpieces[LSIZE];
	char	cmdstr[LSIZE];
	char	From[LSIZE], To[LSIZE], fname[20], ftype[20], pftype[20],
		datestr[26], Forms[9], Tag[137];
	int	i;
	int	fnametypeset = 0;
	int	rc;
	unsigned long seqid;
	time_t now;
	struct tm *timestruct;
	struct stat filestat;

	*From = 0;
	*Tag  = 0;

	if (argc < 2) {
	  fprintf(stderr, "Usage: BITSEND ToUser@ToNode [-fn fname ftype] [-frm FormName] [-tag tagstring] [filepath]\n");
	  exit(1);
	}
	strcpy( fname,"STANDARD" );
	strcpy( ftype,"INPUT"    );
	filestat.st_mtime = time(0); /* Time right now */

	/* Read our name */
	read_configuration();

	strcpy( Forms, DefaultForm);

	++argv;
	strcpy(To, *argv);
	if (strchr(To,'@')==NULL) {
	  fprintf(stderr,"Target address does not contain '@' in it. Something wrong!\n");
	  exit(2);
	}

	++argv;
	argc -= 2;
	while (1) {
	  if (argc >= 2 && strcmp(*argv, "-u") == 0) {
	    ++argv;
	    strncpy(From, *argv++, sizeof From -1);
	    argc -= 2;
	    continue;
	  }
	  if (argc >= 3 && strcmp(*argv, "-fn") == 0) {
	    ++argv;
	    strncpy(fname, *argv++,sizeof fname -1);
	    strncpy(ftype, *argv++,sizeof ftype -1);
	    fnametypeset = 1;
	    argc -= 3;
	    continue;
	  }
	  if (argc >= 2 && strcmp(*argv, "-frm") == 0) {
	    ++argv;
	    strncpy(Forms,*argv++,sizeof Forms -1);
	    argc -= 2;
	    continue;
	  }
	  if (argc >= 2 && strcmp(*argv, "-tag") == 0) {
	    ++argv;
	    strncpy(Tag,*argv++,sizeof Tag -1);
	    argc -= 2;
	    continue;
	  }
	  break;
	}
	if (argc>0) { /* extra file path... ? */
	  if (freopen(*argv,"r",stdin) == NULL) {
	    fprintf(stderr,"Can't open '%s' -file for reading! \n",*argv);
	    exit(3);
	  }
	  stat(*argv,&filestat);  /* Pick up its dates */
	  if (!fnametypeset) {
	    /* XXX: No fname+ftype set yet (aside of default), set here ?? */
	  }
	}
	if (*From == 0 || getuid() > LuserUidLevel) {
	  if (mcuserid(From) == NULL) {
	    fprintf(stderr,"Can't determine who you are. Aborting!\n");
	    exit(2);
	  }
	  strcat(From,"@");
	  strcat(From,LOCAL_NAME);
	}
	

	/* Upcase From and To to prevent problems with RSCS */
	upperstr(From);
	upperstr(To);
	upperstr(fname);
	upperstr(ftype);

	/* Actual splits, etc... */

	strcpy( bitctrlname,TEMPDIR );
	strcpy( bitpieces,TEMPPIECES );
	mktemp(bitpieces);
	if ((i = mkstemp(bitctrlname)) == -1) {
	  fprintf(stderr,"Couldn't open BITCTRL (temporary) file.\n");
	  exit(3);
	}
	bitctrl = fdopen( i,"w" );

#define LRECL 80
#define RECS  1
#define BLKS  1
#define RECFM "V"

	timestruct = gmtime( &filestat.st_mtime );
	sprintf(datestr,"%.2d/%02d/%02d %.2d:%02d:%02d",
		timestruct->tm_mon+1,timestruct->tm_mday,timestruct->tm_year,
		timestruct->tm_hour,timestruct->tm_min,timestruct->tm_sec);

	fprintf(bitctrl,"Control file for use with BITRCV.\n");
	fprintf(bitctrl,"Release:%s SENDER:%s  Protocol:B\n\n",VERSION,From);
	fprintf(bitctrl,"%8s  %8s A0 %4.4s %5d %8d, %8d %s\n",
		fname,ftype,RECFM,LRECL,RECS,BLKS,datestr);

	rc = splitsrc( stdin,&pieces,bitpieces,PIECESIZE_CHARS );
	if (rc == 1) {
	  /* All right, only one piece. Send it! */
	  fclose(bitctrl); /* Superfluous.. */
	  sprintf(cmdstr,"%s %s -fn %s %s -frm %s ",
		  SENDFILECMD,To,fname,ftype,Forms);
	  if (*Tag) {
	    strcat(cmdstr,"-tag ");
	    strcat(cmdstr,Tag);
	    strcat(cmdstr," ");
	  }
	  strcat(cmdstr,*pieces);

	  rc = system(cmdstr);
	  /* Should observe possible errors on SENDFILECMD ! */
	  if (rc) printf("Executing '%s', rc=%d\n",cmdstr,rc);

	} else {
	  /* Multiple pieces - more than 0 - I hope [mea] */

	  pp = pieces;
	  time(&now);
	  seqid = (now % (2<<18)) << 6; /* 64 individual files... */
	  seqid = (seqid | ((now / 86400) % (2<<7)) << 24) % 100000000;

	  while (*pp) {
	    sprintf(pftype,"%08d",seqid++);
	    sprintf(cmdstr,"%s %s -fn %s %s -frm %s ",
		    SENDFILECMD,To,fname,pftype,Forms);
	    if (*Tag) {
	      strcat(cmdstr,"-tag ");
	      strcat(cmdstr,Tag);
	      strcat(cmdstr," ");
	    }
	    strcat(cmdstr,*pp);
	    rc = system(cmdstr);
	    /* Should observe possible errors on SENDFILECMD ! */
	    if (rc) printf("Executing '%s', rc=%d\n",cmdstr,rc);

	    fprintf(bitctrl,"%8.8s  %8.8s A0 %4s %5d %8d, %8d %s\n",
		    fname,pftype,RECFM,LRECL,RECS,BLKS,datestr);
	    ++pp;
	  }
	  fprintf(bitctrl,"\n");
	  fclose(bitctrl);
	  strcpy(pftype,"BITCTRL");
	  sprintf(cmdstr,"%s %s -fn %s %s -frm %s %s",
		  SENDFILECMD,To,fname,pftype,Forms,bitctrlname);
	  rc = system(cmdstr);
	  /* Should observe possible errors on SENDFILECMD ! */
	  if (rc) printf("Executing '%s', rc=%d\n",cmdstr,rc);

	}

	pp = pieces;
	while (*pp)
	  unlink( *pp++ );  /* Delete temp files */
	unlink( bitctrlname ); /* This one also */

	exit(0);
}

static int
splitsrc( infile,pieces,piecebase,piecesize )
     FILE *infile;
     char **pieces[];
     char *piecebase;
     int piecesize;
{
	char	piecename[256];
	char	buf[1024];
	long	bytes = 0;
	long	piecebytes = 0;
	char	*pp;
	int	len;
	FILE	*piece = NULL;
	int	pieceindex = 0;

	strcpy( piecename,piecebase );
	pp = piecename + strlen(piecename);
	*pp++ = 'a';
	*pp   = 'a';
	while (!feof(infile) && !ferror(infile)) {
	  if (piecebytes > piecesize || piece == NULL) {
	    if (piece) fclose(piece);
	    piece = fopen(piecename,"w");
	    if (piece == NULL) {
	      fprintf(stderr,"Can't open file '%s', thus can't write BITSEND pieces on /tmp...\n",piecename);
	      exit(4);
	    }
	    /* store this file name into array... */
	    len = strlen(piecename)+1;
	    if (*pieces == NULL)
	      *pieces = (char**)malloc((pieceindex+2)*sizeof(char*));
	    else
	      *pieces = (char**)realloc(*pieces,(pieceindex+2)*sizeof(char*));
	    (*pieces)[pieceindex] = malloc( len );
	    strcpy( (*pieces)[pieceindex],piecename );
	    (*pieces)[++pieceindex] = NULL;
	    /* file name stored.. */
	    piecebytes = 0;
	    /* Increment file name.. */
	    *pp += 1;
	    if (*pp > 'z') {
	      *(--pp) += 1;
	      *(++pp) = 'a';
	    }
	    
	  }
	  if (fgets( buf,sizeof buf,infile ) == NULL) break; /* EOF... */
	  len = strlen(buf);
	  piecebytes += len;  bytes += len;
	  fputs( buf,piece );
	}
	if (piece) fclose(piece);
	fclose( infile );

	return pieceindex;
}
