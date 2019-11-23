/* IO.C    V3.3-mea
 | Copyright (c) 1988,1989,1990 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.
 |
 | Do the OS independent IO parts.
 | Note: The way of saving the last tramitted buffer should be improved. It
 |       currently saves all data if it is not a NAK. This should be modified
 |       according to the status, and also the routine that handles the NAK
 |       should be corrected accordingly.
 |
 | When a buffer is sent, we send the saved buffer in IoLines structure. This
 | is because the buffer we usually get is a dynamic memory location. Thus,
 | the XmitBuffer in IoLines structure is the only one which is static.
 |
 |
 | Sections: SHOW-LINES:   Send to users the lines status.
 |           INIT-IO:      Initialize the I/O lines.
 |           QUEUE-IO:     Queue an I/O request.
 |           STATS:        Write statistics.
 |           COMMAND-PARSER Parses the opareator's commands.
 */
#include "consts.h"
#include "headers.h"
#include "prototypes.h"

static int   add_VMnet_block __(( struct LINE *Line, const int flag, const void *buffer, const int size, void *NewLine, const int BCBcount ));
static void  debug_dump_buffers __(( const char *UserName ));

EXTERNAL int	MustShutDown;
EXTERNAL int	PassiveSocketChannel;
EXTERNAL int	LogLevel;

#define	VMNET_INITIAL_TIMEOUT	15	/* 15 seconds */

/*============================ SHOW-LINES ==============================*/

char *StreamStateStr(state)
StreamStates state;
{
	switch (state) {
	  case S_INACTIVE:
	      return "S_INACTIVE";
	  case S_REFUSED:
	      return "S_REFUSED";
	  case S_WAIT_A_BIT:
	      return "S_WAIT_A_BIT";
	  case S_WILL_SEND:
	      return "S_WILL_SEND";
	  case S_NJH_SENT:
	      return "S_NJH_SENT";
	  case S_NDH_SENT:
	      return "S_NDH_SENT";
	  case S_SENDING_FILE:
	      return "S_SENDING_FILE";
	  case S_RECEIVING_FILE:
	      return "S_RECVING_FILE";
	  case S_NJT_SENT:
	      return "S_NJT_SENT";
	  case S_EOF_SENT:
	      return "S_EOF_SENT";
	  case S_EOF_FOUND:
	      return "S_EOF_FOUND";
	  case S_REQUEST_SENT:
	      return "S_REQUEST_SENT";
	  default:
	      return "**UnknownStreamState**";
	}
	/* NOTREACHED */
}


static char *linestate2string(state,sockpend)
int state, sockpend;
{
	static char bugbuf[10];
	switch (state) {
	    case INACTIVE:
	        return "INACTIVE  ";
	    case SIGNOFF:
		return "SIGNEDOFF ";
	    case DRAIN:
		return "DRAINED   ";
	    case ACTIVE:
		return "ACTIVE    ";
	    case F_SIGNON_SENT:
	    case I_SIGNON_SENT:
		return "SGN-Sent  ";
	    case LISTEN:
		return "LISTEN    ";
	    case RETRYING:
		return "Retry     ";
	    case TCP_SYNC:
		if (sockpend >= 0)
		  return "TCP-pend  ";
		else
		  return "TCP-sync  ";
	    default:
		sprintf(bugbuf,"**Unk:%d**",(int)state);
		return bugbuf;
	  }
}

static char *linetypedev2string(type,device)
int type;
char *device;
{
	static char line[60];
	switch (type) {
	    case DMF:
		sprintf(line, "  DMF (%s)", device);
		return line;
	    case DMB:
		sprintf(line, "  DMB (%s)", device);
		return line;
		break;
	    case DSV:
		sprintf(line, "  DSV (%s)", device); 
		return line;
		break;
	    case UNIX_TCP:
		return "  TCP      ";
	    case EXOS_TCP:
		return  "  TCP(Exos)";
	    case MNET_TCP:
		return "  TCP(Mnet)";
	    case DEC__TCP:
		return "  TCP(DEC) ";
	    case DECNET:
		sprintf(line, "  DECNET (%s)", device);
		return line;
	    case ASYNC:
		sprintf(line, "  ASYNC (%s)", device);
		return line;
	    default:
		return "   ***";
	  }
}

/*
 | Send the lines status to the user.
 */
void show_lines_status(to,infoprint)
const char	*to;	/* User@Node */
int infoprint;
{
	int	i, j;
	char	line[LINESIZE],
		from[SHORTLINE];	/* The message's sender address */
	struct	LINE	*Line;

	TIMETYPE now;
	GETTIME(&now);

	/* Create the sender's address.
	   It is the daemon on the local machine */

	sprintf(from, "@%s", LOCAL_NAME);	/* No username */

	sprintf(line, "FUNET-NJE version %s(%s)/%s, Lines status:",
		VERSION, SERIAL_NUMBER, version_time);
	send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);

	for (i = 0; i < MAX_LINES; i++) {
	  Line = &(IoLines[i]);
	  if (*(Line->HostName) == 0) continue; /* No host defined for this
						   line, ignore             */
	  sprintf(line, "Line.%d %-8s %3d (Q=%04d)  ", i,
		  Line->HostName, Line->TotalErrors,
		  Line->QueuedFiles);


	  strcat(line,linestate2string(Line->state,Line->socketpending));
	  strcat(line,linetypedev2string(Line->type,Line->device));

	  send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);

	  if (infoprint) {
	    char InAge[20], XmitAge[20];
	    char *sep = "", *sepp="|";
	    TIMETYPE nw;
	    memcpy(&nw,&now,sizeof(nw));
	    strcpy(InAge,MsecAgeStr(&Line->InAge,&nw));
	    memcpy(&nw,&now,sizeof(nw));
	    if (GETTIMESEC(Line->XmitAge) != 0)
	      strcpy(XmitAge,MsecAgeStr(&Line->XmitAge,&nw));
	    else
	      strcpy(XmitAge,"Never");
	    sprintf(line," Bufinfo: InAge=%ss, RecvSize=%d, XmitAge=%ss, XmitSize=%d",
		    InAge, Line->RecvSize, XmitAge, Line->XmitSize);
	    send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);
	    sep=""; strcpy(line," Flags: ");
