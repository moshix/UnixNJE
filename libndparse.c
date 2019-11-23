/* NETDATA-PARSE.C

   Originally a forefather of this module was within HUJI-NJE protocol.c,
   and recv_file.c, but [mea@nic.funet.fi] didn't like it at all...

   Now this is separate entity and does not mess with BITNET
   transport.  (Which is dangerous to do -- no guarantees what is
   transported, so no reason to blow transport because of faulty
   NETDATA file.)

   This module is a library routine for UNIX user programs, and has some
   non-re-entrancy codes in it.

   Copyright  1988,1989,1990 (?) by
	The Hebrew University of Jerusalem, Computation Center.
   Copyright  1993 by
	Finnish University and Research Network, FUNET.

   [Got the beast working on 19-Oct-93,   mea@nic.funet.fi]

 */


#include "ebcdic.h"
#include "prototypes.h"
#include "clientutils.h"
#include "ndlib.h"

#undef LINESIZE
#define LINESIZE 65536

static int NDcc = 0;  /* Netdata CC info */
static unsigned char SavedNdLine[LINESIZE];
static unsigned char SavedNdFlag;
static int  CarriageControl = 0;
static int  SavedNdPosition = 0;
static unsigned char SavedNdSegment[256];
static int  SavedSegmentSize  = 0;
static char *inmr   = "\311\325\324\331";		/* INMR   */

#define	MACHINE_CC	1		/* For Netdata parsing routine */

static char *NDP = "NETDATAPARSE: ";


/*
 | Retrieve one text unit from the passed buffer.
 */
static int
get_text_unit(buffer, length, key, value)
unsigned char	*buffer, *value;
int	*key, length;
{
	register int	j, i, count, size, position;

	*key = ((buffer[0] << 8) + buffer[1]);
	count = ((buffer[2] << 8) + buffer[3]);
	size = 0; position = 4;
	value[0] = 0;
	
	if (count == 0) return 0;

	/* Loop the count numbered. Separate values by spaces */
	for (j = 0; j < count; j++) {
	  i = ((buffer[position] << 8) + buffer[position + 1]);
	  if ((i > (length - 6))) {
	    fprintf(stderr, "%sget-text-unit (key=X'%04X'), field-length=%d > length=%d, at count %d, offset=%d\n",
		    NDP, *key, i, length, j, position);
	    return -1;
	  }

	  if (i < 0) { /* should not happen.. */
	    fprintf(stderr, "%sget-text-unit (key=X'%04X'), field-length=%d at count %d, offset=%d\n",
		    NDP, *key, i, j, position);
	    return -1;
	  }
	  position += 2;
	  if (i)
	    memcpy(&value[size], &buffer[position], i);
	  size += i; position += i;
	  /* Add 2 spaces. This will count also for the 2 bytes
	     length field... */
	  value[size++] = E_SP; value[size++] = E_SP;
	}
	size -= 2;	/* For the last 2 spaces */
	value[size] = '\0';
	return size;
}


/*
 | Try parsing the net-data control records. Ignore all non relevant records.
 | Currently, only write the information gathered from control records and
 | don't do much more than that.
 */
#define INMFNODE	0x1011
#define	INMFUID		0x1012
#define	INMTNODE	0x1001
#define	INMTUID		0x1002
#define	INMDSORG	0x3c
#define	INMLRECL	0x42
#define	INMRECFM	0x49
#define	INMSIZE		0x102c
#define	INMNUMF		0x102f
#define INMFACK		0x1026
#define INMDSNAM	0x0002
#define INMMEMBR	0x0003
#define INMTERM		0x0028
#define INMFTIME	0x1024
#define INMCREAT	0x1022
/* File organization: */
#define	VSAM		0x8
#define	PARTITIONED	0x200
#define	SEQUENTIAL	0x4000

struct keynames {
	short key;
	char *name;
	};
