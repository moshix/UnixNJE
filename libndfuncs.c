/*	FUNET-NJE/HUJI-NJE		Client utilities
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 26-Sep-1993
 */


#include "prototypes.h"
#include "clientutils.h"
#include "ebcdic.h"
#include "ndlib.h"

/* [mea]
 *
 * Punch buffered -- Netdata is output w/o much regard on 80-char
 * virtual cards, and thus this is needed for creating a simulation
 * layer..
 */

/* Return 0 for failure, 1 for ok. */
int
punch_buffered(PUNCH,TempLine,size,flushflg)
struct puncher *PUNCH;
void *TempLine;
int size,flushflg;
{
	unsigned char *inbuf = TempLine;

	/*printf("PUNBUF: %d bytes, first three: X'%02X%02X%02X'\n",
	  size,inbuf[0],inbuf[1],inbuf[2]);*/

	while (PUNCH->len+size >= 80) {
	  memcpy(PUNCH->buf + 2 +PUNCH->len, inbuf, 80 - PUNCH->len);
	  size  -= (80 - PUNCH->len);
	  inbuf += (80 - PUNCH->len);
	  if (!Uwrite(PUNCH->fd, PUNCH->buf, 82)) return 0;
	  PUNCH->punchcnt += 1;
	  PUNCH->len = 0;
	}
	if (size > 0) {
	  memcpy(PUNCH->buf + 2 + PUNCH->len, inbuf, size);
	  PUNCH->len += size;
	  size = 0;
	}
	if (flushflg && PUNCH->len > 0) {
	  memset(PUNCH->buf + 2 + PUNCH->len, 0, 80 - PUNCH->len);
	  if (!Uwrite(PUNCH->fd, PUNCH->buf, 82)) return 0;
	  PUNCH->punchcnt += 1;
	  PUNCH->len = 0;
	}
	return 1;
}


#define	INMFTIME	0x1024
#define INMCREAT	0x1022
#define	INMLRECL	0x0042
#define	INMNUMF		0x102F
#define INMFACK		0x1026
#define	INMFNODE	0x1011
#define	INMFUID		0x1012
#define	INMTNODE	0x1001
#define	INMTUID		0x1002
#define	INMSIZE		0x102C
#define	INMDSORG	0x003C
#define	INMUTILN	0x1028
#define	INMRECFM	0x0049
#define	INMDSNAM	0x0002

#define	PHYSICAL	0x4000	/* File organization - Physical */


/* [ HUJI's code nearly as is.. ]
 |
 | Insert a single text unit into the output line.
 | The input is either string or numeric. 
 |
 | The returned value is the number of characters written to OutputLine.
 | Input parameters:
 |     InputKey - The keyword (a number).
 |     InputParameter - A numeric value if the keyword needs numeric data.
 |     InputString - the parameter when keyword needs string data.
 | (Either InputParameter or InputString is used, but not both).
 |     InputSize - size of the string (if exists).
 |     InputType = Do we have to use InputParam or InputString?
 */
static int
insert_text_unit(InputKey, InputParameter, InputString, InputSize, InputType,
	OutputLine)
int	InputKey, InputType, InputParameter, InputSize;
unsigned char	*InputString, *OutputLine;
{
	int	size, i, j;
	unsigned char	TextUnits[3][80];
	int	NumTextUnits;
	size = 0;

	OutputLine[size++] = ((InputKey & 0xff00) >> 8);
	OutputLine[size++] = (InputKey & 0xff);

	if(InputType == 0) {	/* Number */
	  OutputLine[size++] = 0;
	  OutputLine[size++] = 1; /* One item only */
	  OutputLine[size++] = 0; /* Count is 2 or 4, so the high order byte
				     count is always zero */
	  /* Test whether it fits in 2 bytes or needs 4 bytes: */
	  if(InputParameter <= 0xffff) {
	    OutputLine[size++] = 2; /* We use 2 bytes integer */
	    OutputLine[size++] = ((InputParameter & 0xff00) >> 8);
	    OutputLine[size++] = (InputParameter & 0xff);
	    return size;
	  } else {
	    OutputLine[size++] = 4; /* We use 4 bytes integer */
	    OutputLine[size++] = ((InputParameter & 0xff000000) >> 24);
	    OutputLine[size++] = ((InputParameter & 0xff0000) >> 16);
	    OutputLine[size++] = ((InputParameter & 0xff00) >> 8);
	    OutputLine[size++] = (InputParameter & 0xff);
	    return size;
	  }
	}

	/* String: */
	/* Check whether the string has more than one value
	   (separated by spaces). If so, convert it to separate
	   text units */
	NumTextUnits = 0; j = 0;
	for(i = 0; i < InputSize; i++) {
		if(InputString[i] == E_SP) {	/* Space found */
			TextUnits[NumTextUnits][0] = j;	/* Size */
			NumTextUnits++;	/* We do not check for overflow */
			j = 0;
		} else {
			TextUnits[NumTextUnits][j + 1] = InputString[i];
			j++;
		}
	}
	TextUnits[NumTextUnits++][0] = j;	/* Size */

	OutputLine[size++] = 0;
	OutputLine[size++] = NumTextUnits;		/* One item only */
	for (i = 0; i < NumTextUnits; i++) {
	  OutputLine[size++] = 0;
	  OutputLine[size++] = (TextUnits[i][0] & 0xff);
	  memcpy(&OutputLine[size], &(TextUnits[i][1]),
		 (int)(TextUnits[i][0] & 0xff));
	  size += TextUnits[i][0] & 0xff;
	}
	return size;
}


