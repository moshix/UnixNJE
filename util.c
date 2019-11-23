/* UTIL.C    V2.0-mea
 | Copyright (c) 1988,1989,1990,1991,1992 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.
 |
 | Heavy modifications  Copyright (c) Finnish University and Research Network,
 | FUNET, 1993, 1994.
 |
 | Most of the routines here should be modified to macros.
 | COMPRESS_SCB & UNCOMPRES should be modified to get a flag whether to
 |              translate to EBCDIC or not.
 | Implementation note: When an external program writes a file for us to
 | transmit, one thing should be noted in the envelope: the TYP field must
 | come before the CLS field (the later is optional).
 |
 | Various utility routines:
 | strcasecmp      - Case insensitive string comparison.
 | parse_envelope  - Parses our internal envelope in the file.
 | compress_scb    - Compress the string using SCB. In the meantime, do not
 |                   compress. Only add SCB's.
 | uncompress_scb  - Get an "SCB-compressed" line and convert it into a normal
 |                   string.
 |
 */
#include "consts.h"
#include "ebcdic.h"
#include "headers.h"
#include "prototypes.h"
#include "bintree.h"

/*
 |  Case insensitive strings comparisons. Return 0 only if they have the same
 |  length.
*/


/* 
 | Read the file's envelope, and fill the various parameters from the
 | information stored there.
 | The CLS line must appear AFTER the TYP line, since the TYP line defines
 | the job class.
 | The FLG line must appear after the type and class, since it adds bits
 | to the type.
 | If the From address is too long we put there "MAILER@Local-Site". If the
 | To address is too long, we put there "POSTMAST@Local-Site" so the postmaster
 | will ge it back.
 |
 | The Header format is:
 | FRM: BITnet sender address.
 | TYP: MAIL, FILE (NetData), PRINT (Print file), PUNCH (Send file as punch),
 |      FASA (NetData with ASA CC), PASA (Print with ASA CC).
 | FNM: File name
 | EXT: File name extension.
 | FMT: Character set (ASCII/ASCII8/DISPLAY/BINARY)
 | CLS: one-Character (the job class)
 | FID: Number - The job id to use (if given).
 | TOA: Address. May be replicated the necessary number of times.
 | FLG: flags. FLG_NOQUIET (defined by MAILER.H) is translated to F_NOQUIET to
 |      not put the QUIET form code.
 | TAG: The tag information.
 | FOR: FormsName	[mea]
 | REC: Record count	[mea]
 | DIS: RSCSDIST	[mea]
 | JNM: JOBNAME		[mea]
 | PRT: NJH PRINT	[mea]
 | PUN: NJH PUNCH	[mea]
 | END: End of envelope.
 | Data...
 */
#define	FROM	1
#define	TYPE	2
#define	FNAM	3
#define	FEXT	4
#define	FRMT	5
#define	TO	6
#define	LNG	7
#define	CLASS	8
#define	FILEID	9
#define OFILEID 10
/*#define FLG	11*/
#define TAG	12
#define VIA	13
#define FORM	14	/* RSCS FORM */ /* [mea] */
#define RECORDS 15	/* NDH RECN */ /* [mea] */
#define DIST    16	/* RSCS DIST */ /* [mea] */
#define JOB	17	/* NJH JOBNAME */ /* [mea] */
#define PRTADDR 18	/* NJH PRINT */
#define PUNADDR 19	/* NJH PUNCH */
#define	ILGL	0
#define	FLG_NOQUIET	1	/* From MAILER.H */

/*
 | Return the code relative to the given keyword from the file's envelope.
 */