#define FLGP(flg,str) \
	if (Line->flags & flg) { strcat(line,sep);strcat(line,str);sep=sepp; }
	    FLGP(F_ACK_NULLS,		"ACK_NULLS");
	    FLGP(F_AUTO_RESTART,	"AUTO_RESTART");
	    FLGP(F_CALL_ACK,		"CALL_ACK");
	    FLGP(F_DONT_ACK,		"DONT_ACK");
	    FLGP(F_FAST_OPEN,		"FAST_OPEN");
	    FLGP(F_HALF_DUPLEX,		"HALF_DUPLEX");
	    FLGP(F_IN_SUSPEND,		"IN_SUSPEND");
	    FLGP(F_RELIABLE,		"RELIABLE");
	    FLGP(F_RESET_BCB,		"RESET_BCB");
	    FLGP(F_SENDING,		"SENDING");
	    FLGP(F_SHUT_PENDING,	"SHUT_PENDING");
	    FLGP(F_SLOW_INTERLEAVE,	"SLOW_ILEAVE");
	    FLGP(F_SUSPEND_ALLOWED,	"SUSPEND_ALLOWED");
	    FLGP(F_WAIT_A_BIT,		"WAIT_A_BIT");
	    FLGP(F_WAIT_V_A_BIT,	"WAIT_V_A_BIT");
	    FLGP(F_XMIT_CAN_WAIT,	"XMIT_CAN_WAIT");
	    FLGP(F_XMIT_MORE,		"XMIT_MORE");
	    FLGP(F_XMIT_QUEUE,		"XMIT_QUEUE");
#undef FLGP
	    send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);

	  }

	  if (Line->state != ACTIVE) /* Not active - don't display */
	    continue;		/* streams status */

	  for (j = 0; j < MAX_STREAMS; j++) {
	    sprintf(line, "Rcv-%d  ", j);
	    if (Line->InStreamState[j] != S_INACTIVE) {
	      /* Don't show inactive ones */
	      switch (Line->InStreamState[j]) {
		case S_INACTIVE:
		    strcat(line, "Inactive");
		    break;
		case S_REQUEST_SENT:
		    strcat(line, "REQUEST ");
		    break;
		case S_NJH_SENT:
		    strcat(line, "NJH-RECV");
		    break;
		case S_NDH_SENT:
		    strcat(line, "NDH-RECV");
		    break;
		case S_NJT_SENT:
		    strcat(line, "NJT-RECV");
		    break;
		case S_RECEIVING_FILE:
		    strcat(line, "RECVING ");
		    break;
		case S_EOF_SENT:
		    strcat(line, "EOF-RECV");
		    break;
		case S_REFUSED:
		    strcat(line, "REFUSED ");
		    break;
		case S_WAIT_A_BIT:
		    strcat(line, "WAITABIT");
		    break;
		default:
		    strcat(line, "******   ");
		    break;
	      }
	      if ((Line->InStreamState[j] != S_INACTIVE) &&
		  (Line->InStreamState[j] != S_REFUSED) )
		sprintf(line+strlen(line), " (%s) (%s => %s) %dkB",
			Line->InFileParams[j].JobName,
			Line->InFileParams[j].From,
			Line->InFileParams[j].To,
			(int)(Line->OutFilePos[j]/1024));
	      send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);
	    }
	    sprintf(line, "Snd-%d  ", j);
	    if (Line->OutStreamState[j] != S_INACTIVE) {
	      /* Don't show inactive ones */
	      switch(Line->OutStreamState[j]) {
		case S_INACTIVE:
		    strcat(line, "Inactive");
		    break;
		case S_REQUEST_SENT:
		    strcat(line, "REQUEST ");
		    break;
		case S_NJH_SENT:
		    strcat(line, "NJH-SENT");
		    break;
		case S_NDH_SENT:
		    strcat(line, "NDH-SENT");
		    break;
		case S_SENDING_FILE:
		    strcat(line, "SENDING ");
		    break;
		case S_NJT_SENT:
		    strcat(line, "NJT-SENT");
		    break;
		case S_EOF_SENT:
		    strcat(line, "EOF-SENT");
		    break;
		case S_REFUSED:
		    strcat(line, "REFUSED ");
		    break;
		case S_WAIT_A_BIT:
		    strcat(line, "WAITABIT");
		    break;
		default:
		    strcat(line, "******  ");
		    break;
	      }
	      if ((Line->OutStreamState[j] != S_INACTIVE)  &&
		  (Line->OutStreamState[j] != S_REFUSED) )
		sprintf(line+strlen(line), " (%s) (%s => %s) %dkB",
			Line->OutFileParams[j].JobName,
			Line->OutFileParams[j].From,
			Line->OutFileParams[j].To,
			(int)(Line->InFilePos[j]/1024));
	      send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);
	    }
	  }
	  sprintf(line,
		  " %d streams in service.  WrSum: %ldfil/%ldB  RdSum: %ldfil/%ldB",
		  Line->MaxStreams,
		  Line->WrFiles, Line->WrBytes, Line->RdFiles, Line->RdBytes);
	  send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);
	}

	sprintf(line, "End of Q SYS display");
	send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);
}


/*
 | Send the lines statistics to the user.
 */
void
show_lines_stats(to)
const char	*to;	/* User@Node */
{
	int	i;
	char	line[LINESIZE],
		from[SHORTLINE];	/* The message's sender address */
	struct	LINE	*Line;

/* Create the sender's address. It is the daemon on the local machine */
	sprintf(from, "@%s", LOCAL_NAME);	/* No username */

	sprintf(line, "FUNET-NJE version %s(%s)/%s, Lines statistics:",
		VERSION, SERIAL_NUMBER, version_time);
	send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);

	for (i = 0; i < MAX_LINES; i++) {
	  Line = &(IoLines[i]);
	  if (*(Line->HostName) != '\0') {
	    sprintf(line, "Line.%d %8s: Blocks send/recv: %d/%d, Wait recvd: %d,",
		    i, Line->HostName,
		    Line->sumstats.TotalOut+Line->stats.TotalOut,
		    Line->sumstats.TotalIn+Line->stats.TotalIn,
		    Line->sumstats.WaitIn+Line->stats.WaitIn	    );
	    send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);
	    sprintf(line, "     NMRs sent/recv: %d/%d, NAKs send/recvd: %d/%d,  Acks-only sent/recv: %d/%d",
		    Line->sumstats.MessagesOut+Line->stats.MessagesOut,
		    Line->sumstats.MessagesIn+Line->stats.MessagesIn,
		    Line->sumstats.RetriesOut+Line->stats.RetriesOut,
		    Line->sumstats.RetriesIn+Line->stats.RetriesIn,
		    Line->sumstats.AckOut+Line->stats.AckOut,
		    Line->sumstats.AckIn+Line->stats.AckIn);
	    send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);
	  }
	}

	sprintf(line, "End of Q STAT display");
	send_nmr(from, to, line, strlen(line), ASCII, CMD_MSG);
}


