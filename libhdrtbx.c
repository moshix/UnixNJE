/*	FUNET-NJE		Client utilities
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 26-Sep-1993
 */


#include "prototypes.h"
#include "clientutils.h"

extern char LOCAL_NAME[];

/*----------------------------------------------------------------------*
 |  Header_Toolbox     Reads FUNET-NJE spool header and analyzes it.    |
 |									|
 |  By  Matti Aarnio <mea@nic.funet.fi>					|
 *----------------------------------------------------------------------*/

/* 0 == ok, !0 == error somewhere */
int
parse_header( fd,Frm,Toa,Fnm,Ext,Typ,Cls,For,Fmt,Tag,Fid )
     FILE *fd;	/* 'fopen()'ed file for reading.  In begin of file
		   When returning, fd points to begin of file data.  */
     /* Rest are for returning values */
     char *Frm, *Toa, *Fnm, *Ext, *Cls, *For, *Tag;
     int  *Typ, *Fid, *Fmt;
{
	char buf[1024];	/* Total ASCII header space reservation.. */
	register char *p,*q;
	int i;
	long filepos;

	/* First, zero out all fields! */
	*Frm = 0; *Toa = 0; *Fnm = 0; *Ext = 0;
	*Cls = 0; *For = 0; *Tag = 0;
	/* From, To, FileName, FileExt, Class, Form */
	*Typ = 0xFF; *Fid = -1; *Fmt = -1;

	for(;;) {
	  filepos = ftell(fd);
	  if( fgets(buf, sizeof buf, fd) == NULL ) return 1; /* Error! */
	  /* Sudden EOF, illegal state here! */

	  if((p = strchr(buf,'\n')) != NULL ) *p = 0; /* Zap \n */
	  p = &buf[4];		/* First char after space */
	  if (*p == ' ') ++p;
	  /* Skip over leading spaces (and I saw null filename which was
	     2-3 spaces and causes RSCS to refuse the file... */
	  while(*p && *p == ' ') ++p;
	  /* And hunt over next token, until find first space or NULL */
	  /* Some data fields have fixed SPACE padded size - to help to
	     rewrite them on reroute.   This parser must drop off those
	     spaces when analysing... */
	  q = buf + strlen(buf) -1;
	  while(q > p && (*q == ' ')) --q;
	  if (++q > p && *q == ' ') *q = 0; /* Stomp it! */

	  if (strncmp("FRM:",buf,4)==0) {
	    if((strlen(p) >= 20) || (*p == '\0'))
	      sprintf(Toa, "MAILER@%s",LOCAL_NAME);
	    else
	      strcpy(Frm, p);
	  } else if (strncmp("TOA: ",buf,5)==0) {
	    strcpy(Toa,p);
	  } else if (strncmp("FNM:",buf,4)==0) {
	    if(*p != '\0') {
	      p[12] = '\0';	/* Delimit length */
	      strcpy(Fnm, p);
	    }
	  } else if (strncmp("EXT:",buf,4)==0) {
	    if(*p != '\0') {
	      p[12] = '\0';	/* Delimit length */
	      strcpy(Ext, p);
	    }
	  } else if (strncmp("TYP:",buf,4)==0) {
	    if (strcasecmp(p, "JOB") == 0) {
	      *Typ = F_JOB;
	    } else if ((strcasecmp(p, "MAIL") == 0) ||
		(strcasecmp(p, "BSMTP") == 0)) {
	      *Typ = F_MAIL;
	      *Cls = 'M';	/* M class */
	    } else { /* It is a file - parse the format */
	      *Cls = 'A';	/* A class */
	      if (strcasecmp(p, "PRINT") == 0)
		*Typ = F_PRINT;
	      else if (strcasecmp(p, "PUNCH") == 0)
		*Typ = F_PUNCH;
	      else if (strcasecmp(p, "PASA") == 0)
		*Typ = F_ASA | F_PRINT;
	      else if (strcasecmp(p, "FASA") == 0)
		*Typ = F_ASA;
	      else
		*Typ = F_FILE;	/* Default */
	    }
	  } else if (strncmp("CLS:",buf,4)==0) {
	    *Cls = *p;
	  } else if (strncmp("FOR:",buf,4)==0) {
	      p[8] = '\0';
	      strcpy(For, p);
	      if( strncmp("QU",p,2) != 0 )
		*Typ |= F_NOQUIET;
	      else
		*Typ &= ~F_NOQUIET;
	  } else if (strncmp("FMT:",buf,4)==0) {
	    if(strcmp(p,"BINARY")==0)
	      *Fmt = BINARY;
	    else
	      *Fmt = EBCDIC;  /* DEFAULT */
	  } else if (strncmp("FID:",buf,4)==0) {
	    i = atoi(p);
	    if((i > 0) && (i <= 9900))	/* Inside the range */
	      *Fid = (short)(i);
	  } else if (strncmp("OID:",buf,4)==0) {
	    /* Internal for transport.. */
	  } else if (strncmp("DIS:",buf,4)==0) {
	    /* Well, nothing.. RSCS DIST */
	  } else if (strncmp("JNM:",buf,4)==0) {
	    /* Well, nothing.. NJH JOBNAME */
	  } else if (strncmp("PUN:",buf,4)==0) {
	    /* Well, nothing.. NJH PUNCH -- for SYSIN especially */
	  } else if (strncmp("PRT:",buf,4)==0) {
	    /* Well, nothing.. NJH PRINT -- for SYSIN especially */
	  } else if (strncmp("VIA:",buf,4)==0) {
	    /* Well, nothing.. */
	  } else if (strncmp("REC:",buf,4)==0) {
	    /* Well, nothing.. */
	  } else if (strncmp("TAG:",buf,4)==0) {
	    p[136] = '\0';	/* Delimit length */
	    strcpy(Tag, p);
	  } else if (strcmp("END:",buf)==0) {
	    return 0;	/* All ok. */
	  } else {
	    fprintf(stderr, "parse_header(): Mystic tag: `%-8.8s...',  Line start offset=%ld\n", buf, filepos);
	    return 1;	/* Some error! */
	  }
	}
}
