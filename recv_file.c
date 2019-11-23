/* RECV_FILE.C	V3.0-mea1.0
 | Copyright (c) 1988,1989,1990,1991,1992 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.

 | Copyright (c) 1993,1994 by
 | Finnish University and Research Network, FUNET.
 |
 | The whole system got serious rewrite when spool handling got
 | adapted in `our' manner..  There is not much left of the original..

 */
#include "consts.h"
#include "headers.h"
#include "prototypes.h"

static void save_header __(( const int SRCB, const void *buffer, const int BufferSize, const int Index, const int Stream ));
static int finish_file __(( const int Index, const int Stream, StreamStates *StreamState, const int SYSIN ));
static int parse_headers __((const int Index, const int Stream, const int WriteIt ));
static void file_sender_abort __(( const int Index, const int Stream, const int SYSIN ));
static void abort_file __(( const int Index, const int Stream, const int SYSIN ));

EXTERNAL struct	COMPLETE_FILE	CompleteFile;
EXTERNAL struct	REJECT_FILE	RejectFile;
EXTERNAL struct	EOF_BLOCK	EOFblock;



/* [mea]
 | Do header opening things, especially
 | the prefix space allocation.
 | All localized into this module
 */
int
recv_file_open(Index, Stream)
const int Index;
const int Stream;
{
	int rc;
	FILE *fd;
	char buf[512];

	rc = open_recv_file(Index, Stream);
	if (!rc) return 0;

	fd = IoLines[Index].OutFds[Stream];
	/* We write a space reserver header, and return to it later
	   to write the actual NJH/DSH information as it has been
	   gathered from the line */

	memset(buf,' ',512);
	memcpy(buf+512-6,"\nEND:\n",6);
	buf[0] = '*';
	fwrite(buf,512,1,fd);	/* Prefix size: 512 bytes */

	IoLines[Index].OutFileParams[Stream].RecordsCount = 0;

	return 1;
}


/*
 | Parse one record. This routine returns either 0 or 1. 0 is returned when
 | the main logic should not send ACK, since we sent here some record. 1 is
 | returned when we received something which should be acked normally.
 */
