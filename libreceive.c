/*------------------------------------------------------------------------*
 |	libreceive.c							  |
 |									  |
 |	For FUNET-NJE by Matti Aarnio  <mea@nic.funet.fi>		  |
 |									  |
 *------------------------------------------------------------------------*/

/*
   Thoughts about developement:
	- can be invoced with multiple names to default several functions.
	- need to learn about "CARD" format (Cornell ?)
	- need to learn about NETDATA MAIL format (PROFS NOTEs)

 */


#include "headers.h"
#include "prototypes.h"
#include "clientutils.h"
#include "ndlib.h"
#include <utime.h>

/* char LOCAL_NAME   [10];
   char BITNET_QUEUE [80];
   char LOG_FILE     [80] = "-"; */

/* extern char DefaultSpoolDir[80]; */

extern	FILE	*dump_header __(( char const *path, const int debugdump, int *binary ));
extern	int	dump_file __(( char const *path, char const *outpath, int binary, const int dumpstyle, const int debugdump ));
extern	void	usage __(( void ));

#ifndef __STDC__
extern FILE *fopen();
#endif


/* From  370 Reference Summary:

   Pre-write MCCs:    01  0B  13  1B  8B
   Post-write MCCs:       09  11  19
   AFA "equivalent":   + ' '   0   -   1
						*/
#define	CALCULATE_NEXT_CC(c,CarriageControl) { \
	switch(c) {	/* Calculate the next one */ \
	case 0x1:	/* Write with no space */ \
	case 0x3: \
		CarriageControl = E_PLUS; break; \
	case 0x9:	/* Write and 1 space */ \
	case 0xb: \
		CarriageControl = E_SP; break; \
	case 0x11:	/* Double space */ \
	case 0x13: \
		CarriageControl = E_0; break; \
	case 0x19:	/* Triple space */ \
	case 0x1b: \
		CarriageControl = E_MINUS; break; \
	case 0x8b:	/* Jump to next page */ \
	case 0x8d: \
		CarriageControl = E_1; break; \
	default:	/* Use single space */ \
		CarriageControl = E_SP; break; \
	} \
}

void eprintf(fmt,a1,a2,a3,a4,a5,a6,a7,a8,a9)
char *fmt;
long a1,a2,a3,a4,a5,a6,a7,a8,a9;
{
  fprintf(stderr,fmt,a1,a2,a3,a4,a5,a6,a7,a8,a9);
}

void etprintf(fmt,index,timep)
char *fmt;
int index;
u_int32 *timep;
{
  eprintf(fmt,index);
  eprintf("<IBM TIME>\n");
}

void
esprintf(prefix,index,buf,len)
char *prefix, *buf;
int index,len;
{
	char Aline[200];
	int i;

	eprintf(prefix,index);
	if (*buf != 0) {
	  EBCDIC_TO_ASCII(buf,Aline,len);
	  Aline[len] = 0;
	  eprintf("A'%-*.*s'\n",len,len,Aline);
	} else {
	  eprintf("X'");
	  for (i = 0; i < len; ++i)
	    eprintf("%02X",buf[i]);
	  eprintf("'\n");
	}
}

static int I(inc)
int inc;
{
	static int i;
	int j = i;

	i += inc;
	if (inc < 0) { i = 0; }
	return j;
}