/* [ HUJI's code heavily modified.. ]
 | Start of a Netdata file. Fill the first 3 INMR's into the buffer.
 */
int
fill_inmr01(PUNCH,infile,From,To,fname,ftype,lrecl,recfm,askack)
struct puncher *PUNCH;
FILE	*infile;
char	*From, *To, *fname, *ftype, *askack;
int	recfm;
int	lrecl;
{
	char	TempLine[800], *p;
	char	line[40];
	char	fname8[9],ftype8[9];
	int	size, i;
	time_t	tim, ctim;
	struct tm	*tms;
	struct stat	stats;

	/* TempLine[0] = rec.len */
	TempLine[1] = 0xe0;	/* Control record */
	size = 2;

	ASCII_TO_EBCDIC("INMR01", TempLine+size, 6);
	size += 6;
	strcpy(line, From);
	if ((p = strchr(line, '@')) == NULL) p = line;
	*p++ = 0;		/* Separate username from nodename */
	i = strlen(line); 	/* Username */
	ASCII_TO_EBCDIC(line, line, i);
	size += insert_text_unit(INMFUID,  0, line, i, 1, TempLine+size);
	i = strlen(p);		/* Node name */
	ASCII_TO_EBCDIC(p, p, i);
	size += insert_text_unit(INMFNODE, 0,    p, i, 1, TempLine+size);

	strcpy(line, To);
	if ((p = strchr(line, '@')) == NULL) p = line;
	*p++ = 0;		/* Separate username from nodename */
	i = strlen(line); 	/* Username */
	ASCII_TO_EBCDIC(line, line, i);
	size += insert_text_unit(INMTUID,  0, line, i, 1, TempLine+size);
	i = strlen(p);		/* Node name */
	ASCII_TO_EBCDIC(p, p, i);
	size += insert_text_unit(INMTNODE, 0,    p, i, 1, TempLine+size);

	/* Create a time stamp(s) */
	fstat(fileno(infile),&stats);
	if (S_ISREG(stats.st_mode))
	  ctim = stats.st_mtime;
	else {
	  ctim = 0;	/* We don't know, thus we don't claim it.. */
	  stats.st_size = 2048; /* Not a regular file, don't know true size */
	}
	
	time(&tim);	/* NETDATA generation time == NOW! */
	tms = gmtime(&tim);
	i = strftime(line,sizeof line,"%Y%m%d%H%M%S",tms);
	ASCII_TO_EBCDIC(line, line, i);
	size += insert_text_unit(INMFTIME,  0, line, i, 1, TempLine+size);

	/* On INMR01 the LRECL is fixed 80 */
	size += insert_text_unit(INMLRECL, 80,    0, 0, 0, TempLine+size);
	size += insert_text_unit(INMNUMF,   1,    0, 0, 0, TempLine+size);

	if (askack) {
	  i = strlen(askack);
	  ASCII_TO_EBCDIC(askack, line, i);
	  size += insert_text_unit(INMFACK, 0, line, i, 1, TempLine+size);
	}

	TempLine[0] = size;
	if (!punch_buffered(PUNCH,TempLine,size,0)) return 0;

	/* Second INMR */
	TempLine[1] = 0xE0;
	size = 2;
	ASCII_TO_EBCDIC("INMR02", TempLine+size, 6);
	size += 6;
	memcpy(TempLine+size, "\0\0\0\1", 4); /* one file */
	size += 4;

	ASCII_TO_EBCDIC("INMCOPY", line, 7);
	size += insert_text_unit(INMUTILN,       0, line, 7, 1, TempLine+size);
	size += insert_text_unit(INMDSORG,   PHYSICAL, 0, 0, 0, TempLine+size);
	size += insert_text_unit(INMLRECL,      lrecl, 0, 0, 0, TempLine+size);
	size += insert_text_unit(INMRECFM,      recfm, 0, 0, 0, TempLine+size);
	size += insert_text_unit(INMSIZE,stats.st_size,0, 0, 0, TempLine+size);

	/* Create the file name as "A File-name File-Ext".
	   The A is for IBM and must be the first */
	strncpy(fname8,fname,8); fname8[8] = 0;
	strncpy(ftype8,ftype,8); ftype8[8] = 0;
	if (*ftype8 == 0)
	  sprintf(line, "A %s", fname8);
	else
	  sprintf(line, "A %s %s", fname8, ftype8);
	upperstr(line);
	i = strlen(line);
	ASCII_TO_EBCDIC(line, line, i);
	size += insert_text_unit(INMDSNAM, 0, line, i, 1, TempLine+size);

	if (ctime != 0) {
	  /* File creation time */
	  tms = gmtime(&ctim);
	  i = strftime(line,sizeof line,"%Y%m%d%H%M%S",tms);
	  ASCII_TO_EBCDIC(line, line, i);
	  size += insert_text_unit(INMCREAT,  0, line, i, 1, TempLine+size);
	}

	TempLine[0] = size;
	if (!punch_buffered(PUNCH,TempLine,size,0)) return 0;


	/* Third INMR */

	TempLine[1] = 0xE0;
	size = 2;
	ASCII_TO_EBCDIC("INMR03", TempLine+size, 6);
	size += 6;
	/* INMR03 has RECFM, and LRECL constant, INMR02 has the real ones */
	size += insert_text_unit(INMRECFM, 0x0001,     0, 0, 0, TempLine+size);
	size += insert_text_unit(INMLRECL,     80,     0, 0, 0, TempLine+size);
	size += insert_text_unit(INMDSORG, 0x4000,     0, 0, 0, TempLine+size);
	size += insert_text_unit(INMSIZE, stats.st_size, 0,0,0, TempLine+size);

	TempLine[0] = size;
	return punch_buffered(PUNCH,TempLine,size,0);
}