int
crack_header(line)
const char	*line;
{
	char	temp[6];
	struct hdrnames { int class; char name[8]; };
	static struct hdrnames Headers[] = {
	  {  CLASS, "CLS:" },
	  {  DIST, "DIS:" },
	  {  FEXT, "EXT:" },
	  {  FILEID, "FID:" },
	  {  OFILEID, "OID:" },
	/*{  FLG, "FLG:" },*/
	  {  FRMT, "FMT:" },
	  {  FNAM, "FNM:" },
	  {  FORM, "FOR:" },
	  {  FROM, "FRM:" },
	  {  JOB, "JNM:" },
	  {  PRTADDR, "PRT:" },
	  {  PUNADDR, "PUN:" },
	  {  LNG, "LNG:" },
	  {  RECORDS, "REC:" },
	  {  TAG, "TAG:" },
	  {  TO, "TOA:" },
	  {  TYPE, "TYP:" },
	  {  VIA, "VIA:" },
	  {  ILGL, "\0\0\0\0" }};

	struct hdrnames *hp = Headers;
	u_int32	tt;

	strncpy(temp, line, 5);
	tt = *(u_int32*)temp;

	while (hp->class != ILGL) {
	  if (tt == *(u_int32*)hp->name)
	    return hp->class;
	  ++hp;
	}

	return ILGL;	/* No legal keyword */
}