void
debug_jobheader(NJH,len)
struct JOB_HEADER *NJH;
int len;
{
	I(-1);
	eprintf ("NETWORK JOB HEADER:  Len=%d\n",len);
	eprintf (" %3d:2  LENGTH   = D'%d'\n",  I(2),ntohs(NJH->LENGTH));
	eprintf (" %3d:1  FLAG     = X'%02X'\n",I(1),NJH->FLAG);
	eprintf (" %3d:1  SEQUENCE = D'%d'\n",  I(1),NJH->SEQUENCE);
	eprintf (" %3d:2  LENGTH_4 = D'%d'\n",  I(2),ntohs(NJH->LENGTH_4));
	eprintf (" %3d:1  ID       = X'%02X'\n",I(1),NJH->ID);
	eprintf (" %3d:1  MODIFIER = X'%02X'\n",I(1),NJH->MODIFIER);
	eprintf (" %3d:2  NJHGJID  = D'%04d'\n",I(2),NJH->NJHGJID);
	eprintf (" %3d:1  NJHGJCLS = A'%c'\n",  I(1),EBCDIC_ASCII[NJH->NJHGJCLS]);
	eprintf (" %3d:1  NJHGMCLS = A'%c'\n",  I(1),EBCDIC_ASCII[NJH->NJHGMCLS]);
	eprintf (" %3d:1  NJHGFLG1 = X'%02X'\n",I(1),NJH->NJHGFLG1);
	eprintf (" %3d:1  NJHGPRIO = D'%d'\n",  I(1),NJH->NJHGPRIO);
	eprintf (" %3d:1  NJHGORGQ = X'%02X'\n",I(1),NJH->NJHGORGQ);
	eprintf (" %3d:1  NJHGJCPY = D'%d'\n",  I(1),NJH->NJHGJCPY);
	eprintf (" %3d:1  NJHGLNCT = D'%d'\n",  I(1),NJH->NJHGLNCT);
	esprintf(" %3d:8  NJHGACCT = ",		I(8),NJH->NJHGACCT,8);
	esprintf(" %3d:8  NJHGJNAM = ",		I(8),NJH->NJHGJNAM,8);
	esprintf(" %3d:8  NJHGUSID = ",		I(8),NJH->NJHGUSID,8);
	esprintf(" %3d:8  NJHGPASS = ",		I(8),NJH->NJHGPASS,8);
	esprintf(" %3d:8  NJHGNPAS = ",		I(8),NJH->NJHGNPAS,8);
	etprintf(" %3d:8  NJHGETS  = ",		I(8),NJH->NJHGETS); /* Time stamp */
	esprintf(" %3d:8  NJHGORGN = ",		I(8),NJH->NJHGORGN,8);
	esprintf(" %3d:8  NJHGORGR = ",		I(8),NJH->NJHGORGR,8);
	esprintf(" %3d:8  NJHGXEQN = ",		I(8),NJH->NJHGXEQN,8);
	esprintf(" %3d:8  NJHGXEQU = ",		I(8),NJH->NJHGXEQU,8);
	esprintf(" %3d:8  NJHGPRTN = ",		I(8),NJH->NJHGPRTN,8);
	esprintf(" %3d:8  NJHGPRTR = ",		I(8),NJH->NJHGPRTR,8);
	esprintf(" %3d:8  NJHGPUNN = ",		I(8),NJH->NJHGPUNN,8);
	esprintf(" %3d:8  NJHGPUNR = ",		I(8),NJH->NJHGPUNR,8);
	esprintf(" %3d:8  NJHGFORM = ",		I(8),NJH->NJHGFORM,8);
	eprintf (" %3d:4  NJHGICRD = D'%d'\n",	I(4),ntohl(NJH->NJHGICRD));
	eprintf (" %3d:4  NJHGETIM = D'%d'\n",	I(4),ntohl(NJH->NJHGETIM));
	eprintf (" %3d:4  NJHGELIN = D'%d'\n",	I(4),ntohl(NJH->NJHGELIN));
	eprintf (" %3d:4  NJHGECRD = D'%d'\n",	I(4),ntohl(NJH->NJHGECRD));
	esprintf(" %3d:20 NJHGPRGN = ",	       I(20),NJH->NJHGPRGN,20);
	esprintf(" %3d:8  NJHGROOM = ",		I(8),NJH->NJHGROOM,8);
	esprintf(" %3d:8  NJHGDEPT = ",		I(8),NJH->NJHGDEPT,8);
	esprintf(" %3d:8  NJHGBLDG = ",		I(8),NJH->NJHGBLDG,8);
	eprintf (" %3d:4  NJHGNREC = D'%d'\n",	I(4),ntohl(NJH->NJHGNREC));
	eprintf (" %3d - End of record\n",I(0));
}

