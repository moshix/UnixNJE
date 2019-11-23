/* SEND_FILE.C		V2.1/mea-1.3
 | Copyright (c) 1988,1989,1990,1991 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.
 |
 | Send a file to the other side. This module fills a buffer and send it to
 | the other side.

 | V2.1-mea - 14/9/93 - Rip out the Netdata processing from the transport!
 |	  If format == EBCDIC, let all kinds of records thru, else (that is,
 |	  with  format == BINARY) strip out NJH, DSH, and NJT -blocks.
 |	  Later one simplifies user-initiated file transfer.
 |	- 5/10/93 - Moved  send_njh_dsh_record(), and send_njt() into here.
 |
 | Document:
 |   Network Job Entry - Formats and Protocols (IBM)
 |	SC23-0070-02
 |	GG22-9373-02 (older version)
 */

#include "consts.h"
#include "headers.h"
#include "prototypes.h"

EXTERNAL struct JOB_HEADER	NetworkJobHeader;
EXTERNAL struct DATASET_HEADER	NetworkDatasetHeader;
EXTERNAL struct JOB_TRAILER	NetworkJobTrailer;


/* Macro to estimate the length of the line after compression. Since we do not
   compress repeatitive characters, it is simply the number of characters
   divided by 63 (the number of added SCBs) + 5 (=RCB+SRCB+length count+
   final-SCB + 1[round-up]).
*/
#define	EXPECTED_SIZE(LENGTH)	(LENGTH + (int)(LENGTH / 63) + 5)


/*
 | Send Punch or Print files (mails are punch files).
 | Fill the buffer and send it. If there is no room for a line in the buffer,
 | it is saved to the next call to this routine in a
 | static buffer.
 */
void
send_file_buffer(Index)
const int	Index;
{
	register int	MaxSize, CurrentSize, i;
	unsigned char	OutputLine[MAX_BUF_SIZE],
			InputBuffer[MAX_BUF_SIZE];
	struct	FILE_PARAMS	*FileParams;
	int	Stream, SYSIN;
	struct	LINE	*Line;

	Line = &IoLines[Index];
	Stream = Line->CurrentStream;

	FileParams = &((Line->OutFileParams)[Stream]);
	SYSIN = FileParams->type & F_JOB;

	/* retrieve the buffer size from the lines database */

	MaxSize = Line->MaxXmitSize;
	/* Leading BCB, FCS, DLE+..., trailing PAD, 2*crc, DLE+ETB, last SCB,
	   last RCB */
	MaxSize -= 14;

	CurrentSize = 0;	/* Empty output buffer */

	/* Fill the buffer until there is no place in it */
	while ((i = uread(Index, Stream,
			  &InputBuffer[1], sizeof(InputBuffer) - 1)) > 0) {

	  /* Put into the current block if there is space for it */

	  MaxSize -= EXPECTED_SIZE( i );
	  if (MaxSize >= 0) {

	    int InpSRCB = InputBuffer[1] & 0xf0;

	    if (InpSRCB == NJH_SRCB ||
		InpSRCB == DSH_SRCB ||
		InpSRCB == NJT_SRCB) {

	      /* If we are not running transparent Store & Forward,
		 we have to strip spurious NJH, DSH, and NJT headers */
	      if (FileParams->format != EBCDIC) {
		i = 0; /* Discard it.. */
		continue;
	      }

	      /* The NJH, DSH, and NJT - they must be the only
		 record in their block */

	      if (CurrentSize != 0)
		/* If we already put there something */
		break;
	    }

	    if (FileParams->RecordsCount >= 0)
	      FileParams->RecordsCount += 1;
	    else
	      FileParams->RecordsCount -= 1;

	    /* there is room for this string */
	    if (SYSIN)
	      OutputLine[CurrentSize++] = ((Stream + 9) << 4) | 0x8;
	    else
	      OutputLine[CurrentSize++] = ((Stream + 9) << 4) | 0x9;

	    /* SRCB is the second character in the saved string. */

	    OutputLine[CurrentSize++] = InpSRCB;

	    CurrentSize += compress_scb(&InputBuffer[2],
					&OutputLine[CurrentSize],
					i-1);

	    i = 0;	/* Stored nicely, mark it so */

	    if (InpSRCB == NJH_SRCB ||
		InpSRCB == DSH_SRCB ||
		InpSRCB == NJT_SRCB) {

	      /* The NJH, DSH, and NJT - they must be the only
		 record in their block */

	      break;
	    }
	  } else
	    break;		/* No room - abort loop */
	}

	/* If not stored nicely, rewind to the begining of the record */
	if (i > 0)
	  fseek(Line->InFds[Stream], -i-2, 1);

	/* See if it was an error! */
	if (i == -2) {
	  /* XXX: [mea] do  Less Reliable Transport Service  sender cancel
	                here...  SC23-0070-02: pp. 4-6 - 4-7   */
	  /* XXX: [mea]  Should cause transmission abort,
			 and move file into error area (postmast).. */
	  bug_check("missing LRTS sender cancel code!");
	}
	/* Now see if it was an EOF */
	if (i == -1) {
	  if (FileParams->format != EBCDIC)
	    /* We will have to generate, and send the NJT */
	    Line->OutStreamState[Stream] = S_EOF_FOUND;
	  else
	    /* NJT was sent already as part of file */
	    Line->OutStreamState[Stream] = S_NJT_SENT;
	  Line->OutFileParams[Stream].FileSize = ftell(Line->InFds[Stream]);
logger(2,"SEND_FILE: EOF on line %s stream %d\n",Line->HostName,Stream);
	}

	logger(4,"SEND_FILE: Built an output buffer, CurrentSize=%d, MaxSize=%d, loop condition var=%d\n",CurrentSize,MaxSize,i);
	

	/* Send the buffer filled */
	if (CurrentSize > 0) {	/* Something was filled in buffer - send it */
	  /* Add EOR RCB */
	  OutputLine[CurrentSize++] = 0;
	  send_data(Index, OutputLine, CurrentSize, ADD_BCB_CRC);
	}
	return;
}