struct keynames KeyNames[] = {
	  { 0x0001, "INMDDNAM" },
	  { 0x0002, "INMDSNAM" },
	  { 0x0003, "INMMEMBR" },
	  { 0x000C, "INMDIR  " },
	  { 0x0022, "INMEXPDT" },
	  { 0x0028, "INMTERM " },
	  { 0x0030, "INMBLKSZ" },
	  { 0x003C, "INMDSORG" },
	  { 0x0042, "INMLRECL" },
	  { 0x0049, "INMRECFM" },
	  { 0x1001, "INMTNODE" },
	  { 0x1002, "INMTUID " },
	  { 0x1011, "INMFNODE" },
	  { 0x1012, "INMFUID " },
	  { 0x1020, "INMLREF " },
	  { 0x1021, "INMLCHG " },
	  { 0x1022, "INMCREAT" },
	  { 0x1023, "INMFVERS" },
	  { 0x1024, "INMFTIME" },
	  { 0x1025, "INMTTIME" },
	  { 0x1026, "INMFACK " },
	  { 0x1027, "INMERRCD" },
	  { 0x1028, "INMUTILN" },
	  { 0x1029, "INMUSERP" },
	  { 0x102A, "INMRECCT" },
	  { 0x102C, "INMSIZE " },
	  { 0x102D, "INMFFM  " },
	  { 0x102F, "INMNUMF " }
	};

static void
print_keyname(key,offset,size,keynamep)
const int key, offset, size;
char **keynamep;
{
	int lo,hi,mid;
	static char KeyNameStr[10];

	lo = 0;
	hi = (sizeof(KeyNames)/sizeof(struct keynames)) -1;

	while (lo <= hi) {
	  mid = (lo+hi) >> 1;
	  if (key == KeyNames[mid].key)
	    break;
	  if (key < KeyNames[mid].key)
	    hi = mid-1;
	  else
	    lo = mid+1;
	}

	if (key == KeyNames[mid].key) {
	  fprintf(stderr,"%sGot key %s (X'%04X') at offset %d, size=%d\n",
		  NDP, KeyNames[mid].name, key, offset, size);
	  *keynamep = KeyNames[mid].name;
	} else {
	  fprintf(stderr,"%sGot unknown key X'%04X' at offset %d, size=%d\n",
		  NDP, key, offset, size);
	  *keynamep = KeyNameStr;
	  sprintf(KeyNameStr,"X'%04X'",key);
	}
}


static int
parse_net_data_control_record(buffer, length, ndp, debugdump)
int	length, debugdump;
unsigned char	*buffer;
struct ndparam	*ndp;
{
	register unsigned char	*p;
	register int	size, rsize, i = 0;
	int		key;			/* Key of values */
	int		ilen = length;
	unsigned char	value[LINESIZE],	/* Value of text unit */
			Aline[LINESIZE];
	char *keyname;