void
debug_datasetheader(DSH,len)
struct DATASET_HEADER *DSH;
int len;
{
	struct DATASET_HEADER_G    *NDH  = &DSH->NDH;
	struct DATASET_HEADER_RSCS *RSCS;
	int NDHsize  = ntohs(NDH->LENGTH_4);
	int RSCSsize, RSCSlim;
	int RSCSpos  = NDHsize+4;

	if (NDHsize != sizeof(struct DATASET_HEADER_G)) {
	  
	}

	eprintf ("DATASET  HEADER:  Len=%d\n",len);
	eprintf ("   0:2  LENGTH   = D'%d'\n",ntohs(DSH->LENGTH));
	eprintf ("   2:1  FLAG     = X'%02X'\n",DSH->FLAG);
	eprintf ("   3:1  SEQUENCE = D'%d'\n",DSH->SEQUENCE);

	for (;;) {
	  I(-1);I(4);
	  eprintf (" 4 - %d: General Section:\n", 4+NDHsize-1);
	  eprintf (" %3d:2  LENGTH_4 = D'%d'\n",  I(2),ntohs(NDH->LENGTH_4));
	  eprintf (" %3d:1  ID       = X'%02X'\n",I(1),NDH->ID);
	  eprintf (" %3d:1  MODIFIER = X'%02X'\n",I(1),NDH->MODIFIER);
	  esprintf(" %3d:8  NDHGNODE = ",         I(8),NDH->NDHGNODE,8);
	  esprintf(" %3d:8  NDHGRMT  = ",         I(8),NDH->NDHGRMT,8);
	  esprintf(" %3d:8  NDHGPROC = ",         I(8),NDH->NDHGPROC,8);
	  esprintf(" %3d:8  NDHGSTEP = ",         I(8),NDH->NDHGSTEP,8);
	  esprintf(" %3d:8  NDHGDD   = ",         I(8),NDH->NDHGDD,8);
	  eprintf (" %3d:2  NDHDSNO  = D'%d'\n",  I(2),ntohs(NDH->NDHDSNO));
	  eprintf (" %3d:1  NDHGCLAS = A'%c'\n",  I(1),EBCDIC_ASCII[NDH->NDHGCLAS]);
	  eprintf (" %3d:4  NDHGNREC = D'%d'\n",  I(4),ntohl(NDH->NDHGNREC));
	  eprintf (" %3d:1  NDHGFLG1 = X'%02X'\n",I(1),NDH->NDHGFLG1);
	  eprintf (" %3d:1  NDHGRCFM = X'%02X'\n",I(1),NDH->NDHGRCFM);
	  eprintf (" %3d:2  NDHGLREC = D'%d'\n",  I(2),ntohs(NDH->NDHGLREC));
	  eprintf (" %3d:1  NDHGDSCT = D'%d'\n",  I(1),NDH->NDHGDSCT);
	  eprintf (" %3d:1  NDHGFCBI = D'%d'\n",  I(1),NDH->NDHGFCBI);
	  eprintf (" %3d:1  NDHGLNCT = D'%d'\n",  I(1),NDH->NDHGLNCT);
	  esprintf(" %3d:8  NDHGFORM = ",         I(8),NDH->NDHGFORM,8);
	  esprintf(" %3d:8  NDHGFCB  = ",         I(8),NDH->NDHGFCB,8);
	  esprintf(" %3d:8  NDHGUCS  = ",         I(8),NDH->NDHGUCS,8);
	  esprintf(" %3d:8  NDHGXWTR = ",         I(8),NDH->NDHGXWTR,8);
	  eprintf (" %3d:1  NDHGFLG2 = X'%02X'\n",I(1),NDH->NDHGFLG2);
	  eprintf (" %3d:1  NDHGUCSO = X'%02X'\n",I(1),NDH->NDHGUCSO);
	  esprintf(" %3d:8  NDHGPMDE = ",         I(8),NDH->NDHGPMDE,8);
	  break;
	} /* end if while.. */

	RSCS = (void*)((char *)NDH + NDHsize);
	RSCSpos = 4+NDHsize;
	if (RSCS->ID != 0x87 /* RSCS section code */) {
	  eprintf (" %d-: Unrecognized section, not RSCS anyway.\n",RSCSpos);
	} else
	  for(;;) {
	    /* We have a RSCS section */
	    I(-1); I(RSCSpos);
	    RSCSsize = ntohs(RSCS->LENGTH_4);
	    RSCSlim  = RSCSpos + RSCSsize;
	    eprintf (" %3d - %3d: RSCS Section:\n",I(0),I(0)+RSCSsize-1);
	    eprintf (" %3d:2  LENGTH_4 = D'%d'\n",  I(2),RSCSsize);
	    eprintf (" %3d:1  ID       = X'%02X'\n",I(1),RSCS->ID);
	    eprintf (" %3d:1  MODIFIER = X'%02X'\n",I(1),RSCS->MODIFIER);
	    eprintf (" %3d:1  NDHVFLG1 = X'%02X'\n",I(1),RSCS->NDHVFLG1);
	    eprintf (" %3d:1  NDHVCLAS = A'%c'\n",  I(1),EBCDIC_ASCII[RSCS->NDHVCLAS]);
	    eprintf (" %3d:1  NDHVIDEV = X'%02X'\n",I(1),RSCS->NDHVIDEV);
	    eprintf (" %3d:1  NDHVPGLE = D'%d'\n",  I(1),RSCS->NDHVPGLE);
	    esprintf(" %3d:8  NDHVDIST = ",         I(8),RSCS->NDHVDIST,8);
	    esprintf(" %3d:12 NDHVFNAM = ",        I(12),RSCS->NDHVFNAM,12);
	    if (I(0) >= RSCSlim) break;
	    esprintf(" %3d:12 NDHVFTYP = ",        I(12),RSCS->NDHVFTYP,12);
	    if (I(0) >= RSCSlim) break;
	    eprintf (" %3d:2  NDHVPRIO = D'%d'\n", I(2),ntohs(RSCS->NDHVPRIO));
	    if (I(0) >= RSCSlim) break;
	    eprintf (" %3d:1  NDHVVRSN = D'%d'\n",  I(1),RSCS->NDHVVRSN);
	    if (I(0) >= RSCSlim) break;
	    eprintf (" %3d:1  NDHVRELN = D'%d'\n",  I(1),RSCS->NDHVRELN);
	    if (I(0) >= RSCSlim) break;
	    esprintf(" %3d:136 NDHVTAGR= ",       I(136),RSCS->NDHVTAGR,136);
	    break;
	  }
}