int
receive_file(Index, Buffer, BufferSize)
const int	Index, BufferSize;
const void	*Buffer;
{
	/* The state of the stream we handle now */
	StreamStates	*StreamState;
	unsigned char *buffer = (unsigned char *)Buffer;
	struct LINE *Line = &IoLines[Index];

	int	Stream;
	int	SYSIN = 0;
	struct	FILE_PARAMS	*FileParams;
	int	SRCB, RCB;

	RCB  = buffer[0];
	SRCB = buffer[1];

	Stream = ((RCB & 0xf0) >> 4) - 9;	/* Stream number
						   in the range 0-7 */

	/* logger(3,"RECV_FILE: receive_file(Line=%s:%d, BufSize=%d)\n",
	   Line->HostName, Stream, BufferSize);
	   trace(Buffer,BufferSize > 8 ? 8 : BufferSize,3); */


	/* SYSINs are  X'N8', SYSOUTs are X'N9' */
	SYSIN = (RCB & 0x0f) == 8;

	FileParams = &(Line->InFileParams[Stream]);
	if (SYSIN) FileParams->type |= F_JOB;	/* Flag it as a SYSIN job! */

	/* Check that the stream number is in range */
	if ((Stream < 0) ||
	    (Stream >= MAX_STREAMS )) {
	  logger(1, "RECV_FILE: Found illegal RCB=x^%x (line=%s:%d, maxstreams=%d)\n",
		 RCB, Line->HostName, Stream,
		 Line->MaxStreams);
	  file_sender_abort(Index, Stream, SYSIN);
	  return 0;		/* Abort-file already sent some record */
	}

	/* Test whether we got an abort file (SCB = 0x40).
	   If so, the Uncompress_SCB routine returns -1.
	   The code that calls us adds 2 to the value returned from
	   Uncompress_SCB, so if we get a record length < 2, then
	   we know this is abort.					*/

	if (BufferSize < 2) {
	  file_sender_abort(Index, Stream, SYSIN);
	  return 0;	/* Abort-file already sent some record */
	}

	/* Get the relevant data from the I/O structure */
	StreamState = &Line->InStreamState[Stream];

	/* Test whether it is EOF block.
	   If so - it signals end of transmission */
	if (BufferSize == 2)	/* This is the empty block */
	  return finish_file(Index, Stream, StreamState, SYSIN);

	/* This was a non-EOF record. Check the SRCB and act appropriately */
	switch (SRCB & 0xf0) {
	      /* The headers */
	      /* Save the headers first, come back to them later.. */
	  case NJH_SRCB:		/* Job header */
	      if (*StreamState != S_REQUEST_SENT &&
		  *StreamState != S_NJH_SENT) {
		logger(1,"RECV_FILE: Line=%s:%d, Job header arrived when in state %s\n",
		       Line->HostName, Stream, StreamStateStr(*StreamState));
		restart_channel(Index);
		return -1;
	      }
	      *StreamState = S_NJH_SENT;

	      save_header(SRCB, buffer+2, BufferSize-2, Index, Stream);
	      /* Save the record */
	      if (uwrite(Index, Stream, buffer+1, BufferSize-1) == 0) {
		logger(1, "RECV_FILE: Aborting incoming file on %s:%d because uwrite() of %d bytes failed\n",
		       Line->HostName, Stream, BufferSize-1);
		abort_file(Index, Stream, SYSIN);
		return 0;
	      }
	      return 1;
	      break;
	  case DSH_SRCB:		/* Dataset header */
	      if (*StreamState != S_NJH_SENT &&
		  *StreamState != S_NDH_SENT &&
		  *StreamState != S_RECEIVING_FILE) {
		logger(1,"RECV_FILE: Line=%s:%d, A DatasetHeader arrived when in state %s\n",
		       Line->HostName, Stream, StreamStateStr(*StreamState));
		restart_channel(Index);
		return -1;
	      }
	      *StreamState = S_NDH_SENT;

	      save_header(SRCB, buffer+2, BufferSize-2, Index, Stream);
	      /* Save the record */
	      if (uwrite(Index, Stream, buffer+1, BufferSize-1) == 0) {
		logger(1, "RECV_FILE: Aborting incoming file on %s:%d because uwrite() of %d bytes failed\n",
		       Line->HostName, Stream, BufferSize-1);
		abort_file(Index, Stream, SYSIN);
		return 0;
	      }
	      return 1;
	      break;
	  case NJT_SRCB:		/* Job Trailer */
	      if (*StreamState != S_RECEIVING_FILE) {
		logger(1,"RECV_FILE: Line=%s:%d, A JobTrailer arrived when in state %s\n",
		       Line->HostName, Stream, StreamStateStr(*StreamState));
		restart_channel(Index);
		return -1;
	      }
	      *StreamState = S_NJT_SENT;

	      save_header(SRCB, buffer+2, BufferSize-2, Index, Stream);
	      /* Save the record */
	      if (uwrite(Index, Stream, buffer+1, BufferSize-1) == 0) {
		logger(1, "RECV_FILE: Aborting incoming file on %s:%d because uwrite() of %d bytes failed\n",
		       Line->HostName, Stream, BufferSize-1);
		abort_file(Index, Stream, SYSIN);
		return 0;
	      }
	      return 1;
	      break;

	      /* The records themselves */
	case CC_NO_SRCB:
	case CC_MAC_SRCB:
	case CC_ASA_SRCB:
	case CC_CPDS_SRCB:
	      if ((*StreamState == S_NDH_SENT) || (*StreamState == S_NJH_SENT))
		/* We got either both or just NJH. The latter case happens
		   in SYSIN files. */
		*StreamState = S_RECEIVING_FILE;
	      if (*StreamState != S_RECEIVING_FILE) {
		logger(1, "RECV_FILE: line=%s:%d, Illegal state=%s while receiving record\n",
		       Line->HostName, Stream, StreamStateStr(*StreamState));
		restart_channel(Index);
		return 0;
	      }
	      /* Save the record */
	      if (uwrite(Index, Stream, buffer+1, BufferSize-1) == 0) {
		logger(1, "RECV_FILE: Aborting incoming file on %s:%d because uwrite() of %d bytes failed\n",
		       Line->HostName, Stream, BufferSize-1);
		abort_file(Index, Stream, SYSIN);
		return 0;
	      }
	      FileParams->RecordsCount += 1;
	      return 1;
	default:
	      logger(1, "RECV_FILE: Line=%s:%d, Illegal SRCB=x^%x\n",
		     Line->HostName, Stream, SRCB);
	      restart_channel(Index);
	      return 0;	/* No explicit ACK */
	}
}


/* [mea]
 | Save header (see SRCB) into proper slot for later analysis.  If
 | fragmented, like DSH could be (RSCS TAG), combine them here. (XX: combine!)
 | Do not worry about the place/phase of protocol in this module.
 */