int
parse_envelope(infile,FileParams,assign_oid)
FILE *infile;
struct FILE_PARAMS *FileParams;
const int assign_oid;
{
	int	i;
	register char	*p, *s;
	/* Prepare to receive ONE long line of header data All blank! */
	char	line[LINESIZE*4];
	struct stat	fstats;
	char	FromUser[9], FromNode[9], ToUser[9], ToNode[9];
	long	filepos, fileid_filepos = -1, ourfileid_filepos = -1;

	FromUser[8] = 0;
	FromNode[8] = 0;
	ToUser[8]   = 0;
	ToNode[8]   = 0;

	FileParams->RecordsCount = -1;
	FileParams->type         = 0;
	FileParams->FileId       = 0;
	FileParams->OurFileId    = 0;
	FileParams->flags        = 0;

	/* Put inital values into variables that might not have value */
	strcpy(FileParams->From, "***@****");	/* Just init it to something */
	sprintf(FileParams->To, "POSTMAST@%s", LOCAL_NAME); /* In case there
							       is empty TO */
	FileParams->PrtTo[0] = 0;
	FileParams->PunTo[0] = 0;
	strcpy(FileParams->FileName, "UNKNOWN");
	strcpy(FileParams->FileExt, "DATA");
	FileParams->tag[0]   = 0;

	strcpy(FileParams->FormsName, DefaultForm);
	strcpy(FileParams->DistName, "SYSTEM");
	FileParams->format   = BINARY;
	FileParams->JobClass = 'A';

	fstat(fileno(infile),&fstats);

	FileParams->FileSize = fstats.st_size;
	FileParams->NJHtime  = fstats.st_mtime;

	while (1) {
	  filepos = ftell(infile);

	  if (fgets(line,sizeof(line)-1,infile) == NULL ||
	      (strncmp(line, "END:", 4) == 0)) break;

	  if (line[3] != ':') return -1; /* Bad header entry! */

	  i = strlen(line);
	  if (line[i-1] == '\n') line[--i] = 0; /* Zap the trailing newline */

	  /* chop off trailing spaces */
	  while ((--i > 0) && (line[i] == ' '))  line[i] = 0;

	  p = line+4; /* Usually this points to a blank.. */
	  while (*p == ' ') ++p;

	  switch (crack_header(line)) {
	    case FROM:
		if ((strlen(p) >= 20) || (*p == '\0'))
		  sprintf(FileParams->To, "MAILER@%s", LOCAL_NAME);
		else
		  strcpy(FileParams->From, p);
		strncpy(FromUser,p,8);
		if ((s = strchr(FromUser,'@'))) *s = 0;
		if ((s = strchr(p,'@'))) {
		  ++s;
		  strncpy(FromNode,s,8);
		}
		break;
	    case TYPE:
		if ((strcasecmp(p, "MAIL") == 0) ||
		    (strcasecmp(p, "BSMTP") == 0)) {
		  FileParams->type = F_MAIL;
		  FileParams->JobClass = 'M';	/* M class */
		} else if (strcasecmp(p, "JOB") == 0) {
		  FileParams->type = F_JOB;
		} else {	/* It is a file - parse the format */
		  FileParams->JobClass = 'A';	/* A class */
		  if (strcasecmp(p, "PRINT") == 0)
		    FileParams->type = F_PRINT;
		  else if (strcasecmp(p, "PUNCH") == 0)
		    FileParams->type = F_PUNCH;
		  else if (strcasecmp(p, "PASA") == 0)
		    FileParams->type = F_ASA | F_PRINT;
		  else if (strcasecmp(p, "FASA") == 0)
		    FileParams->type = F_ASA;
		  else
		    FileParams->type = F_FILE;	/* Default */
		}
		break;
	    case CLASS:		/* Check whether class is ok */
		if ((*p >= 'A') && (*p <= 'Z'))	/* OK */
		  FileParams->JobClass = *p;
		else	/* Don't change it */
		  logger(1, "UTILS, Illegal job class '%c'in CLS\n",
			 *p);
		break;
	    case FNAM:
		if (*p != '\0') {
		  p[8] = '\0';	/* Delimit length */
		  strcpy(FileParams->FileName, p);
		}
		break;
	    case FEXT:
		if (*p != '\0') {
		  p[8] = '\0';
		  strcpy(FileParams->FileExt, p);
		}
		break;
	    case TAG:
		p[sizeof(FileParams->tag)-1] = 0; /* Delimit length */
		strcpy(FileParams->tag, p);
		break;
	    case DIST:
		p[8] = 0; /* delimit length */
		strcpy(FileParams->DistName,p);
		break;
	    case FRMT:
		if (strcmp(p,"EBCDIC")==0)
		  FileParams->format = EBCDIC;
		else
		  FileParams->format = BINARY;  /* DEFAULT */
		break;
	    case FORM:
		p[8] = 0; /* Delimit length */
		strcpy(FileParams->FormsName,p);
		break;
	    case RECORDS:
		sscanf(p,"%ld",&FileParams->RecordsCount);
		break;
	    case JOB:
		p[8] = 0; /* Delimit length */
		strcpy(FileParams->JobName,p);
		break;
	    case PRTADDR:
		strncpy(FileParams->PrtTo,p,17);
		FileParams->PrtTo[17] = 0;
		break;
	    case PUNADDR:
		strncpy(FileParams->PrtTo,p,17);
		FileParams->PrtTo[17] = 0;
		break;
	    case LNG:	/* Language from PC */
		break;	/* Ignore it currently */
	    case TO:
		if (strlen(p) >= 20)
		  sprintf(FileParams->To, "POSTMAST@%s", LOCAL_NAME);
		else
		  if ((*p != '\0') && (*p != ' '))
		    strcpy(FileParams->To, p);
		/* Else - leave the POSTMAST@Local_node */

		strncpy(ToUser,FileParams->To,8);
		if ((s = strchr(ToUser,'@'))) *s = 0;
		if ((s = strchr(FileParams->To,'@'))) {
		  ++s;
		  strncpy(ToNode,s,8);
		}
		break;
	    case FILEID:
		i = atoi(p);
		if ((i > 0) && (i <= 9900))	/* Inside range */
		  FileParams->FileId = i;
		if ((ftell(infile) - filepos) == 10) /* UNIX specific.. */
		  fileid_filepos = filepos;
		break;
	    case OFILEID:
		i = atoi(p);
		if ((i > 0) && (i <= 9900))	/* Inside range */
		  FileParams->OurFileId = i;
		if (FileParams->OurFileId == 0 && assign_oid) {
		  if (ftell(infile) - filepos == 10)
		    ourfileid_filepos = filepos;
		}
		break;
/*
	    case FLG:
		i = atoi(p);
		if ((i & FLG_NOQUIET) != 0)
		  FileParams->type |= F_NOQUIET;
		break;
 */
	    case VIA:
		break;
	    case ILGL:
		logger(1, "UTIL, Illegal header line: '%s'\n", line);
		return -1; /* Failure */
	  }
	}
	filepos = ftell(infile);
	/* Assign sender's FirstfileID */
	if (FileParams->FileId == 0 && fileid_filepos >= 0) {
	  i = get_user_fileid(FromUser);
	  fseek(infile,fileid_filepos,0);
	  fprintf(infile,"FID: %04d\n",i);
	  FileParams->FileId = i;
	}
	/* Assign OutputFileID */
	if (FileParams->OurFileId == 0 && assign_oid &&
	    ourfileid_filepos >= 0) {
	  i = get_send_fileid();
	  fseek(infile,ourfileid_filepos,0);
	  fprintf(infile,"OID: %04d\n",i);
	  FileParams->OurFileId = i;
	}
	fflush(infile);
	fseek(infile,filepos,0);

	FileParams->FileSize = fstats.st_size - filepos;
	if (FileParams->PrtTo[0] == 0)
	  strcpy(FileParams->PrtTo,FileParams->From);
	if (FileParams->PunTo[0] == 0)
	  strcpy(FileParams->PunTo,FileParams->From);

	if (FileParams->tag[0] == 0) {
	  sprintf(FileParams->tag,"%-8s %-8s 50",ToNode,ToUser);
	}


	return 0;
}