/*======================= INIT-IO ==================================*/
/*
 | Initialize all the communication lines. Initialize DMFs, TCP and
 | DECnet connections, each one only if there is a link of that type.
 | If we have at least one TCP passive end, we don't queue an accept for
 | the specific line. We queue only one accept, regarding of the number of
 | passive ends. When a connection will be received, the VMnet control record
 | will be used to locate the correct line.
 */
void
init_communication_lines()
{
	int	i, InitTcpPassive, InitDECnetPassive = 0, TcpType = 0;

	InitDECnetPassive = InitTcpPassive = 0;
	for (i = 0; i < MAX_LINES; i++) {
	  if (IoLines[i].HostName[0] == 0) continue; /* no line */
	  if (IoLines[i].state == ACTIVE) {
	    switch (IoLines[i].type) {
#ifdef VMS
	      case DMB:
	      case DSV:
	      case DMF:
	          init_dmf_connection(i);
		  queue_receive(i);
		  break;
	      case DEC__TCP:
	      case MNET_TCP:
	      case EXOS_TCP:
		  /* Create an active side and also passive side */
		  init_active_tcp_connection(i,0);
		  InitTcpPassive++;
		  TcpType = IoLines[i].type;
		  break;
	      case ASYNC:
		  init_async_connection(i);
		  queue_receive(i);
		  break;
	      case DECNET:
		  init_active_DECnet_connection(i);
		  InitDECnetPassive++;
		  break;
#endif
#ifdef UNIX
	      case UNIX_TCP:
		  init_active_tcp_connection(i,0);
		  InitTcpPassive++;
		  break;
#endif
	      case X_25:
	      default:
		  logger(1, "IO: No protocol for line #%d\n",
			 i);
		  break;
		}
	  }
	}

/* Check whether we have to queue a passive accept for TCP & DECnet lines */
	if(InitTcpPassive != 0)
	  init_passive_tcp_connection(TcpType);
#ifdef VMS
	if (InitDECnetPassive != 0)
	  init_DECnet_passive_channel();
#endif
}


void
abort_streams_and_requeue(Index)
int Index;
{
	int i;
	struct	LINE	*Line = &IoLines[Index];
	struct	MESSAGE	*MessageEntry;

	logger(2, "IO: init_link_state(%s/%d) type=%d (Q=%d)\n",
	       Line->HostName,  Index,  Line->type, Line->QueuedFiles);
	
	/* Close active file, and delete of output file. */
	for (i = 0; i < MAX_STREAMS; i++) {
	  Line->CurrentStream = i;
	  if ((Line->OutStreamState[i] != S_INACTIVE) &&
	      (Line->OutStreamState[i] != S_REFUSED)) { /* File active */
	    close_file(Index, F_INPUT_FILE, i);
	    Line->OutStreamState[i] = S_INACTIVE;
	    requeue_file_entry(Index,Line);
	  }
	  if ((Line->InStreamState[i] != S_INACTIVE) &&
	      (Line->InStreamState[i] != S_REFUSED)) { /* File active */
	    delete_file(Index, F_OUTPUT_FILE, i);
	    Line->InStreamState[i] = S_INACTIVE;
	  }
	}

	i = requeue_file_queue(Line);
	if (i != 0)
	  logger(1,"IO: init_link_state() calling requeue_file_queue() got an activecount of %d!  Should be 0!\n",i); 
	/* Line->QueuedFiles = i; */
	/* Line->QueuedFilesWaiting = 0; */

	/* Requeue all messages and commands waiting on it */
	MessageEntry = Line->MessageQstart;
	Line->MessageQstart = NULL;
	Line->MessageQend   = NULL;
	while (MessageEntry != NULL) {
	  struct MESSAGE *NextEntry = MessageEntry->next;
	  nmr_queue_msg(MessageEntry);
	  free(MessageEntry);
	  MessageEntry = NextEntry;
	}
}
/*
 | Reset various link state variables, make sure all open files
 | get closed, queue states get reset, etc..
 */
void
init_link_state(Index)
const int Index;
{
	struct	LINE	*Line;
	register int	i;	/* Stream index */
	int	oldstate;

	Line = &(IoLines[Index]);
	if (Index > MAX_LINES || Line->HostName[0] == 0) {
	  logger(1,"IO: init_link_state(%d) - Bad line number!\n",Index);
	  return;
	}

	if (Line->socket >= 0)
	  close(Line->socket);
	Line->socket = -1;
	if (Line->socketpending >= 0)
	  close(Line->socketpending);
	Line->socketpending = -1;
#ifdef NBSTREAM
	Line->WritePending = NULL;
#endif

	oldstate = Line->state;
	Line->state = INACTIVE;

	abort_streams_and_requeue(Index);

	Line->state = oldstate;

#ifdef	USE_XMIT_QUEUE
	/* Clear all queued transmit buffers */
	while (Line->FirstXmitEntry != Line->LastXmitEntry) {
	  if (Line->XmitQueue[Line->FirstXmitEntry])
	    free(Line->XmitQueue[Line->FirstXmitEntry]);
	  Line->XmitQueue[Line->FirstXmitEntry] = NULL;
	  Line->FirstXmitEntry = (Line->FirstXmitEntry +1) % MAX_XMIT_QUEUE;
	}
	Line->FirstXmitEntry = 0;
	Line->LastXmitEntry  = 0;
#endif

	/* After all files closed, restart the line */
	Line->errors = 0;	/* We start again... */
	Line->InBCB  = 0;
	/* Line->OutBCB = 0; */
	Line->flags &= ~(F_RESET_BCB    |
			 F_WAIT_A_BIT   | F_WAIT_V_A_BIT |
			 F_SENDING      | F_CALL_ACK     |
			 F_XMIT_CAN_WAIT| F_SLOW_INTERLEAVE);
	Line->CurrentStream = 0;
	Line->ActiveStreams = 0;
	Line->FreeStreams   = Line->MaxStreams;
	for (i = 0; i < MAX_STREAMS; i++) {
	  Line->InStreamState[i] = S_INACTIVE;
	  Line->OutStreamState[i] = S_INACTIVE;
	}
	Line->RecvSize = 0;
	Line->XmitSize = 0;
	Line->WritePending = NULL;

	/* Dequeue all waiting timeouts for this line */
	delete_line_timeouts(Index);
}