static void
save_header(SRCB, buffer, BufferSize, Index, Stream)
const int SRCB;
const void *buffer;
const int BufferSize, Index, Stream;
{
	struct LINE	*Line = &IoLines[Index];
	char	*bufp = NULL;
	int	*lenp = NULL;
	int	savesize;

	switch (SRCB & 0xf0) {
	  case NJH_SRCB:
	      /* NETWORK JOB HEADER -- fragmented ??? */
	      bufp = (char*)Line->SavedJobHeader[Stream];
	      savesize = sizeof(Line->SavedJobHeader[Stream]);
	      lenp = &(Line->SizeSavedJobHeader[Stream]);
	      if (!*lenp) {	/* Ok, save into statically allocated buffer */
		memset(bufp,0,savesize);
		if (BufferSize < savesize)
		  savesize = BufferSize;
		memcpy(bufp,buffer,savesize);
		*lenp = BufferSize;
	      } else {
		/* This is the second fragment -- should be anyway.. */
		bufp += *lenp; /* Advance somewhat */
		savesize -= *lenp;
		if (savesize > 0) {
		  /* Save the fragment if it fits in.. */
		  logger(1,"** RECV_FILE: NETWORK JOB HEADER SECOND FRAGMENT RECEIVED?? Wow!  Line=%s:%d, total size=%d\n",
			 Line->HostName,Stream, *lenp+savesize-4);
		  memcpy(bufp,(char*)(buffer)+4,savesize-4);
		  *lenp += (savesize-4);
		}
	      }
	      break;
	  case DSH_SRCB:
	      /* DATASET_HEADER - possibly fragmented.. */
	      bufp = (char*)Line->SavedDatasetHeader[Stream];
	      savesize = sizeof(Line->SavedDatasetHeader[Stream]);
	      lenp = &(Line->SizeSavedDatasetHeader[Stream]);
	      if (!*lenp) {	/* Ok, save into statically allocated buffer */
		memset(bufp,0,savesize);
		if (BufferSize < savesize)
		  savesize = BufferSize;
		memcpy(bufp,buffer,savesize);
		*lenp = BufferSize;
	      } else {
		/* This is the second fragment -- should be anyway.. */
		bufp += *lenp; /* Advance somewhat */
		savesize -= *lenp;
		if (savesize > 0) {
		  /* Save the fragment if it fits in.. */
		  if (((unsigned char *)buffer)[3] != 1) {
		    /* logger(1,"RECV_FILE: DSH second fragment received with bad serial number: X'%02X'\n",
		       ((unsigned char *)buffer)[3]); */
		  } else {
		    memcpy((void*)bufp,(char*)(buffer)+4,savesize-4);
		    *lenp += (savesize-4);
		  }
		}
	      }
	      break;
	  case NJT_SRCB:
	      /* NETWORK JOB TRAILER */
	      bufp = (char*)Line->SavedJobTrailer[Stream];
	      savesize = sizeof(Line->SavedJobTrailer[Stream]);
	      lenp = &(Line->SizeSavedJobTrailer[Stream]);
	      if (!*lenp) {	/* Ok, save into statically allocated buffer */
		memset(bufp,0,savesize);
		if (BufferSize < savesize)
		  savesize = BufferSize;
		memcpy(bufp,buffer,savesize);
		*lenp = BufferSize;
	      } else {
		/* Whoops -- Defragmenting is left for further study..
		   It all gets written into the file anyway */
		logger(1,"** RECV_FILE: NETWORK JOB TRAILER SECOND FRAGMENT RECEIVED???  Line=%s:%d\n",
		       Line->HostName,Stream);
	      }
	      break;
	  default:
	      break;
	}
	parse_headers(Index, Stream, 0); /* Update whatever data you have.. */

}


/*
 | End of file received. Queue file to correct disposition and inform mailer
 | if needed. Also inform sender if no Quiet form.
 | Called from Receive_file().
 */