/*
 | Add SCB's to the given string.
 */
int
compress_scb(InString, OutString, InSize)
const void	*InString;
void		*OutString;
int	InSize;
{
	register int	i, OutSize;
	const unsigned char	*p;
	unsigned char	*q;

	p = InString;  q = OutString; i = OutSize = 0;
	while (InSize > 63) {
	  *q++ = 0xff;		/* SCB of 63 characters */
	  for(i = 0; i < 63; i++) {
	    *q++ = *p++;	/* Copy 63 characters */
	  }
	  OutSize += 64;	/* 63 characters + SCB character */
	  InSize -= 63;
	}

	/* Copy what is left */
	if (InSize > 0) {
	  OutSize += (InSize + 1);	/* +1 for SCB */
	  *q++ = (0xc0 + InSize);	/* SCB for the left characters */
	  for(i = 0; i < InSize; i++) {
	    *q++ = *p++;
	  }
	}
	*q++ = 0; OutSize++;	/* Final SCB = end of string */
	return OutSize;
}


/*
 | Get a string (first char is the first SCB) and convert it into a normal
 | string. Return the resulting string size.
 */
int
uncompress_scb(InString, OutString, InSize, OutSize, SizeConsumed)
const void	*InString;
void		*OutString;
int		InSize;
const int	OutSize;	/* Size of preallocated space for out string */
int		*SizeConsumed;	/* How many characters we consumed from the
				   input string */
{
	register unsigned char	*q, SCB;
	const unsigned char	*p;
	register int		i, j, ErrorFound, SavedInSize;

	SavedInSize = InSize;
	i = j = *SizeConsumed = 0;
	p = InString; q = OutString;
	ErrorFound = 0;

	while (InSize-- > 0) {		/* Decrement by one for the SCB byte */
	  *SizeConsumed += 1;			/* SCB byte */
	  switch ((SCB = *p++) & 0xf0) {	/* The SCB */
	    case 0:			/* End of record */
		if (ErrorFound == 0)
		  return j;
		else	return -1;
	    case 0x40:			/* Abort */
		return -1;
	    case 0x80:
	    case 0x90:			/* Blanks */
		j += (SCB &= 0x1f);		/* Get the count */
		if (j > OutSize) {		/* Output too long */
		  logger(1, "UTILS: Output line in convert too long (spaces).\n");
		  ErrorFound = -1;
		  break;
		}
		for(i = 0; i < SCB; i++)
		  *q++ = E_SP;
		break;
	    case 0xa0:
	    case 0xb0:			/* Duplicate single character */
		j += (SCB &= 0x1f);
		if (j > OutSize) { 		/* Output too long */
		  logger(1, "UTILS: Output line in convert too long (Dup).\n");
		  ErrorFound = -1;
		  ++p; --InSize; *SizeConsumed += 1;
		  break;
		}
		for (i = 0; i < SCB; i++)
		  *q++ = *p;
		++p; --InSize; *SizeConsumed += 1;
		break;
	    case 0xc0:
	    case 0xd0:
	    case 0xe0:
	    case 0xf0:			/* Copy characters as they are */
		j += (SCB &= 0x3f);
		if (j > OutSize) {		/* Output too long */
		  logger(1,"UTILS: Output line in convert too long.\n");
		  ErrorFound = -1;
		  InSize -= SCB;
		  *SizeConsumed += SCB;
		  p += SCB;			/* Jump over the characters */
		  break;
		}
		InSize -= SCB;
		*SizeConsumed += SCB;
		for (i = 0; i < SCB; i++) {
		  *q++ = *p++;
		}
		break;
	  }
	}

	/* If we are here, then there is no closing SCB. Some error... */
	logger(1, "UTILS: No EOR SCB in convert.\n");
	trace(InString, SavedInSize, 1);
	return -1;
}