	if (memcmp(buffer,inmr,4) != 0) {
	  fprintf(stderr, "%sIllegal ND control record\n",NDP);
	  trace(buffer,ilen,1);
	  return 0;
	}
	/* We know that the first 4 characters are INMR.
	   Now find which one */
	p = buffer + 5;	/* [4] = 0, [5] = second number */
	length -= 6;
	switch (*p++) {
	  case 0xf2:	/* 02 */
	      p += 4;
	      length -= 4;	/* INMR02 has 4 byte file number before
				   text units. Jump over it */
	  case 0xf1:	/* 01 */
	      ndp->DSNAM[0] = 0;
	      ndp->MEMBR[0] = 0;
	      ndp->FTIMES[0] = 0;
	      ndp->CTIMES[0] = 0;
	  case 0xf3:	/* INMR03 */
	  case 0xf4:	/* User specified */

	      if (debugdump) {
		fprintf(stderr,"%sINMR0%c, size=%d\n",NDP,
			((p[-1])&0x0f)+'0',length+6);
	      }

	      while (length > 0) {
		key = 0;
		if ((size = get_text_unit(p, length, &key, value)) < 0) {
		  fprintf(stderr,
			  "%sget_text_unit() p=%d, key=X'%04X', length=%d, rc=%d\n",
			  NDP,p-buffer,key,ilen,size);
		  trace(buffer,ilen,1);
		  return size;
		}
		if (debugdump)
		  print_keyname(key,p-buffer,size,&keyname);
 
		switch (key) {
	 	  case INMDSORG:
		  case INMRECFM:
		  case INMLRECL:
		  case INMSIZE:
		  case INMNUMF:
		      switch (size) {
		        case 1:
			    i = value[0];
			    if (debugdump)
			      fprintf(stderr, "%s    value=X'%02X' (D'%d')\n",
				      NDP, i, i);
			    break;
			case 2:
			    i = (value[0] << 8) + value[1];
			    if (debugdump)
			      fprintf(stderr, "%s    value=X'%04X' (D'%d')\n",
				      NDP, i, i);
			    break;
			case 4:
			    i = (value[0] << 24) +
				(value[1] << 16) +
				(value[2] << 8) + value[3];
			    if (debugdump)
			      fprintf(stderr, "%s    value=X'%08X' (D'%d')\n",
				      NDP, i, i);
			    break;
			default:
			    break;
		      }
		      /* Right, we have values, now analyze the result */
		      switch (key) {
			case INMDSORG:
			    if (i != SEQUENTIAL)
			      fprintf(stderr,
				     "%sNetdata file organization=X'%04X' (non SEQential) not supported\n",
				      NDP, i);
			    break;
			case INMRECFM:

			    if (debugdump)
			      fprintf(stderr,
				      "%sNetdata RECFM = X'%04X'\n", NDP, i);

			    /* Record content info */

			    if (i & 0x0001) {
			      /* Shortened VSB format */
			    } else if (i & 0x0002) {
			      /* Varying length records without
				 the 4 byte header */
			    } else if (i & 0x0004) {
			      /* Packed file -- multiple blanks removed */
			    } else if (i & 0x0008) {
			      /* Packed file -- Huffman encoded */
			    }

			    /* If a printer file, what format ? */

			    NDcc = 0;	/* Default */
			    if (i & 0x0200) {
			      /* Data includes MCC printer control chars */
			      NDcc = MACHINE_CC;
			    } else if (i & 0x0400) {
			      /* Data includes ASA printer control chars */
			    }

			    /* Blocking info */

			    if (i & 0x0800) {
			      /* Standard fixed records,
				 or spanned variable records*/
			    } else if (i & 0x1000) {
			      /* Blocked records */
			    } else if (i & 0x2000) {
			      /* Track overflow, or variable ASCII records */
			    }

			    /* Record kind info */

			    if ((i & 0xC000) == 0xC000) {
			      /* Undefined records */
			    } else if (i & 0x4000) {
			      /* Variable length records */
			    } else if (i & 0x8000) {
			      /* Fixed length records */
			    }

			    break;
			case INMLRECL:
			    if (debugdump)
			      fprintf(stderr, "%sNetdata LRECL = %d\n",
				      NDP, i);
			    if (i > 65535)
			      fprintf(stderr,"%sNetdata record length=%d too long\n", NDP, i);
			    break;

			default:
			    /* if (debugdump) {
			      EBCDIC_TO_ASCII(value, Aline, size);
			      Aline[size] = '\0';
			      fprintf(stderr,"%sINMR: key=%s, value=A'%s'\n",
				      NDP, keyname, Aline);
			    } */
			    break;
		      } /* End of value-analysis */

		      break;
		  case INMFACK:
		      ndp->ackwanted = 1;
		      memcpy(ndp->ACKKEY,value,size);
		      ndp->ackkeylen = size;
		      if (debugdump)
			fprintf(stderr,"%sINMR: Asks for ACK, flag size=%d\n",
				NDP, size);
		      break;
		  case INMTERM:
		      if (debugdump) {
			fprintf(stderr,"%s  File is PROFS NOTE\n",NDP);
		      }
		      break;
		  case INMFTIME:
		      rsize = size;
		      if (size > sizeof(ndp->FTIMES)-2)
			rsize = sizeof(ndp->FTIMES)-2;
		      EBCDIC_TO_ASCII(value,ndp->FTIMES,rsize);
		      ndp->FTIMES[rsize] = 0;
		      {
			struct tm tm;
			char *s = ndp->FTIMES;

			tm.tm_year = ((((s[0] - '0')*10 +
					(s[1] - '0'))*10 +
				       (s[2] - '0'))*10 +
				      (s[3] - '0')) - 1900;
			tm.tm_mon = ((s[4] - '0')*10 +
				     (s[5] - '0')) -1;
			tm.tm_mday = ((s[6] - '0')*10 +
				      (s[7] - '0'));
			tm.tm_hour = ((s[8] - '0')*10 +
				      (s[9] - '0'));
			tm.tm_min = ((s[10] - '0')*10 +
				     (s[11] - '0'));
			tm.tm_sec = ((s[12] - '0')*10 +
				     (s[13] - '0'));
			tm.tm_wday = 0;
			tm.tm_yday = 0;
			/*tm.tm_zone = NULL;
			  tm.tm_gmtoff = 0; */
			tm.tm_isdst = 0;

#if	defined(_POSIX_SOURCE) || defined(SYSV) || defined(USG) || defined(__svr4__)
			ndp->ftimet = mktime(&tm);
#else
			ndp->ftimet = timegm(&tm);
#endif
			/* fprintf(stderr,"NDPARSE: FTIMES='%s', -> y=%d m=%d d=%d, %02d:%02d:%02d; timeval=%d\n",
			   ndp->FTIMES,
			   tm.tm_year,tm.tm_mon,tm.tm_mday,
			   tm.tm_hour,tm.tm_min,tm.tm_sec,
			   ndp->ftime); */
		      }
		      goto contentprint;
		  case INMCREAT:
		      rsize = size;
		      if (size > sizeof(ndp->CTIMES)-2)
			rsize = sizeof(ndp->CTIMES)-2;
		      EBCDIC_TO_ASCII(value,ndp->CTIMES,rsize);
		      ndp->CTIMES[rsize] = 0;
		      {
			struct tm tm;
			char *s = ndp->CTIMES;

			tm.tm_year = ((((s[0] - '0')*10 +
					(s[1] - '0'))*10 +
				       (s[2] - '0'))*10 +
				      (s[3] - '0')) - 1900;
			tm.tm_mon = ((s[4] - '0')*10 +
				     (s[5] - '0')) -1;
			tm.tm_mday = ((s[6] - '0')*10 +
				      (s[7] - '0'));
			tm.tm_hour = ((s[8] - '0')*10 +
				      (s[9] - '0'));
			tm.tm_min = ((s[10] - '0')*10 +
				     (s[11] - '0'));
			tm.tm_sec = ((s[12] - '0')*10 +
				     (s[13] - '0'));
			tm.tm_wday = 0;
			tm.tm_yday = 0;
			/*tm.tm_zone = NULL;
			  tm.tm_gmtoff = 0; */
			tm.tm_isdst = 0;

#if	defined(_POSIX_SOURCE) || defined(SYSV) || defined(USG) || defined(__svr4__)
			ndp->ctimet = mktime(&tm);
#else
			ndp->ctimet = timegm(&tm);
#endif
			/* fprintf(stderr,"NDPARSE: CTIMES='%s', -> y=%d m=%d d=%d, %02d:%02d:%02d; timeval=%d\n",
			   ndp->CTIMES,
			   tm.tm_year,tm.tm_mon,tm.tm_mday,
			   tm.tm_hour,tm.tm_min,tm.tm_sec,
			   ndp->ctime); */
		      }
		      goto contentprint;
		  case INMDSNAM:
		      /* Fall thru */
		      rsize = size;
		      if (size > sizeof(ndp->DSNAM)-1)
			rsize = sizeof(ndp->DSNAM)-1;
		      EBCDIC_TO_ASCII(value,ndp->DSNAM,rsize);
		      ndp->DSNAM[rsize] = 0;
		      goto contentprint;
		  case INMMEMBR:
		      rsize = size;
		      if (size > sizeof(ndp->MEMBR)-1)
			rsize = sizeof(ndp->MEMBR)-1;
		      EBCDIC_TO_ASCII(value,ndp->MEMBR,rsize);
		      ndp->MEMBR[rsize] = 0;
		      goto contentprint;
		  contentprint:
		      if (debugdump) {
			EBCDIC_TO_ASCII(value, Aline, size);
			Aline[size] = '\0';
			fprintf(stderr,"%s    value=A'%s'\n", NDP, Aline);
		      }
		      break;

		  default:
		      break;

		} /* end of key analysis */

		size += 6;	/* 6 for key+count+length */
		length -= size; p += size;

	      } /* while (length > 0) */
	      break;

	  case 0xf6:	/* Last INMR - ignore */
	      if (debugdump)
		fprintf(stderr,"NETDATAPARSE: INMR06; EOF; size=%d\n",length+6);
	      return 6;  /* Flag EOF :) */

	  default:
	      break;

	} /* End of INMR0X -analysis */