static int
finish_file(Index, Stream, StreamState, SYSIN)
int		Index, Stream, SYSIN;
StreamStates	*StreamState;	/* The state of the stream we handle now */
{
	unsigned char	OutputLine[LINESIZE], MessageSender[20];
	char		*FileName;
	char		*p;
	TIMETYPE	dt;
	char		*dts;
	char		ToNode[10], Format[20];
	register int	i, FileSize;
	int		PrimLine, AltLine;
	struct	FILE_PARAMS	*FileParams;
	struct	QUEUE		*Entry;
	struct	LINE 		*Line = &IoLines[Index];

	if (*StreamState != S_NJT_SENT) { /* Something is wrong */
	  logger(1, "RECV_FILE, line=%s:%d, EOF received when in state %s, Abort file\n",
		 Line->HostName, Stream, StreamStateStr(*StreamState));
	  abort_file(Index, Stream, SYSIN);
	  return 0;
	}

	i = parse_headers(Index, Stream, 1);
	if (i < 0)	/* Fatal error in headers */
	  return 0;	/* Channel was restarted */

	FileParams = &(Line->InFileParams[Stream]);

	if (FileParams->OurFileId == 0)
	  FileParams->OurFileId = get_send_fileid();

	/* All is ok (?) - ack completion, and rename file to 
	   reflect its disposition */
	if (SYSIN)
	  CompleteFile.SRCB = (((Stream + 9) << 4) | 0x8);
	else
	  CompleteFile.SRCB = (((Stream + 9) << 4) | 0x9);
	send_data(Index, &CompleteFile, sizeof(struct COMPLETE_FILE),
		  ADD_BCB_CRC);

	*StreamState = S_INACTIVE;	/* Xfer complete - Stream is idle */

	memset(&dt,0,sizeof(dt));
	dts = MsecAgeStr(&FileParams->RecvStartTime, &dt);
	logger(2,"RECV_FILE: received SYS%s file on line %s:%d, %d B in %s secs, %s B/sec.\n",
	       SYSIN ? "IN" : "OUT",
	       Line->HostName, Stream,
	       Line->OutFilePos[Stream], dts,
	       BytesPerSecStr(Line->OutFilePos[Stream],&dt));

	/* Because we'll get another EOF or ACK soon... */

	FileSize = close_file(Index, F_OUTPUT_FILE, Stream);
	/* A bit later, do  rename_file(...,RN_NORMAL,..) */

	/* Lets find target LINK... */
	*ToNode = 0;

	strcpy(FileParams->line,"?Unknown?");
	if ((p = strchr(FileParams->To, '@')) != NULL) {
		strncpy( ToNode,p+1,sizeof(ToNode)-1 );
	}

	/* Following code lifted from   FILE_QUEUE.C:  queue_file()  */

	stat(FileParams->SpoolFileName,&FileParams->FileStats);

	switch (i = find_line_index(ToNode,FileParams->line,Format,
				    &PrimLine,&AltLine)) {
	  case NO_SUCH_NODE:

	      /* Get a pointer to where ever it is.. */
	      FileName = rename_file(FileParams,RN_HOLD_ABORT,F_OUTPUT_FILE);

	      /* Log a receive */
	      rscsacct_log(FileParams,0);

	      logger(1,"RECV_FILE: Line %s:%d,  Got a file from `%s' to `%s', but don't know route!\n",
		     Line->HostName,Stream,
		     FileParams->From,FileParams->To);
	      break; /* Hmm.. Local ? */

	  case ROUTE_VIA_LOCAL_NODE:
	      /* ??? Needed or not ? */
	      FileName = rename_file(FileParams,RN_NORMAL,F_OUTPUT_FILE);

	      inform_filearrival( FileName,FileParams,OutputLine );
	      /* Log a receive */
	      rscsacct_log(FileParams,0);

	      sprintf(MessageSender, "@%s", LOCAL_NAME);
	      /* Send message back only if not found the QUIET option. */
	      if (((FileParams->type & F_NOQUIET) != 0) &&
		  (*OutputLine != 0))
		send_nmr(MessageSender,
			 FileParams->From,
			 OutputLine, strlen(OutputLine),
			 ASCII, CMD_MSG);
	      else
		logger(3,"RECV_FILE: Quiet ack of received file - no msg back.\n");
	      break;

	  case LINK_INACTIVE:	/* No alternate route available... */
	      /* Get a pointer to where ever it is.. */
	      FileName = rename_file(FileParams,RN_NORMAL,F_OUTPUT_FILE);
	      /* Log a receive */
	      rscsacct_log(FileParams,0);

	      /* Queue it */
	      Entry = build_queue_entry(FileName, PrimLine, FileSize,
					FileParams->To, FileParams);
	      add_to_file_queue( &IoLines[PrimLine], PrimLine, Entry );
	      return 0;

	  default:	/* Hopefully a line index */
	      if ((i < 0) || (i >= MAX_LINES) || IoLines[i].HostName[0] == 0) {
		/* Get a pointer to where ever it is.. */
		FileName = rename_file(FileParams,RN_HOLD_ABORT,F_OUTPUT_FILE);
		/* Log a receive */
		rscsacct_log(FileParams,0);
		logger(1, "FILE_QUEUE, Find_line_index() returned erronous index (%d) for node %s\n", i, p);
		return 0;
	      }

	      /* Get a pointer to where ever it is.. */
	      FileName = rename_file(FileParams, RN_NORMAL, F_OUTPUT_FILE);
	      /* Queue it */

	      Entry = build_queue_entry(FileName, i, FileSize,
					FileParams->To, FileParams);
	      add_to_file_queue( &IoLines[i], i, Entry );
	      break;
	} /* switch() */

	return 0;	/* We finished here. No need to send ACK */
}