void
ASCII_TO_EBCDIC(INSTRING, OUTSTRING, LENGTH)
const void *INSTRING;
void *OUTSTRING;
const int LENGTH;
{
	int TempVar;
	unsigned char *op = OUTSTRING;
	const unsigned char *ip = INSTRING;
	for (TempVar = 0; TempVar < LENGTH; TempVar++)
	  op[TempVar] = ASCII_EBCDIC[ ip[TempVar] ];
}

void
EBCDIC_TO_ASCII(INSTRING, OUTSTRING, LENGTH)
const void *INSTRING;
void *OUTSTRING;
const int LENGTH;
{
	int TempVar;
	unsigned char *op = OUTSTRING;
	const unsigned char *ip = INSTRING;
	for (TempVar = 0; TempVar < LENGTH; TempVar++)
	  op[TempVar] = EBCDIC_ASCII[ ip[TempVar] ];
}

void
PAD_BLANKS(STRING, ORIG_SIZE, FINAL_SIZE)
void *STRING;
const int ORIG_SIZE, FINAL_SIZE;
{
	int TempVar;
	unsigned char *s = (unsigned char *)STRING;

	for (TempVar = ORIG_SIZE; TempVar < FINAL_SIZE; TempVar++)
	  s[TempVar] = E_SP;
}

#ifdef NEED_STRCASECMP
int
strcasecmp(a,b)
const void *a, *b;
{
	const unsigned char	*p, *q;

	p = a; q = b;

#define	TO_UPPER(c)	(((c >= 'a') && (c <= 'z')) ? (c - ' ') : c)

	for (; TO_UPPER(*p) == TO_UPPER(*q) && *p && *q;
	     p++,q++) ;

	if ((*p == 0) && (*q == 0))  /* Both strings done = Equal */
	  return 0;

	/* Not equal */
	return (TO_UPPER(*p) - TO_UPPER(*q));
}
#endif

int
despace(str,len)
char *str;
int len;
{
	if (*str == 0) return 0;
	while (len > 0 && str[len-1] == ' ')
	  --len;
	str[len] = 0;
	return len;
}


int32
timevalsub(result,a,b)
struct timeval *result,*a,*b;
{
	result->tv_sec  = a->tv_sec  - b->tv_sec;
	if ((result->tv_usec = a->tv_usec - b->tv_usec) < 0) {
	  result->tv_usec += 1000000L;
	  result->tv_sec  -= 1L;
	}
	return result->tv_sec;
}

int32
timevaladd(result,a,b)
struct timeval *result,*a,*b;
{
	result->tv_sec  = a->tv_sec  + b->tv_sec;
	if ((result->tv_usec = a->tv_usec - b->tv_usec) >= 1000000L) {
	  result->tv_usec -= 1000000L;
	  result->tv_sec  += 1L;
	}
	return result->tv_sec;
}


/* SpoolID handling routines -- keeps spoolid info in  Bintree databse,
   see   bintree.[ch] -- by Matti Aarnio <mea@nic.funet.fi> on 15-16 Jan-94 */


static int FileID = 0, UFileID = 0;
static struct Bintree *SpoolidBT = NULL;

struct spoolids {
	char  name[9];
	short spoolid;
};

static int
spoolid_compare(s1,s2)
struct spoolids *s1, *s2;
{
	return strcmp(s1->name,s2->name);
}

static int
fileid_db_open()
{
	char path[LINESIZE];
	sprintf(path,"%s/.spoolid.htdb",BITNET_QUEUE);

	SpoolidBT = bintree_open(path, sizeof(struct spoolids),
				 spoolid_compare);
	return (SpoolidBT == NULL);
}

void
fileid_db_close()
{
	/* Used when system gets a SIGHUP */
	if (SpoolidBT != NULL) bintree_close(SpoolidBT);
	SpoolidBT = NULL;
}