	return 1;
}


/* Parse the actual record */

static int
parse_netdata_record(flag,buffer,size,ndp,binary,debugdump)
int	size, binary, debugdump;
unsigned char	flag, *buffer;
struct ndparam	*ndp;
{
	int	rc = 0;
	unsigned char	c;

	/* If the 0x20 bit is on, this is a control record. */
	if (debugdump)
	  fprintf(stderr,
		  "NETDATAPARSE: %s record, flag byte: 0x%x, length =%d\n",
		  flag & 0x20 ? "Control":"Data",
		  flag, size);
	if ((flag & 0x20) == 0x20) {
	  if ((rc = parse_net_data_control_record(buffer, size, ndp,
						  debugdump )) <= 0) {
	    fprintf(stderr, "Control record parser exited with rc=%d, RecordsCount=%d\n",
		    rc, ndp->RecordsCount);
	    return 2;
	  }
	} else {		/* Ordinary record */
	  /* if (debugdump) {
	     fprintf(stderr,
	     "NETDATAPARSE: Normal record (X'%02X'), length=%d\n",
	     buffer[0],size);
	     trace(buffer,size,1);
	     } */

	  if (!ndp->outproc && !ndp->outfile) {
	    /* Not to a program, but file is not open either,
	       we have to figure out the filename.
	       The  outname  will be NULL, if user has specified
	       nothing, while  defoutname  has a 8+.+8 string
	       from the ASCII headers. */
	    if (!ndp->outname && ndp->DSNAM[0]) {
	      /* Use DSNAM string for the task! */
	      char *s = ndp->DSNAM;
	      char *m = ndp->MEMBR;
	      char *p = ndp->defoutname;
	      while (*s && *s != ' ') ++s; /* Skip over the first field */
	      while (*s == ' ') ++s;
	      while (*s && *s != ' ')
		*p++ = *s++;
	      if (*s) *p++ = '.';
	      while (*s == ' ')++s;
	      while (*s && *s != ' ')
		*p++ = *s++;
	      if (*m) {
		*p++ = '.';
		while (*m && *m != ' ')
		  *p++ = *m++;
	      }
	      *p = 0;
	      lowerstr(ndp->defoutname);
	    }
	    if (!ndp->outname)
	      ndp->outname = ndp->defoutname;
	    if (strcmp(ndp->outname,"-")==0)
	      ndp->outfile = stdout;
	    else
	      ndp->outfile = fopen(ndp->outname,"w");
	    if (!ndp->outfile) {
	      perror("Open of output file failed!\n");
	      exit(8);
	    }
	  }

	  if (binary) {
	    unsigned char recsize = size;
	    if (ndp->outproc) {
	      ndp->outproc(buffer,size,ndp);
	    } else {
	      if (binary>1)
		fwrite(&recsize,1,1,ndp->outfile);
	      fwrite(buffer,1,size,ndp->outfile);
	    }
	  } else {
	    /* If there is a carriage control character here,
	       handle it: */
	    if (NDcc == MACHINE_CC) {
	      /* Convert machine carriage control to ASA */
	      c = *buffer;	/* Save previous carriage control */
	      *buffer = CarriageControl; /* XX: really here ? */

	      switch (c) {	/* Calculate the next one */
		case 0x1:	/* Write with no space */
		case 0x3:
		    CarriageControl = E_PLUS;
		    break;
		case 0x9:	/* Write and 1 space */
		case 0xb:
		    CarriageControl = E_SP;
		    break;
		case 0x11:	/* Double space */
		case 0x13:
		    CarriageControl = E_0;
		    break;
		case 0x19:	/* Triple space */
		case 0x1b:
		    CarriageControl = E_MINUS;
		    break;
		case 0x8b:	/* Jump to next page */
		case 0x8d:
		    CarriageControl = E_1;
		    break;
		default:	/* Use single space */
		    CarriageControl = E_SP;
		    break;
	      } 
	    } 
	    EBCDIC_TO_ASCII(buffer,buffer,size);
	    if (ndp->outproc) {
	      ndp->outproc(buffer,size,ndp);
	    } else {
	      fwrite(buffer,1,size,ndp->outfile);
	      putc('\n',ndp->outfile);
	    }
	  }
	}
	if (rc == 6) return -1; /* INMR06 -> EOF */
	return 0;
}