/*
 | Send the Network Job headers record or the Dataset header.
 | If the message is of type FILE, the class in DSH is set to A; if it is
 | MAIL, that class is set to M. However, if the user explicitly specified
 | another class, we use that class.
 | The class fields in NJH are set to A.
 | If a job number was assigned by the user's agent - use it. If not - assign
 | one of ourself.
 | The maximum record length stored in NDH should be read from FAB.
 */
#define CREATE_NJH_FIELD(FIELD, USER, NODE) {\
	strcpy(TempLine, FIELD); \
	upperstr(TempLine); \
	if((p = strchr(TempLine, '@')) != NULL) *p++ = '\0'; \
		/* Separate the sender and receiver */ \
	else p = TempLine; \
	p[8] = '\0';	/* Delimit to 8 characters */ \
	i = strlen(p); \
	ASCII_TO_EBCDIC(p, NODE, i); \
	PAD_BLANKS(NODE, i, 8); \
	TempLine[8] = '\0'; \
	if(strcmp(TempLine, "SYSTEM") == 0) \
		*TempLine = '\0';	/* IBM forbids SYSTEM */ \
	i = strlen(TempLine); \
	ASCII_TO_EBCDIC(TempLine, (USER), i); \
	PAD_BLANKS((USER), i, 8); }