int
get_send_fileid()
{
	static struct spoolids key = { "*NJE*", 0 };
	struct spoolids *datp;

	if (SpoolidBT == NULL) fileid_db_open();
	if (SpoolidBT == NULL) {
	  /* AARGH!! */
	  FileID = (FileID % 9900) + 1;
	  return FileID;
	}

	datp = bintree_find(SpoolidBT,&key);
	if (!datp) {
	  bintree_insert(SpoolidBT,&key);
	  datp = bintree_find(SpoolidBT,&key);
	}

	if (!datp) {
	  /* AARGH!! */
	  FileID = (FileID % 9900) + 1;
	  return FileID;
	}

	datp->spoolid = (datp->spoolid % 9900) + 1;
	bintree_update(SpoolidBT,datp);
	return datp->spoolid;
}


int
get_user_fileid(uname)
const char *uname;
{
	struct spoolids key;
	struct spoolids *datp;

	if (SpoolidBT == NULL) fileid_db_open();
	if (SpoolidBT == NULL) {
	  /* AARGH!! */
	  UFileID = (UFileID % 9900) + 1;
	  return UFileID;
	}

	strcpy(key.name,uname);
	key.spoolid = 0;

	datp = bintree_find(SpoolidBT,&key);
	if (!datp) {
	  bintree_insert(SpoolidBT,&key);
	  datp = bintree_find(SpoolidBT,&key);
	}

	if (!datp) {
	  /* AARGH!! */
	  UFileID = (UFileID % 9900) + 1;
	  return UFileID;
	}

	datp->spoolid = (datp->spoolid % 9900) + 1;
	bintree_update(SpoolidBT,datp);
	return datp->spoolid;
}

/* Store the user-spoolid back, in case we had to pick another..
   this is for local spool storing */
void
set_user_fileid(uname,spoolid)
const char *uname;
const int spoolid;
{
	struct spoolids key;
	struct spoolids *datp;

	if (SpoolidBT == NULL) fileid_db_open();
	if (SpoolidBT == NULL) return;

	strcpy(key.name,uname);
	key.spoolid = spoolid;

	datp = bintree_find(SpoolidBT,&key);
	if (!datp) { /* This should never happen, but one never knows.. */
	  logger(1,"** UTIL: set_uid_fileid('%s',%d) called w/o existing user entry! **\n",
		 uname,spoolid);
	  bintree_insert(SpoolidBT,&key);
	  return;
	}

	if (!datp) return;

	datp->spoolid = spoolid;
	bintree_update(SpoolidBT,datp);
}

/* Report age -- in milliseconds  - as a STRING */
char *
MsecAgeStr(timeptr, ageptr)
TIMETYPE *timeptr, *ageptr;
{
	static char buf[20];
	TIMETYPE age;
	if (ageptr == NULL)
	  ageptr = &age;

#ifndef NO_GETTIMEOFDAY
	memset(&age,0,sizeof(age));
	if (ageptr->tv_sec == 0)
	  GETTIME(ageptr);
	/* Using ``struct timeval'', et.al. - microsecond resolution */
	ageptr->tv_sec  = ageptr->tv_sec  - timeptr->tv_sec;
	ageptr->tv_usec = ageptr->tv_usec - timeptr->tv_usec;
	if (ageptr->tv_usec < 0) {
	  ageptr->tv_sec -= 1;
	  ageptr->tv_usec += 1000000;
	}
	sprintf(buf,"%d.%03d",ageptr->tv_sec, (ageptr->tv_usec + 500)/1000);
#else
	/* Using one second resolution. */
	if (*ageptr == 0)
	  GETTIME(ageptr);
	*ageptr = *timeptr - *ageptr;
	sprintf(buf,"%d",*ageptr);
#endif
	return buf;
}

char *
BytesPerSecStr(bytecount,deltatimep)
	long bytecount;
	TIMETYPE  *deltatimep;
{
	static char buf[20];
#ifndef NO_GETTIMEOFDAY
	/* Using ``struct timeval'', et.al. - microsecond resolution */
	double age = (double)deltatimep->tv_sec + (deltatimep->tv_usec * 0.000001);
	if (age == 0.0)
	  sprintf(buf,"%ld",bytecount);
	else
	  sprintf(buf,"%6.3f",(double)bytecount/age);
#else
	/* Using one second resolution. */
	if (*deltatimep == 0)
	  sprintf(buf,"%ld",bytecount);
	else
	  sprintf(buf,"%5.1f",(double)bytecount/(double)*deltatimep);
#endif
	return buf;
}