/*
 | Restart a line that is not active.
 */
void
restart_line(Index)
const int Index;
{
	struct LINE *Line = &IoLines[Index];

	/* First check the line is in correct status */
	if ((Index < 0) || (Index >= MAX_LINES)) {
	  logger(1, "IO, Restart line: line #%d out of range\n",
		 Index);
	  return;
	}

	Line->flags &= ~F_SHUT_PENDING; /* Remove the shutdown pending flag. */

	switch (Line->state) {
	  case INACTIVE:
	  case SIGNOFF:
	  case RETRYING:
	      break;	/* OK - go ahead and start it */
	  case DRAIN:
	  case F_SIGNON_SENT:
	  case I_SIGNON_SENT:
	      logger(1, "IO, Trying to start line %s (#%d) in state %d. Illegal\n",
		     Line->HostName, Index, Line->state);
	      return;
	  default:
	      logger(1, "IO, Line %s (#%d) in illegal state (#%d) for start op.\n",
		     Line->HostName,Index,Line->state);
	      return;
	}

	logger(2, "IO, Restarting line #%d (%s) (Q=%d)\n",
	       Index, Line->HostName, Line->QueuedFiles);

	/* Init the line according to its type */
	Line->state = DRAIN;	/* Will be set to INACTIVE in case
				   of error during initialization. */

	/* Programmable backoff.. */
	if (Line->RetryIndex < MAX_RETRIES-1 &&
	    Line->RetryPeriods[Line->RetryIndex+1] > 0)
	  Line->RetryPeriod = Line->RetryPeriods[Line->RetryIndex++];
	else
	  Line->RetryPeriod = Line->RetryPeriods[Line->RetryIndex];

	init_link_state(Index);

	switch (Line->type) {
#ifdef VMS
	  case DMB:
	  case DSV:
	  case DMF:
	      init_dmf_connection(Index);
	      queue_receive(Index);
	      break;
	  case DEC__TCP:
	  case MNET_TCP:
	  case EXOS_TCP:
	      init_active_tcp_connection(Index,0);
	      break;
	  case DECNET:
	      init_active_DECnet_connection(Index);
	      break;
	  case ASYNC:
	      init_async_connection(Index);
	      queue_receive(Index);
	      break;
#endif
#ifdef UNIX
	  case UNIX_TCP:
	      init_active_tcp_connection(Index,0);
	      break;
#endif
	  default:
	      break;
	}
}



/*===================== QUEUE-IO =====================================*/
/*
 | Queue the receive for the given line. Test its type, and then call the
 | appropriate routine. Also queue a timer request for it (our internal
 | timer, not the VMS one).
 */
void
queue_receive(Index)
const int	Index;
{
	struct	LINE	*Line;

	Line = &(IoLines[Index]);

/* Do we have to queue a receive ??? */
	switch (Line->state) {
	  case INACTIVE:
	  case SIGNOFF:
	  case RETRYING:
	  case LISTEN:		/* No need to requeue on these states */
	      return;
	  case ACTIVE:
	  case DRAIN:
	  case F_SIGNON_SENT:
	  case I_SIGNON_SENT:
	  case TCP_SYNC:
	      break;	/* OK, requeue it */
	  default:
	      logger(1, "IO, Illegal line state %d on line %d during queue-Receive\n",
		     Line->state, Index);
	      Line->state = INACTIVE;
	      return;
	}

	switch (Line->type) {
#ifdef VMS
	  case DMB:
	  case DSV:
	  case DMF:
	      if (queue_dmf_receive(Index) != 0)
		/* Queue a timeout for it */
		Line->TimerIndex =
		  queue_timer(Line->TimeOut, Index, T_DMF_CLEAN);
	      break;
	  case ASYNC:
	      if (queue_async_receive(Index) != 0)
		Line->TimerIndex =
		  queue_timer(Line->TimeOut, Index, T_ASYNC_TIMEOUT);
	      break;
	  case DECNET:
	      if (queue_DECnet_receive(Index) != 0)
		Line->TimerIndex =
		  queue_timer(Line->TimeOut, Index,
			      T_DECNET_TIMEOUT);
		  break;
#ifdef EXOS
	  case EXOS_TCP:
	      if (queue_exos_tcp_receive(Index) != 0) {
		if (Line->state != ACTIVE)
		  Line->TimerIndex =
		    queue_timer(VMNET_INITIAL_TIMEOUT,
				Index, T_TCP_TIMEOUT);
		else
		  Line->TimerIndex =
		    queue_timer(Line->TimeOut, Index,
				T_TCP_TIMEOUT);
	      }
	      break;
#endif
#ifdef MULTINET_or_DEC
	  case DEC__TCP:
	  case MNET_TCP:
	      if (queue_mnet_tcp_receive(Index) != 0)

		/* If the link was not established yet, use longer timeout
		   (since the VMnet is running here in a locked-step mode
		   on the VM side). If we use the regular small timeout we
		   make the login process difficult (we transmit the next
		   DLE-ENQ packet while the other side is acking the previous
		   one. This is  caused due to slow lines). */

		if (Line->state != ACTIVE)
		  Line->TimerIndex =
		    queue_timer(VMNET_INITIAL_TIMEOUT,
				Index, T_TCP_TIMEOUT);
		else
		  Line->TimerIndex =
		    queue_timer(Line->TimeOut, Index,
				T_TCP_TIMEOUT);
	      break;
#endif
#endif /* VMS */
#ifdef UNIX
	  case UNIX_TCP: /* We poll here, so we don't queue a real receive */
	      Line->TimerIndex =
		queue_timer(Line->TimeOut, Index, T_TCP_TIMEOUT);
	      break;
#endif /* UNIX */
	  default:
	      logger(1, "IO: No support for device on line #%d\n", Index);
	      break;
	}
}


/*
 | Send the data using the correct interface. Before sending, add BCB+FCS and
 | CRC if asked for.
 | If the line is of TCP or DECnet type, we add only the first headers, and
 | do not add the tralier (DLE+ETB_CRCs) and we do not duplicate DLE's.
 | See comments about buffers at the head of this module.
 | If we are called when the link is already transmitting (and haven't finished
 | it), then the buffer is queued for later transmission if the line supports
 | it. The write AST routine will send it.
 */