void
debug_jobtrailer(NJT,len)
struct JOB_TRAILER *NJT;
int len;
{
	eprintf ("NETWORK JOB TRAILER:  Len=%d\n",len);
	eprintf ("   0:2  LENGTH   = D'%d'\n",ntohs(NJT->LENGTH));
	eprintf ("   2:1  FLAG     = X'%02X'\n",NJT->FLAG);
	eprintf ("   3:1  SEQUENCE = D'%d'\n",NJT->SEQUENCE);
	eprintf ("   4:2  LENGTH_4 = D'%d'\n",ntohs(NJT->LENGTH_4));
	eprintf ("   6:1  ID       = X'%02X'\n",NJT->ID);
	eprintf ("   7:1  MODIFIER = X'%02X'\n",NJT->MODIFIER);
	eprintf ("   8:1  NJTGFLG1 = X'%02X'\n",NJT->NJTGFLG1);
	eprintf ("   9:1  NJTGXCLS = A'%c'\n",EBCDIC_ASCII[NJT->NJTGXCLS]);
	etprintf("  12:8  NJTGSTRT = ",NJT->NJTGSTRT);
	etprintf("  20:8  NJTGSTOP = ",NJT->NJTGSTOP);
	eprintf ("  28:4  NJTGACPU = D'%d'\n",ntohl(NJT->NJTGACPU));
	eprintf ("  32:4  NJTGALIN = D'%d'\n",ntohl(NJT->NJTGALIN));
	eprintf ("  36:4  NJTGCARD = D'%d'\n",ntohl(NJT->NJTGCARD));
	eprintf ("  40:4  NJTGEXCP = D'%d'\n",ntohl(NJT->NJTGEXCP));
	eprintf ("  44:1  NJTGIXPR = D'%d'\n",NJT->NJTGIXPR);
	eprintf ("  45:1  NJTGAXPR = D'%d'\n",NJT->NJTGAXPR);
	eprintf ("  46:1  NJTGIOPR = D'%d'\n",NJT->NJTGIOPR);
	eprintf ("  47:1  NJTGAOPR = D'%d'\n",NJT->NJTGAOPR);
}