/* Combine segments into full ND record */
static int
parse_netdata_segment(buffer, size, ndp, binary, debugdump)
int	size, binary, debugdump;
unsigned char	*buffer;
struct ndparam	*ndp;
{
	if ((buffer[1] & 0xC0) == 0xC0)
	  /* First, AND final segment, just handle it. */
	  return parse_netdata_record(buffer[1],buffer+2,size-2,
				      ndp,binary,debugdump);

	if (buffer[1] & 0x80) {
	  /* The first segment, start composing */
	  SavedNdFlag = buffer[1];
	  memcpy(SavedNdLine,buffer+2,size-2);
	  SavedNdPosition = size-2;

	} else if ((buffer[1] & 0x40) == 0) {
	  /* Not the final segment, compose */
	  memcpy(SavedNdLine+SavedNdPosition,buffer+2,size-2);
	  SavedNdPosition += size-2;

	} else {
	  /* Here must be the final segment */
	  memcpy(SavedNdLine+SavedNdPosition,buffer+2,size-2);
	  SavedNdPosition += size-2;
	  return parse_netdata_record(SavedNdFlag,SavedNdLine,SavedNdPosition,
				      ndp,binary,debugdump);
	}
	return 0;
}

/*
 | Parse NetData files punch records.
 */
int
parse_netdata(buffer, size, ndparam, binary, debugdump)
int	size, binary, debugdump;
unsigned char	*buffer;
struct ndparam	*ndparam;
{
	int seglen, rc = 0;