void
send_data(Index, buffer, size, AddEnvelope)
const int	Index, size, AddEnvelope;	/* Add the BCB+...? */
const void	*buffer;
{
	struct	LINE	*Line;
	int	NewSize;
	const	unsigned char *SendBuffer;

	register int	i, flag;
	/* In the followings, ttr/b is used to point inside the buffer, while
	   Ttr/b is used to construct the buffer (RISC alignment..). */
	char		*ttb, *ttr;
	struct	TTB	Ttb;
	struct	TTR	Ttr;
#ifdef	USE_XMIT_QUEUE
	unsigned char *p;
	register int NextEntry;
#endif

	Line = &(IoLines[Index]);
	Line->flags &= ~F_XMIT_MORE;	/* Default - do not block */

	/* Collects stats */
	if (*(unsigned char *)buffer != NAK)
	  Line->stats.TotalOut++;
	else
	  Line->stats.RetriesOut++;

	SendBuffer = buffer;
	NewSize    = size;
	i = Line->OutBCB;
	flag = (Line->flags & F_RESET_BCB);
/* if (flag)
   logger(1,"IO: send_data(), F_RESET_BCB, line=%s\n",Line->HostName); */

#ifdef	USE_XMIT_QUEUE
	/* Test whether the link is already transmitting.
	   If so, queue the message only if the link supports so.
	   If not, ignore this transmission.			  */

	if (
#ifdef NBSTREAM
	    Line->WritePending != NULL ||
#endif
	    Line->LastXmitEntry != Line->FirstXmitEntry || /* Something in queue */
	    (Line->flags & F_SENDING) != 0) { /* Yes - it is occupied */
	  if ((Line->flags & F_RELIABLE) == 0) {
	    logger(1, "IO: Line %s doesn't support queueing\n",
		   Line->HostName);
	    return;		/* Ignore it */
	  }
	  Line->flags |= F_WAIT_V_A_BIT; /* Signal wait-a-bit so sender
					    will not transmit more */

	  /* Calculate where shall we put it in the queue (Cyclic queue) */
	  NextEntry = (Line->LastXmitEntry + 1) % MAX_XMIT_QUEUE;

	  /* If the new last is the same as the first one,
	     then we have no place... */
	  if (NextEntry == Line->FirstXmitEntry) {
	    int canwait = (Line->flags & F_XMIT_CAN_WAIT);
	    logger(1, "IO: No place to queue Xmit on line %s:%d can%s wait ABORTING\n",
		   Line->HostName,Line->CurrentStream, canwait ? "":"'t" );
	    Line->flags |= F_CALL_ACK;
bug_check("IO: Overflowed XMIT_QUEUE");
	    return;
	  }

	  /* There is a place - queue it */
	  if ((p = (unsigned char*)malloc(size + TTB_SIZE + 5 + 2 +
					  /* 5 for BCB+FCS overhead,
					     2 for DECnet CRC */
					  2 * TTR_SIZE)) == NULL) {
#ifdef VMS
	    logger(1, "IO, Can't malloc. Errno=%d, VaxErrno=%d\n",
		   errno, vaxc$errno);
#else
	    logger(1, "IO, Can't malloc. Errno=%d\n", errno);
#endif
	    bug_check("IO, Can't malloc() memory\n");
	  }

	  NewSize = add_VMnet_block(Line, AddEnvelope,
				    buffer, size, p + TTB_SIZE, i);
	  SendBuffer = p;
	  if (AddEnvelope == ADD_BCB_CRC)
	    if (flag != 0)	/* If we had to reset BCB, don't increment */
	      Line->OutBCB = (i + 1) % 16;

	  /* <TTB>(LN=length_of_VMnet+TTR) <VMnet_block> <TTR>(LN=0) */

	  Ttr.F = Ttr.U = 0;
	  Ttr.LN = 0;		/* Last TTR */
	  ttr = (void *)(p + NewSize + TTB_SIZE);
	  memcpy(ttr, &Ttr, TTR_SIZE);

	  NewSize += (TTB_SIZE + TTR_SIZE);
	  Ttb.F = 0;		/* No flags */
	  Ttb.U = 0;
	  Ttb.LN = htons(NewSize);
	  Ttb.UnUsed = 0;
	  ttb = (void *)(p + 0);
	  memcpy(ttb, &Ttb, TTB_SIZE);

	  Line->XmitQueue[Line->LastXmitEntry] = (char *)p;
	  Line->XmitQueueSize[Line->LastXmitEntry] = NewSize;
	  Line->LastXmitEntry = NextEntry;

	  /* Put a timer there waiting for us.. */

/* logger(2,"IO: XMIT-Queued %d bytes to line %s\n",NewSize,Line->HostName); */

	  queue_timer_reset(T_XMIT_INTERVAL, Index, T_XMIT_DEQUEUE);
	  Line->flags |= F_CALL_ACK;
	  return;
	}
#endif

	/* No queueing - format buffer into output buffer.
	   If the line is TCP - block more records if can.
	   Other types - don't try to block.			*/

	if ((Line->flags & F_RELIABLE) != 0) {
	  /* There are  DECNET, or TCP/IP links, which get TTB + TTRs */

	  if (Line->XmitSize == 0)
	    Line->XmitSize = TTB_SIZE;
	  /* First block - leave space for TTB */
	  NewSize = add_VMnet_block(Line, AddEnvelope, buffer, size,
				    Line->XmitBuffer + Line->XmitSize, i);
	  Line->XmitSize += NewSize;
	  if (AddEnvelope == ADD_BCB_CRC)
	    if (flag != 0)	/* If we had to reset BCB, don't increment */
	      Line->OutBCB = (i + 1) % 16;

	} else {		/* Normal block */

	  if (AddEnvelope == ADD_BCB_CRC) {
	    if ((Line->type == DMB) || (Line->type == DSV))
	      Line->XmitSize =
		NewSize = add_bcb(Index, buffer,
				  size, Line->XmitBuffer, i);
	    else
	      Line->XmitSize =
		NewSize = add_bcb_crc(Index, buffer,
				      size, Line->XmitBuffer, i);
	    if (flag != 0)	/* If we had to reset BCB, don't increment */
	      Line->OutBCB = (i + 1) % 16;

	  } else {
	    memcpy(Line->XmitBuffer, buffer, size);
	    Line->XmitSize = size;
	  }
	  SendBuffer = Line->XmitBuffer;
	}

	/* Check whether we've overflowed some buffer. If so - bugcheck... */

	if (Line->XmitSize > MAX_BUF_SIZE) {
	  logger(1, "IO: Xmit buffer overflow in line #%d\n", Index);
	  bug_check("Xmit buffer overflow\n");
	}

	/* If TcpIp line and there is room in buffer and the sender
	   allows us to defer sending - return. */

	if ((Line->flags & F_RELIABLE) != 0) {
	  if ((Line->flags & F_XMIT_CAN_WAIT) != 0 &&
	      (Line->XmitSize + TTB_SIZE +
	       2 * TTR_SIZE + 5 + 2 +
	       /* +5 for BCB + FCS overhead, +2 for DECnet;s CRC */
	       Line->MaxXmitSize) < Line->TcpXmitSize) { /* There is room */
	      Line->flags |= (F_XMIT_MORE | F_CALL_ACK);
	      return;
	    }

	  /* Ok - we have to transmit buffer. If DECnet or TcpIp - insert
	     the TTB and add TTR at end */

	  NewSize = Line->XmitSize;
	  ttb = (void *)Line->XmitBuffer;
	  ttr = (void *)(Line->XmitBuffer + NewSize);
	  Ttr.F = Ttr.U = 0;
	  Ttr.LN = 0;		/* Last TTR */
	  memcpy(ttr, &Ttr, TTR_SIZE);
	  Line->XmitSize = NewSize = NewSize + TTR_SIZE;
	  Ttb.F = 0;		/* No flags */
	  Ttb.U = 0;
	  Ttb.LN = htons(NewSize);
	  Ttb.UnUsed = 0;
	  memcpy(ttb, &Ttb, TTB_SIZE);
	  SendBuffer = Line->XmitBuffer;

	  /* Check whether we've overflowed some buffer. If so - bugcheck... */
	  if (Line->XmitSize > MAX_BUF_SIZE) {
	    logger(1, "IO, TCP Xmit buffer overflow in line #%d\n", Index);
	    bug_check("Xmit buffer overflow\n");
	  }
	}

	Line = &(IoLines[Index]);
#ifdef DEBUG
	logger(3, "IO: Sending: line=%s, size=%d, sequence=%d:\n",
	       Line->HostName, NewSize, i);
	trace(SendBuffer, NewSize, 5);
#endif
	switch(Line->type) {
#ifdef VMS
	  case ASYNC:
	      send_async(Index, SendBuffer, NewSize);
	      return;
	  case DMB:
	  case DSV:
	  case DMF:
	      send_dmf(Index, SendBuffer, NewSize);
	      return;
	  case DECNET:
	      send_DECnet(Index, SendBuffer, NewSize);
	      return;
#ifdef EXOS
	  case EXOS_TCP:
	      send_exos_tcp(Index, SendBuffer, NewSize);
	      return;
#endif
#ifdef MULTINET_or_DEC
	  case DEC__TCP:
	  case MNET_TCP:
	      send_mnet_tcp(Index, SendBuffer, NewSize);
	      return;
#endif
#endif
#ifdef UNIX
	  case UNIX_TCP:
	      send_unix_tcp(Index, SendBuffer, NewSize);
	      return;
#endif
	  default:
	      logger(1, "IO: No support for device on line #%d\n", Index);
	      break;
	}
}