static char headerbuf[512] = "";
static int  headerlen = 0;
static unsigned char prev_srcb = 0;

void
debug_headers(buf,buflen)
unsigned char const *buf;
const int buflen;
{
	int is_header = (*buf == NJH_SRCB ||
			 *buf == NJT_SRCB ||
			 *buf == DSH_SRCB ||
			 *buf == DSHT_SRCB);

	if (*buf != prev_srcb && headerlen != 0) {
	  if (prev_srcb == NJH_SRCB)
	    debug_jobheader((struct JOB_HEADER*)headerbuf, headerlen);
	  else if (prev_srcb == DSH_SRCB)
	    debug_datasetheader((struct DATASET_HEADER*)headerbuf, headerlen);
	  else if (prev_srcb == NJT_SRCB)
	    debug_jobtrailer((struct JOB_TRAILER*)headerbuf, headerlen);
	  prev_srcb = *buf;
	  headerlen = 0;
	}

	/*
	   For a header defragmenter assume following:
	     "There will never be a case of same header type
	      appearing in consecutive input records"
	 */

	if (is_header) {
	  if (headerlen == 0) {  /* First fragment */
	    memset(headerbuf,0,sizeof(headerbuf));
	    memcpy(headerbuf,buf+1,buflen-1);
	    prev_srcb = *buf;
	    headerlen = buflen-1;
	  } else {
	    memcpy(headerbuf+headerlen,buf+5,buflen-5);
	    headerlen += buflen-5;
	  }
	}
}


char	FileName[20], FileExt[20], From[20], To[20], Class;
char	Form[20], Tag[140];
static	int	TypeCode, FileId,Format;


static void
Type2Str(type,str)
     const int type;
     char *str;
{
	if (type & F_PRINT)
	  strcpy(str,"PRT");
	else if (type & F_JOB)
	  strcpy(str,"JOB");
	else
	  strcpy(str,"PUN");
}


FILE *
dump_header(path,debugdump,binary)
     char const *path;
     const int debugdump;
     int *binary;
{
	int	rc;
	char	TypeStr[20];
	FILE	*fd;
	char	const *SpoolID = strrchr(path,'/');

	if (SpoolID) ++SpoolID;
	else	       SpoolID = path; 

	if (SpoolID) {
	  if (!(SpoolID[0] >= '0' && SpoolID[0] <= '9' &&
		SpoolID[1] >= '0' && SpoolID[1] <= '9' &&
		SpoolID[2] >= '0' && SpoolID[2] <= '9' &&
		SpoolID[3] >= '0' && SpoolID[3] <= '9' &&
		SpoolID[4] == 0))
	    SpoolID = "??";
	}

	if( !(fd = fopen(path,"r"))) return NULL;

	rc = parse_header(fd, From,To,FileName,FileExt,&TypeCode,&Class,
			  Form,&Format,Tag,&FileId);
	Type2Str(TypeCode,TypeStr);

	if (*binary == 0)
	  if (Class == 'N')
	    *binary = 1;

	if (debugdump) {
	  fprintf(stderr,"%-17s %-17s %-8s %-8s %4s  %-8s %s\n",
		 "From:","To:","FName:","FExt:","Type","Form:",
		 "SpoolID");
	  printf("%-17s %-17s %-8.8s %-8.8s %-3s %c %-8.8s %s\n",
		 From,To,FileName,FileExt,TypeStr,Class,Form,SpoolID);
	}

	return fd;
}