	/* Compose netdata segments */
	if (ndparam->RecordsCount == 0) {
	  SavedSegmentSize = 0;
	  SavedNdPosition  = 0;
	}
	/* While this input record has data.. */
	while (rc == 0 && size > 0) {

	  /* Figure out this segments size;
	     Rather, how much to read in.. */
	  seglen  = SavedSegmentSize ? SavedNdSegment[0] : buffer[0];
	  seglen -= SavedSegmentSize; /* Decrement what is in */
	  if (size == 1) /* A special case */
	    seglen = 1;
	  if (seglen == 0) {
	    ndparam->RecordsCount = 0; /* Clear it for the next going over.. */
	    return -1; /* EOF */
	  }

	  /* Only as much as there is data.. */
	  if (seglen > size) seglen = size;
	  memcpy(SavedNdSegment + SavedSegmentSize, buffer, seglen);

	  if (SavedNdSegment[1] == 0xf0) /* A record number segment */
	    seglen = SavedSegmentSize ? 1 : 2;

	  /* Advance */
	  SavedSegmentSize += seglen;
	  buffer += seglen;
	  size   -= seglen;

	  if (debugdump) {
	    fprintf(stderr,
		    "NETDATAPARSE: Composing segments; SavedLen=%d, DesiredLen=%d, Flag=X'%02X'\n",
		    SavedSegmentSize,SavedNdSegment[0],SavedNdSegment[1]);
	    /* trace(SavedNdSegment,SavedSegmentSize,1); */
	  }

	  /* If full segment, process it.
	     (Ignore record number segments) */
	  rc = 0;

	  if (SavedNdSegment[0] == SavedSegmentSize ||
	      SavedNdSegment[1] == 0xf0) {
	    if (SavedNdSegment[1] != 0xf0)
	      rc = parse_netdata_segment(SavedNdSegment, SavedSegmentSize,
					 ndparam, binary, debugdump);
	    SavedSegmentSize = 0;
	  }
	} /* while () */
	ndparam->RecordsCount += 1;
	return rc;
}