/*
 | Parse the various headers and trailers used by NJE.
 | If there is an error in the headers the channel is restarted
 | and the function returns -1. In a normal case it returns zero.
 */
static int
parse_headers(Index, Stream, WriteIt)
const int	Index, Stream, WriteIt;
{
	int	i, size;
	unsigned char	*p,
			Afield[20];	/* Used when translating
					   fields to ASCII */
	struct	JOB_HEADER	*NJHp;
	struct	DATASET_HEADER	*DSHp;
	struct	DATASET_HEADER_G    *DSHGp;
	struct	DATASET_HEADER_RSCS *DSHVp;
	unsigned char		*DSHcp;
	struct	FILE_PARAMS	*FileParams;
	struct	LINE	*Line;
	FILE	*fd;
	struct stat fstats;
	long	DSHrecords = -1;

	Line = &IoLines[Index];

	/* Create some equivalences */
	NJHp = (struct JOB_HEADER *)    Line->SavedJobHeader[Stream];
	DSHp = (struct DATASET_HEADER *)Line->SavedDatasetHeader[Stream];
	DSHcp = (unsigned char *)       Line->SavedDatasetHeader[Stream];
	DSHGp = &DSHp->NDH;  /* NOTE: These CAN be of different size, */
	DSHVp = &DSHp->RSCS; /*       than the expected defaults!     */
	FileParams = &(Line->InFileParams[Stream]);
	fd = Line->OutFds[Stream];
	fstat(fileno(fd),&fstats);

	strcpy(FileParams->From, "***@***");
	strcpy(FileParams->To, "***@***");
	strcpy(FileParams->FileName,"_unknown_");
	strcpy(FileParams->FileExt, "_unknown_");
	FileParams->OurFileId = 0;
	FileParams->tag[0]    = 0;

	/* Init it, just in case we don't have NJH;
	   like with locally sent files.. */
	FileParams->NJHtime = fstats.st_mtime;

	if (WriteIt && Line->SizeSavedJobHeader[Stream] == 0) {
	  /* No JOB HEADER ! */
	  logger(1,"RECV_FILE: Line=%s:%d, Missing JOB HEADER from a %s!\n",
		 Line->HostName,Stream,
		 (FileParams->type & F_JOB) ? "SYSIN JOB" : "file");
	  abort_file(Index,Stream,FileParams->type & F_JOB);
	  /*restart_channel(Index);*/
	  return -1;
	}
	if (WriteIt && Line->SizeSavedJobTrailer[Stream] == 0) {
	  /* No JOB TRAILER ! */
	  logger(1,"RECV_FILE: Line=%s:%d, Missing JOB TRAILER from a %s!\n",
		 Line->HostName,Stream,
		 (FileParams->type & F_JOB) ? "SYSIN JOB" : "file");
	  abort_file(Index,Stream,FileParams->type & F_JOB);
	  /*restart_channel(Index);*/
	  return -1;
	}

	if (WriteIt) {
	  fflush(fd);
	  fseek(fd,0,0);	/* Seek to the begin of the file,
				   we rewrite the ASCII header.    */
	}

	/* ================ Common things ================ */
	FileParams->format = EBCDIC;
	if (WriteIt)
	  fprintf(fd, "FMT: EBCDIC\n");

	if (Line->SizeSavedJobHeader[Stream] != 0) {
	  /* Convert the from address */
	  /* For Username */
	  EBCDIC_TO_ASCII(NJHp->NJHGORGR, Afield, 8);
	  if (Afield[0] == 0 || Afield[0] == ' ')
	    EBCDIC_TO_ASCII(NJHp->NJHGUSID, Afield, 8);
	  /* Remove the trailing spaces */
	  i = despace(Afield,8);
	  p = Afield + i;
	  *p++ = '@';
	  EBCDIC_TO_ASCII(NJHp->NJHGORGN, p, 8);
	  despace(p,8);
	  strcpy(FileParams->From, Afield);
	  if (WriteIt)
	    fprintf(fd, "FRM: %-17s\n", Afield);

	  /* Get the job name */
	  EBCDIC_TO_ASCII(NJHp->NJHGJNAM, Afield, 8);
	  despace(Afield,8);
	  strcpy(FileParams->JobName, Afield);
	  if (WriteIt)
	    fprintf(fd,"JNM: %-8s\n",Afield);

	  /* Store the file id number */
	  FileParams->FileId = ntohs(NJHp->NJHGJID);
	  FileParams->OurFileId = FileParams->FileId;
	  if (WriteIt)
	    fprintf(fd, "FID: %04ld\nOID: %04ld\n",
		    FileParams->FileId, FileParams->FileId);

	  FileParams->NJHtime = ibmtime2unixtime(NJHp->NJHGETS);
	}


	/* ================== SYSIN vs. SYSOUT ================== */
	/* See which type of task this is: A SYSIN, or a SYSOUT ? */
	if (FileParams->type & F_JOB) {
	  /* ============================================ */
	  /* A SYSIN, thus we have only  NJH, (data), NJT */

	  if (WriteIt)
	    fprintf(fd,"TYP: JOB\n");


	  if (Line->SizeSavedDatasetHeader[Stream] != 0) {
	    /* Spurious DATASET HEADER ! */
	    logger(1,"RECV_FILE: Line=%s:%d, Spurious DATASET HEADER from a SYSIN JOB!\n",
		   Line->HostName,Stream);
	    restart_channel(Index);
	    return -1;
	  }

	  if (Line->SizeSavedJobHeader[Stream] != 0) {
	    FileParams->JobClass = EBCDIC_ASCII[NJHp->NJHGJCLS];
	    if (WriteIt)
	      fprintf(fd, "CLS: %c\n", FileParams->JobClass);

	    /* To Username */
	    EBCDIC_TO_ASCII(NJHp->NJHGXEQU, Afield, 8);
	    i = despace(Afield,8);
	    /* Commonly the username is NULL, rewrite it as `JOB' */
	    /* if (Afield[0] == 0) {
	       strcpy(Afield,"JOB");
	       i = 3;
	       } */
	    p = Afield + i;
	    *p++ = '@';
	    EBCDIC_TO_ASCII(NJHp->NJHGXEQN, p, 8);
	    despace(p,8);
	    strcpy(FileParams->To, Afield);

	    strcpy(FileParams->FormsName, DefaultForm);
	    strcpy(FileParams->FileName,  FileParams->JobName);
	    strcpy(FileParams->FileExt,   "JOB");
	    strcpy(FileParams->DistName,  "SYSTEM");
	    if (WriteIt) {
	      fprintf(fd, "TOA: %-17s\n", FileParams->To);
	      fprintf(fd, "FOR: %-8s\n", FileParams->FormsName);
	      fprintf(fd, "FNM: %-8s\n", FileParams->FileName);
	      fprintf(fd, "EXT: %-8s\n", FileParams->FileExt);
	      fprintf(fd, "DIS: %-8s\n", FileParams->DistName);
	    }  	
	     
	    /* PRT To Username */
	    EBCDIC_TO_ASCII(NJHp->NJHGPRTR, Afield, 8);
	    i = despace(Afield,8);
	    p = Afield + i;
	    *p++ = '@';
	    EBCDIC_TO_ASCII(NJHp->NJHGPRTN, p, 8);
	    despace(p,8);
	    if (WriteIt)
	      fprintf(fd, "PRT: %-17s\n", Afield);

	    /* PUN To Username */
	    EBCDIC_TO_ASCII(NJHp->NJHGPUNR, Afield, 8);
	    i = despace(Afield,8);
	    p = Afield + i;
	    *p++ = '@';
	    EBCDIC_TO_ASCII(NJHp->NJHGPUNN, p, 8);
	    despace(p,8);
	    if (WriteIt)
	      fprintf(fd, "PUN: %-17s\n", Afield);
	  }


	} else {
	  /* =================================================== */
	  /* A SYSOUT, which means we have NJH. DSH, (data), NJT */

	  if (WriteIt && Line->SizeSavedDatasetHeader[Stream] == 0) {
	    /* No DATASET HEADER ! */
	    logger(1,"RECV_FILE: Line=%s:%d, Missing DATASET HEADER from a file!\n",
		   Line->HostName,Stream);
	    restart_channel(Index);
	    return -1;
	  }

	  if ((size = Line->SizeSavedDatasetHeader[Stream]) != 0) {

	    /* Test whether it is PRINT file */
	    if ((DSHGp->NDHGFLG2 & 0x80) != 0) {
	      FileParams->type = F_PRINT;
	      if (WriteIt)
		fprintf(fd, "TYP: PRINT\n");
	    } else {
		FileParams->type = F_PUNCH;
		if (WriteIt)
		  fprintf(fd, "TYP: PUNCH\n");
	    }

	    DSHrecords = ntohl(DSHGp->NDHGNREC);

	    if (ntohs(DSHGp->LENGTH_4) != sizeof(struct DATASET_HEADER_G)){
	      /* Hmm.. General section is not of expected size! */
	      int offset = (ntohs(DSHGp->LENGTH_4) -
			    sizeof(struct DATASET_HEADER_G));
	      DSHVp = (struct DATASET_HEADER_RSCS *)(((char *)DSHVp) + offset);

	      /* If the offset isn't multiple of 4, we propably crash soon! */
	      if ((offset % 4) != 0) {
		logger(1,"RECV_FILE: Dataset Header  General Section's size varies from expected by %d bytes! Immediate crash due to non-alignment propable!\n",offset);
		logger(1,"   From: %s  To: %s  JobName: %s\n",
		       FileParams->From,FileParams->To,FileParams->JobName);
	      }
	    }

	    FileParams->JobClass = EBCDIC_ASCII[DSHGp->NDHGCLAS];
	    if (WriteIt)
	      fprintf(fd, "CLS: %c\n", FileParams->JobClass);

	    /* To Username */
	    EBCDIC_TO_ASCII(DSHGp->NDHGRMT, Afield, 8);
	    if (Afield[0] == 0 || Afield[0] == ' ')
	      EBCDIC_TO_ASCII(DSHGp->NDHGXWTR, Afield, 8);
	    i = despace(Afield,8);
	    p = Afield + i;
	    *p++ = '@';
	    EBCDIC_TO_ASCII(DSHGp->NDHGNODE, p, 8);
	    despace(p,8);
	    strcpy(FileParams->To, Afield);
	    if (WriteIt)
	      fprintf(fd, "TOA: %-17s\n", FileParams->To);

	    /* Check for the QUIET option */
	    EBCDIC_TO_ASCII(DSHGp->NDHGFORM, Afield, 8);
	    Afield[8] = '\0';
	    strcpy(FileParams->FormsName,Afield);
	    if (WriteIt)
	      fprintf(fd, "FOR: %-8s\n",FileParams->FormsName);

	    if (strncmp(Afield, "QU", 2) != 0) /* Don't be quiet */
	      FileParams->type |= F_NOQUIET;
	    /* Note: Recommendation says: detect by two first chars! */


#define RSCS_SECTION 0x87 /* Section ID byte */
	    if (DSHVp->ID == RSCS_SECTION) {
	      /* Ok, process it, if it is RSCS section. MVS can put
		 there DATASTREAM_SECTION, which we don't know.. */

	      /* Filename */
	      EBCDIC_TO_ASCII(DSHVp->NDHVFNAM, Afield, 8);
	      /* if (Afield[0] == 0 || Afield[0] == ' ')
		 EBCDIC_TO_ASCII(NJHp->NJHGJNAM, Afield, 8); */
	      despace(Afield,8);
	      strcpy(FileParams->FileName, Afield);
	      if (WriteIt)
		fprintf(fd, "FNM: %-8s\n", Afield);

	      /* Filename extension */
	      EBCDIC_TO_ASCII(DSHVp->NDHVFTYP, Afield, 8);
	      despace(Afield,8);
	      /* if (Afield[0] == 0 || Afield[0] == ' ')
		 strcpy(Afield, "OUTPUT");	*/
	      strcpy(FileParams->FileExt, Afield);
	      if (WriteIt)
		fprintf(fd, "EXT: %-8s\n", Afield);

	      /* RSCS DIST information */
	      EBCDIC_TO_ASCII(DSHVp->NDHVDIST,
			      FileParams->DistName, 8);
	      despace(FileParams->DistName,8);
	      if (WriteIt)
		fprintf(fd, "DIS: %-8s\n",FileParams->DistName);

	      /* If there is no RSCS TAG, we have zeros, which map to zeros..*/
	      EBCDIC_TO_ASCII(DSHVp->NDHVTAGR,
			      FileParams->tag,136);
	      FileParams->tag[136] = 0;
	      /* Don't write the TAG here yet.. */

	    } /* End of RSCS section processing */
	    else {
	      /* No RSCS section available, build it like
		 the VM/370 RSCS has the style.. */

	      strcpy(FileParams->FileName, FileParams->JobName);
	      if (WriteIt)
		fprintf(fd, "FNM: %-8s\n", FileParams->FileName);

	      /* Filename extension */
	      strcpy(FileParams->FileExt, "OUTPUT");
	      if (WriteIt)
		fprintf(fd, "EXT: OUTPUT  \n");

	      strcpy(FileParams->DistName,"SYSTEM");
	      if (WriteIt)
		fprintf(fd, "DIS: SYSTEM  \n");

	    } /* End of no RSCS section available */

	  } /* End of "Saved DSH available" */

	  if (WriteIt)
	    logger(3,
		   "=> Receiving file %s.%s from %s to %s, format=%d, type=%d, class=%c\n",
		   FileParams->FileName, FileParams->FileExt,
		   FileParams->From, FileParams->To,
		   FileParams->format, FileParams->type,
		   FileParams->JobClass);

	} /* End of SYSOUT files */

	if (FileParams->tag[0] == 0 /* No tag present.. */) {
	  strcpy(Afield,FileParams->From);
	  /* Pick the userid and node separate.. */
	  if ((p = (unsigned char *)strchr(Afield,'@')) != NULL) *p++ = 0;
	  else p = Afield;
	  sprintf(FileParams->tag,"FILE (%04ld) ORIGIN %-8s %-8s",
		  FileParams->FileId, p, Afield);
/*XX:??*/
	  if (FileParams->NJHtime != 0) {
	    struct tm *tm_var = localtime(&FileParams->NJHtime);
	    p = (unsigned char *)(FileParams->tag + strlen(FileParams->tag));
	    strftime(p,sizeof(FileParams->tag)-1-strlen(FileParams->tag),
		     "  %D %T %Z",tm_var);
	  }
	} /* When no TAG:, generate one! */
	if (WriteIt)	/* Now print the TAG */
	  fprintf(fd,"TAG: %-136s\n",FileParams->tag);

	/* Records counts.. */
	if (WriteIt) {
	  fprintf(fd, "REC: %8ld (%ld)",FileParams->RecordsCount,DSHrecords);
	}

	/* fprintf(fd,"END:");  -- this is pre-written */

	if (WriteIt) {
	  fflush(fd);
	  fseek(fd,0,2);	/* Seek to the END of the file,
				   we rewrote the ASCII header.    */
	}

	return 1;	/* All ok */
}