void
pad_to_size(line,cursize,wantedsize,padchar)
     unsigned char *line, padchar;
     int cursize, wantedsize;
{
	register unsigned char *p = line + cursize -1;
	register int cnt = wantedsize - cursize;

	if (cnt <= 0) return;
	while( --cnt >= 0 ) {
	  *++p = padchar;
	}
}


char *Post = NULL; /* Post-write things.. Like Final Newline.. */


void
dump_punchline(ndp, buf, reclen, len, style)
     struct ndparam *ndp;
     unsigned char *buf;
     register int len, reclen;
     const int style;
{
	if (style <= 0) {
	  if (reclen != *buf)
	    fprintf(stderr,
		    "<BITCAT: BINARY dump, reclen(%d) != advised length(%d)!>",
		    reclen,*buf);
	  if (style < 0) /* -1 == -rawpun, so drop off the length byte.. */
	    ++buf;
	  else
	    ++len;	/* The `-b' mode, output also the length byte.. */

	  if (!ndp->outproc)
	    fwrite(buf,len,1,ndp->outfile);
	  else {
	    buf[len] = 0;
	    ndp->outproc(buf,len,ndp);
	  }

	} else {	/* ASCII styles.. */

	  /* If we bitcating a MVS batch log output, it will have BOTH
	     the 0xA0 (CC_ASA_SRCB), and 0x80 (CC_NO_SRCB) records in it! */

	  if (!ndp->outproc && Post)
	    fputs(Post,ndp->outfile);
	  Post = NULL;

	  ++buf;	/* Drop off the length byte */

	  /* Chop off the trailing spaces */
	  while (len > 0 && buf[ len-1 ] == E_SP)
	    --len;

	  if (style > 1 && !ndp->outproc)
	    putc(' ',ndp->outfile);	/* HAN: we have requested ASA style output and need a dummy space */

	  EBCDIC_TO_ASCII( buf,buf,len );
	  if (ndp->outproc) {
	    buf[len] = 0;
	    ndp->outproc(buf,len,ndp);
	  } else {
	    fwrite(buf,len,1,ndp->outfile);
	    putc('\n',ndp->outfile);
	  }
	}
	/* fflush(ndp->outfile); */
}