/*
 | TCP lines - add the initial TCP header, and copy the buffer. Don't add the
 | final TTR, to allow blocking of more records. The caller routine will add
 | it.
 */
static int
add_VMnet_block(Line, flag, buffer, size, NewLine, BCBcount)
const int	flag, size, BCBcount;
struct LINE	*Line;
const void	*buffer;
void		*NewLine;
{
	struct	TTR	Ttr;
	char		*ttr;
	register int	NewSize;
	register unsigned char	*p;

	p = NewLine;
	NewSize = size + TTR_SIZE;
	if (flag == ADD_BCB_CRC)	/* Have to add BCB + FCS */
	  NewSize += 5;			/* DLE + STX + BCB + FCS + FCS */

	ttr = (void *)p;
	Ttr.F = 0;	/* No fast open */
	Ttr.U = 0;
	if (flag == ADD_BCB_CRC)
	  Ttr.LN = htons((size + 5));	/* The BCB header */
	else
	  Ttr.LN = htons(size);
	/* Copy the control blocks */
	memcpy(ttr, &Ttr, TTR_SIZE);
	p += TTR_SIZE;

	/* Put the DLE, STX, BCB and FCS.
	   If BCB is zero, send a "reset" BCB.	*/
	if (flag == ADD_BCB_CRC) {
	  *p++ = DLE; *p++ = STX;
	  if ((BCBcount == 0) && ((Line->flags & F_RESET_BCB) == 0)) {
	    Line->flags |= F_RESET_BCB;
	    *p++ = 0xa0;	/* Reset BCB count to zero */
	  } else
	    *p++ = 0x80 + (BCBcount & 0xf); /* Normal block */
	  *p++ = 0x8f; *p++ = 0xcf; /* FCS - all streams are enabled */
	}
	memcpy(p, buffer, size);
	return NewSize;
}


/*
 | Close the communication channel.
 */
void
close_line(Index)
const int	Index;
{
	IoLines[Index].flags &= ~F_SHUT_PENDING;	/* Clear the flag */

	switch(IoLines[Index].type) {
#ifdef VMS
	  case DMB:
	  case DSV:
	  case DMF:
	      close_dmf_line(Index);
	      break;
	  case ASYNC:
	      close_async_line(Index);
	      break;
	  case DECNET:
	      close_DECnet_line(Index);
	      break;
	  case DEC__TCP:
	  case MNET_TCP:
	  case EXOS_TCP:
	      close_tcp_chan(Index);
	      break;
#endif
#ifdef UNIX
	  case UNIX_TCP:
	      close_unix_tcp_channel(Index);
	      break;
#endif
	  default:
	      logger(1, "IO, Close-Line: line=%d, no support for device #%d\n",
		     Index, IoLines[Index].type);
	      break;
	}
}


/*================================= STATS ===========================*/
/*
 | Write the statistics, clear the counts and requeue the timer.
 */