/*
 | Abort the receiving file because of sender's abort. Send EOF as reply,
 | close the output file, put it on hold-Abort for later inspection, and
 | reset the stream's state.
 */
static void
file_sender_abort(Index, Stream, SYSIN)
int	Index, Stream, SYSIN;
{

	/* Send EOF block to confirm this abort */
	if (SYSIN)
	  EOFblock.SRCB = (((Stream + 9) << 4) | 0x8);
	else
	  EOFblock.SRCB = (((Stream + 9) << 4) | 0x9);
	send_data(Index, &EOFblock, sizeof(struct EOF_BLOCK), ADD_BCB_CRC);

	/* Signal that the file was closed and hold it */
	close_file(Index, F_OUTPUT_FILE, Stream);
	{
	  struct FILE_PARAMS *FileParams;
	  FileParams = &IoLines[Index].InFileParams[Stream];
	  stat(FileParams->SpoolFileName,&FileParams->FileStats);
	  rename_file(FileParams, RN_HOLD_ABORT, F_OUTPUT_FILE);
	  IoLines[Index].InStreamState[Stream] = S_INACTIVE;
	}
}


/*
 | Abort the receiving file because we have a problem.
 | close the output file, put it on hold-Abort for later inspection, and
 | reset the stream's state.
 */
static void
abort_file(Index, Stream, SYSIN)
int	Index, Stream, SYSIN;
{

	/* Send Reject-file to abort file */
	if (SYSIN)
	  RejectFile.SRCB = (((Stream + 9) << 4) | 0x8);
	else
	  RejectFile.SRCB = (((Stream + 9) << 4) | 0x9);
	send_data(Index, &RejectFile, sizeof(struct REJECT_FILE), ADD_BCB_CRC);

	/* Signal that the file was closed and hold it */
	close_file(Index, F_OUTPUT_FILE, Stream);
	rename_file(&IoLines[Index].InFileParams[Stream],
		    RN_HOLD_ABORT, F_OUTPUT_FILE);
	/* To stop this stream */
	IoLines[Index].InStreamState[Stream] = S_REFUSED;
}