void
dump_printmccline(ndp, buf, reclen, len, style)
     struct ndparam *ndp;
     unsigned char *buf;
     int len, reclen; /* len = content length w/o len byte, nor cc-byte */
     const int style;
{

	unsigned char cc;

	Post = NULL;

	/* From  370 Reference Summary:

	   Pre-write MCCs:    01  0B  13  1B  8B
	   Post-write MCCs:       09  11  19
	   ASA "equivalent":   + ' '   0   -   1
							 */
	if (style <= 0) {
	  if (reclen != *buf)
	    fprintf(stderr,
		    "<BITCAT: BINARY dump, reclen(%d) != advised length(%d)!>",
		    reclen,*buf);

	  if (style < 0) /* -1 == -rawpun, so drop off the length byte.. */
	    ++buf;
	  else
	    ++len;	/* The `-b' mode, output also the length byte.. */

	  if (ndp->outproc) {
	    buf[len] = 0;
	    ndp->outproc(buf,len,ndp);
	  } else
	    fwrite(buf,len,1,ndp->outfile);

	} else {

	  ++buf; /* Skip over the length byte */

	  /* Formatted data with "-asa" -option ?,
	     show the ASA character! */
	  cc = *buf;

	  if (style > 1) {
	    CALCULATE_NEXT_CC(*buf,*buf); /* Convert to ASA */
	    ++len;
	  } else
	    ++buf;

	  /* Ascii dump, chop off trailing spaces */
	  while (len > 0 && buf[ len-1 ] == E_SP)
	     --len;

	  /*printf("CC: X'%02X' ",cc);*/

	  /* If print-mode, but not asking ASA output */
	  if (style == 1 && !ndp->outproc) {
	    /* Print as is.. */
	    switch (cc) {
	      case 0x01:
		  putc('\r',ndp->outfile);
		  break;
	      case 0x1B:	/* 3 newlines */
		  putc('\n',ndp->outfile);
	      case 0x13:	/* 2 newlines */
		  putc('\n',ndp->outfile);
	      case 0x0B:	/* 1 newline */
		  putc('\n',ndp->outfile);
		  break;
	      case 0x8B:
		  putc('\f',ndp->outfile);
		  break;
	      case 0x09:
		  Post = "\n";
		  break;
	      case 0x11:
		  Post = "\n\n";
		  break;
	      case 0x19:
		  Post = "\n\n\n";
		  break;
	      default:
		  putc('\n',ndp->outfile);
		  break;
	    }
	  } else Post = "\n";

	  EBCDIC_TO_ASCII( buf,buf,len );
	  if (ndp->outproc) {
	    buf[len] = 0;
	    ndp->outproc(buf,len,ndp);
	  } else {
	    fwrite(buf,len,1,ndp->outfile);
	    if (Post) { fputs(Post,ndp->outfile); Post = NULL; }
	    else Post = "\n";
	  }
	}
}

void
dump_printasaline(ndp, SRCB, buf, reclen, len, style)
     struct ndparam *ndp;
     unsigned char SRCB, *buf;
     int len, reclen; /* len = content length w/o len byte, nor cc-byte */
     const int style;
{
	unsigned char cc;

	if (style <= 0) {
	  if (reclen != *buf)
	    fprintf(stderr,
		    "<BITCAT: BINARY dump, reclen(%d) != advised length(%d)!>",
		    reclen,*buf);

	  /* XX: Is this sensible ?? PRINTs... */
	  if (style < 0) /* -1 == -rawpun, so drop off the length byte.. */
	    ++buf;
	  else
	    ++len;	/* The `-b' mode, output also the length byte.. */

	  if (ndp->outproc) {
	    buf[len] = 0;
	    ndp->outproc(buf,len,ndp);
	  } else
	    fwrite(buf,len,1,ndp->outfile);
	} else {

	  ++buf;	/* Skip over the length byte */

	  cc = *buf;

	  /* Formatted data with "-asa" -option,
	     show the ASA character! */
	  if (style == 2)
	    ++len;
	  else
	    ++buf;	/* Else skip the ASA char */

	  /* Ascii dump, chop off trailing spaces */
/*	  if (style == 1)   HAN: also in ASA outputs we don't want trailing spaces ... */
	    while (len > 0 && buf[ len-1 ] == E_SP)
	      --len;

	  if (style == 1 && !ndp->outproc)
	    switch (EBCDIC_ASCII[cc]) {
	      case '1':
		  putc('\f',ndp->outfile);
		  break; /* HAN: before this felt through to case '+' - why ? */
	      case '+':
		  putc('\r',ndp->outfile);
		  break;
	      case '-':
		  putc('\n',ndp->outfile);
	      case '0':
		  putc('\n',ndp->outfile);
	      case ' ':
		  putc('\n',ndp->outfile);
		  break;
	      default:
		  break;
	    }
	  Post = "\n";

	  EBCDIC_TO_ASCII( buf,buf,len );
	  if (ndp->outproc) {
	    buf[len] = 0;
	    ndp->outproc(buf,len,ndp);
	  } else {
	    fwrite(buf,len,1,ndp->outfile);
	    if (style > 1)
	      putc('\n',ndp->outfile);
	    /*else Post = "\n";  HAN: is this necessary ? */
	  }
	}
}