int
fill_inmr06(PUNCH)
struct puncher *PUNCH;
{
	char	inmr06[10]; /* INMR06 - EOF flag */

	inmr06[0] = 8;    /* NETDATA segment length */
	inmr06[1] = 0xE0; /* NETDATA segment flags.. */
	ASCII_TO_EBCDIC("INMR06",inmr06+2,6);
	return punch_buffered(PUNCH,inmr06,8,1); /* FLUSH! */
}

int
punch_nddata(PUNCH,Linebuf,linesize)
struct puncher *PUNCH;
void *Linebuf;
int linesize;
{
	unsigned char *linebuf = Linebuf;
	char *p = (char*)linebuf;

	if (linesize < 254) {

	  /* One segment */

	  linebuf[0] = linesize+2; /* NETDATA segment length */
	  linebuf[1] = 0xC0;	/* NETDATA segment flags */
	  return punch_buffered(PUNCH,linebuf,linesize+2,0);

	} else {

	  /* Separate the block into smaller segments,
	     put out the first record */

	  p[0] = 253+2;		/* NETDATA segment size */
	  p[1] = 0x80;		/* NETDATA segment flag */
	  if (!punch_buffered(PUNCH,p,253+2,0)) return 0;
	  linesize -= 253;
	  p += 253;		/* 253 bytes forwards.. */

	  /* Middle segments */
	  while (linesize > 253) {
	    p[0] = 253+2;	/* NETDATA segment size */
	    p[1] = 0x00;	/* NETDATA segment flag */
	    if (!punch_buffered(PUNCH,p,253+2,0)) return 0;
	    linesize -= 253;
	    p += 253;		/* 253 bytes forwards.. */
	  }

	  /* The last segment */
	  p[0] = linesize + 2;	/* NETDATA segment size */
	  p[1] = 0x40;		/* NETDATA segment flag */
	  return punch_buffered(PUNCH,p,linesize+2,0);
	}
}