void
compute_stats()
{
	int	i;
	struct	LINE	*Line;
	struct	STATS	*stats, *sum;

	for (i = 0; i < MAX_LINES; i++) {
	  Line = &(IoLines[i]);
	  if (*(Line->HostName) == 0) continue; /* no line */
	  stats = &Line->stats;
	  sum   = &Line->sumstats;

	  logger(2, "Stats for line #%d, name=%s, state=%d\n",
		 i, Line->HostName, Line->state);
	  logger(2, "Out: Total=%d, Wait-a-Bit=%d, Acks=%d, Messages=%d, NAKS=%d\n",
		 stats->TotalOut, stats->WaitOut, stats->AckOut,
		 stats->MessagesOut, stats->RetriesOut);
	  if (stats->TotalOut > 0)
	    logger(2, "     Util=%d%%, Messages bandwidth=%d%%\n",
		   (100 - 
		    (100*(stats->WaitOut+stats->AckOut+stats->MessagesOut)) /
		    stats->TotalOut),
		   (100 * stats->MessagesOut) / stats->TotalOut);
	  logger(2, "In:  Total=%d, Wait-a-Bit=%d, Acks=%d, Messages=%d, NAKS=%d\n",
		 stats->TotalIn, stats->WaitIn, stats->AckIn,
		 stats->MessagesIn, stats->RetriesIn);
	  if (stats->TotalIn > 0)
	    logger(2, "     Util=%d%%, Messages bandwidth=%d%%\n",
		   (100 -
		    (100*(stats->WaitIn + stats->AckIn + stats->MessagesIn)) /
		    stats->TotalIn),
		   (100 * stats->MessagesIn) / stats->TotalIn);

	  /* Reset the statistics */
	  
	  sum->TotalIn     += stats->TotalIn;     stats->TotalIn     = 0;
	  sum->TotalOut    += stats->TotalOut;    stats->TotalOut    = 0;
	  sum->WaitIn      += stats->WaitIn;      stats->WaitIn      = 0;
	  sum->WaitOut     += stats->WaitOut;     stats->WaitOut     = 0;
	  sum->AckIn       += stats->AckIn;       stats->AckIn       = 0;
	  sum->AckOut      += stats->AckOut;      stats->AckOut      = 0;
	  sum->RetriesIn   += stats->RetriesIn ;  stats->RetriesIn   = 0;
	  sum->RetriesOut  += stats->RetriesOut;  stats->RetriesOut  = 0;
	  sum->MessagesIn  += stats->MessagesIn;  stats->MessagesIn  = 0;
	  sum->MessagesOut += stats->MessagesOut; stats->MessagesOut = 0;
	}

	/* Use -1 so Delete_lines_timeout for line 0 will not clear us */
	queue_timer(T_STATS_INTERVAL, -1, T_STATS);
}

void
vmnet_monitor()
{
	struct LINE *Line = IoLines;
	int i;
	char vmnet_from[20];
	char vmnet_to[20];
	time_t now, timelim;

	now = time(NULL);
	timelim = now - (T_VMNET_INTERVAL << 2); /* Timelimit is four times the
						    probe interval (4 mins) */

	sprintf(vmnet_from,"VMNET@%s", LOCAL_NAME);

	for (i = 0; i < MAX_LINES; ++i, ++Line) {
	  if (*(Line->HostName) == 0) continue; /* no line */
	  if (Line->state != ACTIVE) continue;  /* we look only actives.. */

	  sprintf(vmnet_to,"@%s",Line->HostName);
	  send_nmr(vmnet_from, vmnet_to, "CPQ TIME", 8, ASCII, CMD_CMD);
	  if (GETTIMESEC(Line->InAge) < timelim) {
	    logger(1,"IO: VMNET-MON: Line %s not responding to our 'CPQ TIME' -probes! (age: %ds)\n",
		   Line->HostName, now-GETTIMESEC(Line->InAge));
	    Line->state = INACTIVE;
	    /* Will close line and put it into correct state */
	    restart_channel(i);
	  }
	}

	/* Use -1 so Delete_lines_timeout for line 0 will not clear us */
	queue_timer(T_VMNET_INTERVAL, -1, T_VMNET_MONITOR);
}

/**************** DEBUG section *********************************/
/*
 | Loop over all lines. For each line, if the xmit or receive buffer size is
 | non-zero, dump it.
 */
static void
debug_dump_buffers(UserName)
const char	*UserName;
{
	register int	i;
	register struct LINE	*Line;

	logger(1, "** IO, Debug-dump-buffers called by user %s\n", UserName);

	for (i = 0; i < MAX_LINES; i++) {
	  Line = &(IoLines[i]);
	  if (*(Line->HostName) == '\0')	/* Line not defined */
	    continue;
	  if (Line->XmitSize > 0) {
	    logger(1, "Line=%d, node=%s, xmit:\n",
		   i, Line->HostName);
	    trace(Line->XmitBuffer, Line->XmitSize, 1);
	  }
	  if (Line->RecvSize > 0) {
	    logger(1, "Line=%d, node=%s, recv:\n",
		   i, Line->HostName);
	    trace(Line->InBuffer, Line->RecvSize, 1);
	  }
	}
	logger(1, "** IO, End of buffers dump\n");
	dump_gone_list();
}


/*
 | Rescan the queue. Clear all the current queue (free all its memory) and
 | then rescan the queue to queue again all files. This is done after files
 | are manually renamed to be queued to another link. [VMS explanation]
 */
/*static*/ void
debug_rescan_queue(UserName,opt)
const char	*UserName, opt;
{
	register int	i;
	register struct LINE	*Line;
	int activecnt = 0;

	logger(1, "** IO: Debug-rescan-queue called by user %s, opt: `%c'\n", UserName,opt);

	for (i = 0; i < MAX_LINES; i++) {
	  Line = &(IoLines[i]);
	  activecnt = 0;
	  if (Line->HostName[0] != 0  &&  Line->QueueStart != NULL) {
	    activecnt = delete_file_queue(Line);
	  }
	  /* Line->QueuedFiles = activecnt;
	     Line->QueuedFilesWaiting = 0; */
	}

	if (opt != '-') {
	  logger(1, "** IO: Starting queue scan\n");
	  i = init_files_queue(); /* -1: Pipe-queue running
				      0: Pipe started
				      1: Sync queueing completed */
	  if (i > 0) {
	    logger(1, "** IO: Queue scan done.\n");
	  } else if (i == 0)
	    logger(1, "** IO: Queue scan started.\n");
	  else {
	    logger(1, "** IO: Queue scan already running.\n");
	    return;
	  }
	} else {
	  logger(1, "** IO: no queue rescan executed!\n");
	}
}


/*================== PARSE-COMMAND =================================*/
/*
 | Parse the command line received via the command mailbox/socket.
 */