int
send_njh_dsh_record(Index, flag, SYSIN)
const int	Index, flag, SYSIN;
{
	struct	LINE	*Line;
	struct	FILE_PARAMS	*FileParams;
	int	i, TempVar;	/* For CREATE_NJH... macro */
	int	NDHsize = 0, NDHRSCSsize = 0;
	int	NDHpriority = 50;
	short	Ishort;
	char		*p, Afield[10],
			TempLine[350];	/* For CREATE_NJH... macro */
	unsigned char	FromUser[10], FromNode[10],
			ToUser[10], ToNode[10],
			FileName[16], FileExt[16];
	int		SerialNumber,	/* We have to divide xmission */
			SizeSent;	/* How much of DSH we already sent */
	unsigned char	SmallTemp[32],	/* For composing small buffers */
			*NetworkDatasetHeaderPointer;	/* For easy manipulations */

	Line = &(IoLines[Index]);
	FileParams = &(Line->OutFileParams[Line->CurrentStream]);

	logger(3,"SEND_FILE: send_njh_dsh_record(%s:%d, flag=%d, SYS%s)\n",
	       Line->HostName,Line->CurrentStream,flag,SYSIN?"IN":"OUT");

	/* SYSINs don't have DSH -- "Simulate" it! */
	if (flag != SEND_NJH && SYSIN) return 0;

	/* ================ Common bits ================ */

	/* Prepare the username and site name in EBCDIC,
	   blanks padded to 8 characters */
	CREATE_NJH_FIELD(FileParams->From, FromUser, FromNode);
	CREATE_NJH_FIELD(FileParams->To,   ToUser,   ToNode);

	if (SYSIN && ToUser[0] == E_SP)
	  memset(ToUser,0,8);

	i = strlen(FileParams->FileName);
	upperstr(FileParams->FileName);
	ASCII_TO_EBCDIC(FileParams->FileName, FileName, i);
	PAD_BLANKS(FileName, i, 12);
	i = strlen(FileParams->FileExt);
	upperstr(FileParams->FileExt);
	ASCII_TO_EBCDIC(FileParams->FileExt, FileExt, i);
	PAD_BLANKS(FileExt, i, 12);


	if (flag == SEND_NJH) {
	  /* ================  NJH  ================ */

	  /* Fill the NJH record */

	  if (FileParams->FileId == 0)
	    FileParams->FileId = FileParams->OurFileId;
	  NetworkJobHeader.NJHGJID = htons(FileParams->OurFileId);
	  sprintf(TempLine, "NJE_%04ld", FileParams->OurFileId); /* Job name */

	  /* For informational purpose */
	  memcpy(FileParams->JobName, TempLine, 9);
	  ASCII_TO_EBCDIC(TempLine, NetworkJobHeader.NJHGJNAM, 8);

	  /* NJH header */
	  /* Log the transaction */
	  /* logger(3, "=> Sending file/mail %s.%s from %s to %s (line %s)\n",
	     FileParams->FileName, FileParams->FileExt,
	     FileParams->From, FileParams->To, Line->HostName);		*/
	  /* Insert the time in IBM format */
	  ibm_time(NetworkJobHeader.NJHGETS);
	  memcpy(NetworkJobHeader.NJHGUSID, FromUser, 8);
	  memcpy(NetworkJobHeader.NJHGORGN, FromNode, 8);
	  memcpy(NetworkJobHeader.NJHGORGR, FromUser, 8);
	  if (SYSIN) {
	    /* For SYSIN the NJHGXEQ[NU] are the EXECUTIONER */
	    memcpy(NetworkJobHeader.NJHGXEQN, ToNode, 8);
	    memcpy(NetworkJobHeader.NJHGXEQU, ToUser, 8);
	  } else {
	    /* For SYSOUT the NJHGXEQ[NU] are SENDER */
	    memcpy(NetworkJobHeader.NJHGXEQN, FromNode, 8);
	    memcpy(NetworkJobHeader.NJHGXEQU, FromUser, 8);
	  }

	  /* Now recycle the FromNode/FromUser variables */

	  /* Following two are usually the same as "From"... */
	  CREATE_NJH_FIELD(FileParams->PrtTo, FromUser, FromNode);
	  memcpy(NetworkJobHeader.NJHGPRTN, FromNode, 8);
	  memcpy(NetworkJobHeader.NJHGPRTR, FromUser, 8);

	  CREATE_NJH_FIELD(FileParams->PunTo, FromUser, FromNode);
	  memcpy(NetworkJobHeader.NJHGPUNN, FromNode, 8);
	  memcpy(NetworkJobHeader.NJHGPUNR, FromUser, 8);

	  if (SYSIN)
	    NetworkJobHeader.NJHGFLG1 = 0x08;
	  else
	    NetworkJobHeader.NJHGFLG1 = 0x0c;
	  

	} else {
	  /* ================  DSH header  ================ */

	  if (flag != SEND_DSH2) {
	    /* Don't generate this front part for frag 2 */

	    /* Fill the general section */
	    memcpy(NetworkDatasetHeader.NDH.NDHGNODE, ToNode, 8);
	    memcpy(NetworkDatasetHeader.NDH.NDHGRMT,  ToUser, 8);
	    memcpy(NetworkDatasetHeader.NDH.NDHGPROC, FileName, 8);
	    memcpy(NetworkDatasetHeader.NDH.NDHGSTEP, FileExt,  8);

	    /* Set the maximum record length */
	    if ((FileParams->type & F_PRINT) != 0) { /* Print format = 132 */
	      NetworkDatasetHeader.NDH.NDHGLREC = htons(132);
	      NetworkDatasetHeader.NDH.NDHGFLG2  = 0x80; /* Print flag */
	      NetworkDatasetHeader.RSCS.NDHVIDEV = 0x41; /* One of the
							    IBM printers  */
	      memset(NetworkDatasetHeader.NDH.NDHGXWTR, 0, 8);
	    } else {		/* All others will be 80 at present */
	      memcpy(NetworkDatasetHeader.NDH.NDHGXWTR, ToUser, 8);
	      NetworkDatasetHeader.NDH.NDHGLREC = htons(80);
	      NetworkDatasetHeader.NDH.NDHGFLG2  = 0x40; /* Punch flag */
	      NetworkDatasetHeader.RSCS.NDHVIDEV = 0x82; /* Punch file */
	      /* why not: 0x81 ?? [mea] */
	    }

	    NetworkDatasetHeader.NDH.NDHGCLAS = ASCII_EBCDIC[(int)FileParams->JobClass];

	    /*  If  FormsName  is defined, use it for this, if not,
		set the form code to QUIET if bit F_NOQUIET is not set.	*/
	    memcpy(NetworkDatasetHeader.NDH.NDHGFORM, EightSpaces, 8);
	    if (*FileParams->FormsName != 0) {
	      ASCII_TO_EBCDIC(FileParams->FormsName,
			      NetworkDatasetHeader.NDH.NDHGFORM,
			      strlen(FileParams->FormsName));
	    } else if ((FileParams->type & F_NOQUIET) == 0) {
	      strcpy(Afield, "QUIET");
	      ASCII_TO_EBCDIC(Afield, NetworkDatasetHeader.NDH.NDHGFORM,
			      strlen(Afield));
	    }

	    /* Fill in the Dataset header record count -- we DON'T KNOW IT!
	       Except that we get it from the ASCII header.. */
	    if (FileParams->RecordsCount > 0)
	      NetworkDatasetHeader.NDH.NDHGNREC = htonl(FileParams->RecordsCount);
	    else
	      NetworkDatasetHeader.NDH.NDHGNREC = htonl(1);

	    /* Fill the RSCS section */
	    NetworkDatasetHeader.RSCS.NDHVCLAS = ASCII_EBCDIC[(int)FileParams->JobClass];
	    memcpy(NetworkDatasetHeader.RSCS.NDHVFNAM, FileName, 12);
	    memcpy(NetworkDatasetHeader.RSCS.NDHVFTYP, FileExt, 12);

	    i = strlen(FileParams->DistName);
	    if (i > 8) i = 8;
	    upperstr(FileParams->DistName);
	    ASCII_TO_EBCDIC(FileParams->DistName, SmallTemp, i);
	    PAD_BLANKS(SmallTemp, i, 8);
	    memcpy(NetworkDatasetHeader.RSCS.NDHVDIST, SmallTemp, 8);

	  }
	  /* When fragmenting, chops in the middle of RSCS TAG.. */
	  /* Store the TAG field (or not, if empty..) */
	  i = strlen(FileParams->tag);
	  if (i > 0) {
	    ASCII_TO_EBCDIC(FileParams->tag,
			    NetworkDatasetHeader.RSCS.NDHVTAGR, i);
	    PAD_BLANKS((NetworkDatasetHeader.RSCS.NDHVTAGR), i, 136);
	    NDHsize = sizeof(NetworkDatasetHeader);
	    NDHRSCSsize = sizeof(NetworkDatasetHeader.RSCS);
	  } else {
	    NDHsize = (sizeof(NetworkDatasetHeader) -
		       sizeof(NetworkDatasetHeader.RSCS.NDHVTAGR));
	    NDHRSCSsize = (sizeof(NetworkDatasetHeader.RSCS) -
			   sizeof(NetworkDatasetHeader.RSCS.NDHVTAGR));
	  }

	  /* Should these *_4's be the current value  -4 ?
	     NJH, and NJT leads towards this assumtion,
	     but several tests claim opposite!		  */

	  NetworkDatasetHeader.LENGTH        = htons(NDHsize);
	  NetworkDatasetHeader.NDH.LENGTH_4  = htons(sizeof(struct DATASET_HEADER_G));
	  NetworkDatasetHeader.RSCS.LENGTH_4 = htons(NDHRSCSsize);
	  NetworkDatasetHeader.RSCS.NDHVPRIO = htons(NDHpriority);

	}

	/* Send the data to the other side */
	TempVar = 0;

	if (flag == SEND_NJH) {
	  if (SYSIN)
	    TempLine[TempVar++] = (((Line->CurrentStream + 9) << 4) | 0x8);
	  else
	    TempLine[TempVar++] = (((Line->CurrentStream + 9) << 4) | 0x9);
	  TempLine[TempVar++] = NJH_SRCB;
	  TempVar += compress_scb(&NetworkJobHeader, &(TempLine[2]),
				  (sizeof(struct JOB_HEADER)));
	  TempLine[TempVar++] = 0; /* Closing SCB */
	  /* Send  as usual */
	  send_data(Index, TempLine, TempVar, ADD_BCB_CRC);
	  return 0;		/* Sent OK */
	}

	/* We process the DSH now */

	/* The DSH content may be large enough to need fragmenting. If so,
	   we fragment.  The DSH  TAG  data may get re-generated while
	   we are at the task, but that is a small penalty for genericity
	   of this routine.						*/

	NetworkDatasetHeaderPointer = (unsigned char*)&NetworkDatasetHeader;

	/* Have to fragment the entry */
	/* Jump over the first 4 bytes as we rebuild them now */
	NetworkDatasetHeaderPointer += 4;
	SizeSent = 4;	/* 4 - since we re-generate the first 4 bytes */
	SerialNumber = 0;

	/* Point to the second fragment, and proceed as with the first.. */
	if (flag == SEND_DSH2) {
	  NetworkDatasetHeaderPointer += 252;
	  SizeSent += 252;
	  SerialNumber += 1;
	}
	/* When there is something to send */
	i = 0; /* If  i > 252,  then more fragments will follow */
	if (SizeSent <= NDHsize) {
	  TempVar = 0;
	  if (SYSIN)
	    TempLine[TempVar++] = (((Line->CurrentStream + 9) << 4) | 0x8);
	  else
	    TempLine[TempVar++] = (((Line->CurrentStream + 9) << 4) | 0x9);
	  TempLine[TempVar++] = DSH_SRCB;
	  /* If the size if smaller than 252 then this is the last fragment */
	  if ((i = (NDHsize - SizeSent)) <= 252) {
	    Ishort = htons(i + 4);
	    memcpy(SmallTemp, &Ishort, 2);
	    SmallTemp[2] = 0;
	    SmallTemp[3] = SerialNumber; /* Last one */
	    TempVar += compress_scb(SmallTemp, &(TempLine[TempVar]), 4);
	    --TempVar;		/* Remove the closing SCB */
	    TempVar += compress_scb(NetworkDatasetHeaderPointer,
				    &(TempLine[TempVar]), i);
	    TempLine[TempVar++] = 0; /* Closing SCB */
	  } else {
	    Ishort = htons(256); /* 252 payload + 4 header */
	    memcpy(SmallTemp, &Ishort, 2);
	    SmallTemp[2] = 0;
	    SmallTemp[3] = 0x80 + (SerialNumber++);
	    TempVar += compress_scb(SmallTemp, &(TempLine[TempVar]), 4);
	    --TempVar;		/* Remove the closing SCB */
	    TempVar += compress_scb(NetworkDatasetHeaderPointer,
				    &(TempLine[TempVar]), 252);
	    TempLine[TempVar++] = 0; /* Closing SCB */
	  }			
	  Line->flags |= F_XMIT_CAN_WAIT; /* We support TCP lines here */
	  send_data(Index, TempLine, TempVar, ADD_BCB_CRC); /* Send  as usual */
	  Line->flags &= ~F_XMIT_CAN_WAIT;
	}
	return (i > 252);
}