void
parse_operator_command(line,length)
unsigned char	*line;
const int length;
{
	unsigned char	Faddress[SHORTLINE],	/* Sender for NMR messages */
			Taddress[SHORTLINE];	/* Receiver for NMR */
	int	i = 0;

	switch (*line) {	/* The first byte is the command code. */
	  case CMD_SHUTDOWN_ABRT:	/* Shutdown now */
	      send_opcom("FUNET-NJE: Immediate shutdown by operator request.");
	      if (PassiveSocketChannel >= 0)
		close(PassiveSocketChannel);
	      PassiveSocketChannel = -1;
	      MustShutDown = 1;
	      for (i = 0; i < MAX_LINES; i++)
		if (IoLines[i].HostName[0] != 0 &&
		    IoLines[i].state == ACTIVE) {
		  abort_streams_and_requeue(i);
		  IoLines[i].flags |= F_SHUT_PENDING;
		}
	      shut_gone_users();	/* Inform them */
#ifdef VMS
	      sys$wake(0,0);	/* Wakeup the main module */
#endif
	      break;
	  case CMD_SHUTDOWN:	/* Normal shudown */
	      logger(1, "Normal shutdown requested by %s\n", &line[1]);
	      MustShutDown = -1;	/* Signal it */
	      if (PassiveSocketChannel >= 0)
		close(PassiveSocketChannel);
	      PassiveSocketChannel = -1;
	      /* Mark all lines as needing shutdown */
	      for (i = 0; i < MAX_LINES; i++)
		IoLines[i].flags |= F_SHUT_PENDING;
	      shut_gone_users();	/* Inform them */
	      can_shut_down();	/* Maybe all lines are
				   already inactive? */
	      break;
	  case CMD_SHOW_LINES:
	      /* LINE contains username only.
		 The routine we call expect full address, so
		 add our local node name */
	      strcat(&line[1], "@");
	      strcat(&line[1], LOCAL_NAME);
	      show_lines_status(&line[1],0);
	      break;
	  case CMD_SHOW_QUEUE:
	      /* LINE contains username only.
		 The routine we call expect full address, so
		 add our local node name */
	      strcat(&line[1], "@");
	      strcat(&line[1], LOCAL_NAME);
	      show_files_queue(&line[1],"");
	      break;
	  case CMD_QUEUE_FILE: 
	      /* Compute the file's size in bytes: */
	      i = ((line[1] << 8) + (line[2])) * 512;
	      queue_file(&line[3], i, NULL, NULL);
	      break;
	  case CMD_SEND_MESSAGE:	/* Get the parameters */
	  case CMD_SEND_COMMAND:
	      if (sscanf(&line[2], "%s %s", Faddress, Taddress) != 2) {
		logger(1, "Illegal SEND_MESSAGE line: '%s'\n", &line[2]);
		break;	/* Illegal line */
	      }
	      /* The sender includes only the username.
		 Add @LOCAL-NAME to it. */
	      if (strchr(Faddress,'@') == NULL) {
		i = strlen(Faddress);
		Faddress[i++] = '@';
		strcpy(&Faddress[i], LOCAL_NAME);
	      }
	      if (*line == CMD_SEND_MESSAGE) {

		/* If there is no @ in the Taddress,
		   then append our local nodename, so it'll
		   be sent to a local user */

		if (strchr(Taddress, '@') == NULL) {
		  i = strlen(Taddress);
		  Taddress[i++] = '@';
		  strcpy(&Taddress[i], LOCAL_NAME);
		}
		send_nmr(Faddress, Taddress,
			 &line[line[1]], strlen(&line[line[1]]),
			 ASCII, CMD_MSG);
	      }
	      else
		send_nmr(Faddress, Taddress,
			 &line[line[1]], strlen(&line[line[1]]),
			 ASCII, CMD_CMD);
	      break;
	  case CMD_START_LINE:
	      i = line[1];
	      if ((i >= 0) && (i < MAX_LINES) &&
		 IoLines[i].HostName[0] != 0)
		restart_line(i);
	      else
		logger(1,"IO: Operator START LINE #%d on nonconfigured line\n",
		       i);
	      break;
	  case CMD_STOP_LINE:	/* Stop a line after the last file */
	      i = (line[1] & 0xff);
	      if ((i >= 0) && (i < MAX_LINES) &&
		  IoLines[i].HostName[0] != 0) {
		IoLines[i].flags |= F_SHUT_PENDING;
		logger(1, "IO: Operator requested SHUT for line #%d\n", i);
	      }
	      else
		logger(1, "IO: Illegal line numbr(%d) for STOP LINE\n", i);
	      break;
	  case CMD_FORCE_LINE:	/* Stop a line now */
	      i = (line[1] & 0xff);
	      if ((i >= 0) && (i < MAX_LINES) &&
		  IoLines[i].HostName[0] != 0) {
		logger(1, "IO: Line #%d  (%s) forced by operator request\n",
		       i, IoLines[i].HostName);
		if (IoLines[i].state == ACTIVE)
		  IoLines[i].flags |= F_SHUT_PENDING;
		/* Requeue all files back: */
		restart_channel(i);
	      }
	      else
		logger(1, "IO: Illegal line numbr(%d) for FORCE LINE\n", i);
	      break;
	  case CMD_DEBUG_DUMP:	/* Dump lines buffers */
	      debug_dump_buffers(&line[1]); /* Pass the username */
	      break;
	  case CMD_DEBUG_RESCAN:	/* Rescan queue */
	      debug_rescan_queue(&line[1],line[11]);
	      break;
	  case CMD_EXIT_RESCAN:
	      handle_sighup(0); /* Do it with the SIGHUP code.. */
	      break;
	  case CMD_ROUTE_RESCAN:
	      close_route_file();
	      open_route_file();
	      break;
	  case CMD_LOGLEVEL:	/* Set new loglevel */
	      i = LogLevel;
	      LogLevel = 1;	/* To close the file;
				   guaranteed log checkpoint */
	      logger(1, "IO: Log level changed from %d to %d\n",
		     i, line[1] & 0xff);
	      LogLevel = line[1] & 0xff;
	      break;
	  case CMD_CHANGE_ROUTE:	/* Change the route in our database */
	      sscanf(&line[1], "%s %s", Faddress, Taddress);
	      logger(1, "IO: Route %s changed to %s by operator\n",
		     Faddress, Taddress);
	      change_route(Faddress, Taddress);
	      break;
	  case CMD_GONE_ADD_UCP:
	  case CMD_GONE_ADD:
	      add_gone_user(&line[1]);
	      break;
	  case CMD_GONE_DEL_UCP:
	  case CMD_GONE_DEL:
	      del_gone_user(&line[1]);
	      break;
	  case CMD_MSG_REGISTER:
	      msg_register(line+1,length-1);
	      break;
	  default:
	      logger(1, "IO: Illegal command = %d\n", *line);
	      break;
	}
}