/*
 | Send the job trailer. Add the count of lines in the file.
 */
void
send_njt(Index, SYSIN)
const int	Index, SYSIN;
{
	unsigned char	OutputLine[LINESIZE];
	int	TempVar, position;
	struct	LINE	*Line;

	Line = &IoLines[Index];

	logger(3,"SEND_FILE: send_njt(%s:%d, SYS%s)\n",
	       Line->HostName,Line->CurrentStream, SYSIN?"IN":"OUT");

	/* Since number of lines not written - send anyway */
	position = 0;
	if (SYSIN)
	  OutputLine[position++] = (((Line->CurrentStream + 9) << 4) | 0x8);	/* RCB */
	else
	  OutputLine[position++] = (((Line->CurrentStream + 9) << 4) | 0x9);	/* RCB */
	OutputLine[position++] = NJT_SRCB;	/* SRCB */

	TempVar = Line->OutFileParams[Line->CurrentStream].RecordsCount;

	if (TempVar > 0)
	  TempVar = htonl(TempVar);
	else
	  TempVar = htonl(1);

	memcpy(&(NetworkJobTrailer.NJTGALIN),
	       &TempVar, sizeof(NetworkJobTrailer.NJTGALIN));
	memcpy(&(NetworkJobTrailer.NJTGCARD),
	       &TempVar, sizeof(NetworkJobTrailer.NJTGCARD));

	position += compress_scb(&NetworkJobTrailer, &OutputLine[position],
			(sizeof(struct JOB_TRAILER)));
	OutputLine[position++] = 0;	/* End of block */
	send_data(Index, OutputLine, position, ADD_BCB_CRC);
}
