/* UNIX.C	V3.5-mea1.1
 | Copyright (c) 1988,1989,1990,1991 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.


 | Extensive modifications for  FUNET-NJE, Copyright (c)
 | Finnish University and Research Network, FUNET, 1991, 1993, 1994

 | Sections: COMMAND - Command mailbox (socket)
 |           COMMUNICATION - Send messages to user and  opcom.
 |           TIMER - The one second timer process.
 |
 |
 */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef UNIX
#include <linux/stat.h>
#endif
#include <fcntl.h>
#include <strings.h>

#include "consts.h"
#include "headers.h"
#include "prototypes.h"
#ifdef	AF_UNIX
#include <sys/un.h>
#endif
#ifdef	AIX
#include <sys/select.h>
#endif /* AIX */

#define	TEMP_R_FILE		".NJE_R_"	/* Won't submit temp files.. */
#define	TEMP_R_EXT		"TMP"		/* before they are ready.. */

static int	CommandSocket = -1;	/* The command socket number */
struct sockaddr_in CommandAddress;	/* Incoming command originator addr */
/*static int      CommandAddressLen;*/
extern int	LogLevel;	/* So we change it with the LOGLEVEL CP command */

extern struct	SIGN_OFF	SignOff;
extern int	MustShutDown;
extern int	PassiveSocketChannel,	/* We called LISTEN on it	*/
		PassiveReadChannel;	/* We are waiting on it for
					   the initial VMnet record	*/

#define	MAX_QUEUE_ENTRIES	20	/* Maximum entries in timer's queue */
struct	TIMER	{			/* The timer queue		*/
		int	expire;		/* Number of seconds until
					   expiration			*/
		time_t	expiration;	/* Expiration time		*/
		int	  index;	/* Line's index			*/
		TimerType action;	/* What to do when expires	*/
	} TimerQueue[MAX_QUEUE_ENTRIES];

#define	EXPLICIT_ACK	0
#define	DELAYED_ACK	2		/* In accordance with PROTOOCL */


typedef enum {CMD_SOCKET, CMD_UDP, CMD_FIFO, CMD_FIFO0, CMD_UNKNOWN} cmdbox_t;
static cmdbox_t cmdbox = CMD_UNKNOWN;

#ifdef USE_XMIT_QUEUE
int dequeue_xmit_queue(const int Index);
#endif
void fileid_db_close();

static void  auto_restart_lines __(( void ));
static void  parse_op_command __(( int *fd ));
static void  parse_file_queue __(( const int fd ));

extern int	errno;
#ifdef __DARWIN_UNIX03
extern const int	sys_nerr;	/* Maximum error number recognised */
#else
extern int	sys_nerr;	/* Maximum error number recognised */
#endif
/* extern char	*sys_errlist[];	*/ /* List of error messages */
#define	PRINT_ERRNO	(errno > sys_nerr ? "***" : sys_errlist[errno])

u_int32 socket_access_key;

/*===================== COMMAND ============================*/
/*
 | Init the command socket. We use a datagram socket.
 	[mea] Using  mkfifo()  for it !
 */
void
init_command_mailbox()
{
	int	fd;
	char	path[LINESIZE];
	struct stat dstat;
	char *socknam = COMMAND_MAILBOX;

	/* Skip the initial token.. */
	while (*socknam != ' ' && *socknam != '\t' && *socknam != 0)
	  ++socknam;
	while (*socknam == ' ' || *socknam == '\t')
	  ++socknam;

	/* We need a random number soon.. */
#ifdef	USG
	srand((unsigned int)time(NULL));
#else
	srandom(time(NULL));
#endif

	cmdbox = CMD_UNKNOWN;

	if (*COMMAND_MAILBOX == 'F' ||
	    *COMMAND_MAILBOX == 'f') {

	  char *s;
	  int rc;

	  if (*socknam != '/') {
	    unlink(socknam);
	    rc = mkfifo(socknam,S_IFIFO|0660);
	    *s = 0;
	    if (rc==0 && stat(socknam,&dstat) == 0) {
	      *s = '/';
	      chown(socknam,0,dstat.st_gid);
	      chmod(socknam,0660);
	    } else {
	      logger(1,"UNIX: Can't mkfifo() COMMAND_MAILBOX, or no directory!  Error: %s\n",PRINT_ERRNO);
	      exit(9);
	    }
	  } else {
	    logger(1,"UNIX: Command mailbox FIFO bad parameters!\n");
	    exit(8);
	  }

	  CommandSocket = open(socknam,O_RDONLY|O_NDELAY,0);

	  if (*COMMAND_MAILBOX == 'f')
	    cmdbox = CMD_FIFO0;
	  else
	    cmdbox = CMD_FIFO;

	  /* End of FIFO-case */

	} else if (*COMMAND_MAILBOX == 'S') /* AF_UNIX SOCKET */ {

#ifndef AF_UNIX
	  logger(1,"UNIX: AF_UNIX socket for CMDMAILBOX is not available on this platform!\n");
	  exit(8);
#else

	  int i;
	  struct sockaddr_un	SocketName;

	  /* Create a local socket */
	  if ((CommandSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
	    logger(1, "UNIX, Can't create command socket, error: %s\n",
		   PRINT_ERRNO);
	    exit(1);
	  }

	  unlink(socknam);  /* in case there is one from
					 previous run.. */
	  bzero((char *) &SocketName, sizeof(SocketName));

	  /* Now, bind a local name for it */
	  SocketName.sun_family = AF_UNIX;
	  strcpy(SocketName.sun_path, socknam);

	  i = sizeof(SocketName.sun_family) + strlen(SocketName.sun_path);
	  if (bind(CommandSocket, (struct sockaddr *)&SocketName, i) < 0) {
	    logger(1, "UNIX, Can't bind command socket, error: %s\n",
		   PRINT_ERRNO);
	    exit(1);
	  }
	  if (listen(CommandSocket, 2) < 0) {
	    logger(1, "UNIX, Can't listen to command socket, error: %s\n",
		   PRINT_ERRNO);
	    exit(1);
	  }

	  cmdbox = CMD_SOCKET;
#endif
	} else if (*COMMAND_MAILBOX == 'U') /* UDP */ {

	  u_int32 iaddr;
	  struct	sockaddr_in	SocketName;
	  int portnum;
	  int on = 1;

	  /* Create a local socket */
	  if ((CommandSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
	    logger(1, "UNIX, Can't create command socket, error: %s\n",
		   PRINT_ERRNO);
	    exit(1);
	  }

	  setsockopt(CommandSocket, SOL_SOCKET, SO_REUSEADDR,
		     (char*)&on, sizeof(on));

	  /* Now, bind a local name for it */
	  memset(&SocketName,0,sizeof SocketName);
	  SocketName.sin_family      = AF_INET;
	  SocketName.sin_addr.s_addr = htonl(INADDR_ANY);
	  iaddr = inet_addr(socknam);
	  if (iaddr != 0xFFFFFFFFL)
	  SocketName.sin_addr.s_addr = iaddr;
	  portnum = 175;
	  sscanf(socknam,"%*s %d", &portnum);
	  SocketName.sin_port = htons(portnum);

	  if (bind(CommandSocket, &SocketName, sizeof(SocketName)) == -1) {
	    logger(1, "UNIX, Can't bind command socket '%s', error: %s\n",
		   socknam, PRINT_ERRNO);
	    exit(1);
	  }

	  cmdbox = CMD_UDP;

	} else {
	  logger(1, "UNIX: Unknown CMDMAILBOX method (S=Socket, F=Fifo, U=UDP are known!)\n");
	  exit(8);
	}

	sprintf(path,"%s/.socket.key",BITNET_QUEUE);
	unlink(path);
	if ((fd = open(path,O_WRONLY|O_CREAT|O_TRUNC,0660)) < 0) {
	  logger(1, "UNIX: Can't create `%s' to hold access authorization key\n",path);
	  exit(1);
	}
	fstat(fd,&dstat);
#ifdef	USG
	sleep((rand() % 5)+1); /* Delay a random time.. */
	/* Auth key is some random value.. */
	socket_access_key = rand() ^ (dstat.st_ino ^ dstat.st_mtime);
	if (socket_access_key == 0)		/* Can't be 0 twice! */
	  socket_access_key = rand() ^ (dstat.st_ino ^ dstat.st_mtime);
#else
	sleep((random() % 5)+1); /* Delay a random time.. */
	/* Auth key is some random value.. */
	socket_access_key = random() ^ (dstat.st_ino ^ dstat.st_mtime);
	if (socket_access_key == 0)		/* Can't be 0 twice! */
	  socket_access_key = random() ^ (dstat.st_ino ^ dstat.st_mtime);
#endif
	write(fd,(void*)&socket_access_key,4);
	close(fd);
	if (stat(BITNET_QUEUE,&dstat) == 0) {
	  chown(path,-1,dstat.st_gid);
	  chmod(path,0440);
	}
	/* That's all. Select will poll for it */
}

/*
 | Close the operator communication channel (we are shutting down).
 */
void
close_command_mailbox()
{
	close(CommandSocket);
	CommandSocket = -1;
}

/*================= COMMUNICATION ========================*/
/*
 | Send a message to console and log it in our logfile.
 */
void
send_opcom(text)
const char *text;
{
	int	ftty;
	char	line[512];

	logger(1, "OPCOM message: %s\n", text);

	/* Too long to handle ? */
	if (strlen(text) > sizeof(line)-5) return;

	sprintf(line, "\r%s\r\n", text);

	/* [mea] XXX: Not always /dev/console! */
	if ((ftty = open("/dev/console", O_WRONLY|O_NOCTTY, 0644)) < 0)
	  return;

	write(ftty, line, strlen(line));
	close(ftty);
}


/*================ TIMER ============================*/
/*
 | Init the timer's queue entries (zero them), queue an entry to tick each
 | second to poll the active sockets.
 | The timer's AST is called by the main routine wach second.
 */
void
init_timer()
{
	long	i;

	/* Zero the queue */
	for (i = 0; i < MAX_QUEUE_ENTRIES; i++)
	  TimerQueue[i].expire = 0;
}

/*
 | The AST routine that is waked-up once a second.
 */
time_t
timer_ast()
{
	long	i;
	struct	LINE	*Line;
	time_t	now;

	time(&now);

	/* Loop over all active timeouts and decrement them.
	   If expired, handle accordingly.
	 */
	for (i = 0; i < MAX_QUEUE_ENTRIES; ++i) {
	  if (TimerQueue[i].expire != 0) {	/* Active	*/
	    /* if ((--TimerQueue[i].expire) == 0) { */
	    if (TimerQueue[i].expiration <= now) {
	      TimerQueue[i].expire = 0;
	      switch (TimerQueue[i].action) {
		case T_SEND_ACK:		/* Send ACK	*/
#ifdef NBSTREAM
		    Line = &(IoLines[TimerQueue[i].index]);
		    if (Line->WritePending != NULL) {
		      /* Requeue it: */
		      Line->TimerIndex = queue_timer(Line->TimeOut,
						     TimerQueue[i].index,
						     T_SEND_ACK);
		      break;
		    }
#endif
		    handle_ack(TimerQueue[i].index, DELAYED_ACK);
		    break;
		case T_TCP_TIMEOUT:
		    /* Read timeout, Simulate an ACK if needed */
		    Line = &(IoLines[TimerQueue[i].index]);
		    if (Line->state == ACTIVE
#ifdef NBSTREAM
			&& Line->WritePending == NULL
#endif
			) {
		      handle_ack(TimerQueue[i].index, EXPLICIT_ACK);
		    }
		    /* Requeue it: */
		    Line->TimerIndex = queue_timer(Line->TimeOut,
						   TimerQueue[i].index,
						   T_TCP_TIMEOUT);
		    break;
		case T_AUTO_RESTART:
		    auto_restart_lines();
		    queue_timer(T_AUTO_RESTART_INTERVAL, -1, T_AUTO_RESTART);
		    break;
		case T_STATS:	/* Compute statistics */
		    compute_stats();
		    break;
		case T_VMNET_MONITOR:
		    vmnet_monitor();
		    break;
#ifdef USE_XMIT_QUEUE
		case T_XMIT_DEQUEUE:
		    if (dequeue_xmit_queue(TimerQueue[i].index))
		      queue_timer(T_XMIT_INTERVAL, TimerQueue[i].index,
				  T_XMIT_DEQUEUE);
		    break;
#endif
		default:
		    logger(1, "UNIX, No timer routine, code=d^%d\n",
			   TimerQueue[i].action);
		    break;
	      }
	    }
	  }
	}
	return 0;
}



/*
 | Queue an entry. The line index and the timeout us expected.  Since we don't
 | use a 1-second clock, but a SLEEP call, we do not add 1 to the timeout value
 | as is done on the VMS.
 | A constant which decalres the routine to call on timeout is also expected.
 */
int
queue_timer(expiration, Index, WhatToDo)
int		expiration;
TimerType	WhatToDo;
int		Index;			/* Index in IoLines array */
{
	int	i;
	time_t	now;

	time(&now);

	for (i = 0; i < MAX_QUEUE_ENTRIES; ++i) {
	  if (TimerQueue[i].expire == 0) { /* Free */
	    TimerQueue[i].expire     = expiration;
	    TimerQueue[i].expiration = expiration+now;
	    TimerQueue[i].index      = Index;
	    TimerQueue[i].action     = WhatToDo;
	    return i;
	  }
	}

	/* Not found an entry - bug check */
	logger(1, "UNIX, Can't queue timer, timer queue dump:\n");
	for (i = 0; i < MAX_QUEUE_ENTRIES; ++i)
	  logger(1, "  #%d, Expire=%d, index=%d, action=%d\n",
		 i, TimerQueue[i].expire,
		 TimerQueue[i].index, TimerQueue[i].action);
	bug_check("UNIX, can't queue timer entry.");
	return 0;	/* To make LINT quiet... */
}

void
queue_timer_reset(expiration, Index, WhatToDo)
int		expiration;
TimerType	WhatToDo;
int		Index;			/* Index in IoLines array */
{
	int	i;
	time_t	now;

	time(&now);

	for (i = 0; i < MAX_QUEUE_ENTRIES; ++i) {
	  if (TimerQueue[i].index == Index &&
	      TimerQueue[i].action == WhatToDo) {
	    TimerQueue[i].expire     = expiration;
	    TimerQueue[i].expiration = expiration+now;
	    TimerQueue[i].index      = Index;
	    TimerQueue[i].action     = WhatToDo;
	    return;
	  }
	}
	queue_timer(expiration, Index, WhatToDo);
}

/*
 | Dequeue a timer element (I/O completed ok, so we don't need this timeout).
 */
void
dequeue_timer(TimerIndex)
int	TimerIndex;		/* Index into our timer queue */
{
	if (TimerQueue[TimerIndex].expire == 0) {
	  /* Dequeueing a non-existent entry... */
	  logger(1, "UNIX, Nonexistent timer entry; Index=%d, action=%d\n",
		 TimerQueue[TimerIndex].index,
		 TimerQueue[TimerIndex].action);
	  logger(1, "      Dequeued it.\n");
	}

	/* Mark this entry as free */
	TimerQueue[TimerIndex].expire = 0;
}

/*
 | Delete all timer entries associated with a line.
 */
void
delete_line_timeouts(Index)
int	Index;			/* Index in IoLines array */
{
	register int	i;

	for (i = 0; i < MAX_QUEUE_ENTRIES; i++) {
	  if (TimerQueue[i].index == Index) /* For this line - clear it */
	    TimerQueue[i].expire = 0;
	}
}


/*
 | Loop over all lines. If some line is in INACTIVE to RETRY state and the
 | AUTO-RESTART flag is set, try starting it.
 */
static void
auto_restart_lines()
{
	int	i;
	struct	LINE	*Line;

	for (i = 0; i < MAX_LINES; i++) {
	  Line = &IoLines[i];
	  if (*Line->HostName == 0) continue;	/* Not defined... */
	  if ((Line->flags & F_AUTO_RESTART) == 0)
	    continue;			/* Doesn't need restarts at all */
	  switch (Line->state) {	/* Test whether it needs restart */
	      case INACTIVE:
	      case RETRYING:
		  if ( -- Line->RetryPeriod > 0)	/* Not yet.. */
		    break;
		  restart_line(i);	/* Yes - it needs */
		  break;
	      default:
		  break;		/* Other states - no need */
	  }
	}
}


/*
 | Use the Select function to poll for sockets status.
 | We distinguish between two types:
 |   - The command mailbox, and
 |   - the NJE/TCP sockets.
 | If there was some processing to do, we repeat this call,
 | since the processing done might take more than 1 second,
 | and our computations of timeouts might be too long.
 |
 | If FD_SET is defined, then we should use the system
 | supplied macros and data structure for Select() call.
 */
void
poll_sockets()
{
	int	j, nfds;
	struct	timeval	timeout;
	int	FdWidth = 0;
	static	int passiv_reinit = 0;
	int	has_pending_connects = 0;
	int	done_only_implicitic_ack = 1;
	int	againcount = 0;
	extern  int file_queuer_pipe;

#ifdef AIX
	struct {
		fd_set  fdsmask;
		fd_set  msgsmask;
	} readfds, writefds;
#define _FD_SET(sock,var) FD_SET(sock,&var.fdsmask)
#define _FD_CLR(sock,var) FD_CLR(sock,&var.fdsmask)
#define _FD_ZERO(var) FD_ZERO(&var.fdsmask)
#define _FD_ISSET(i,var) FD_ISSET(i,&var.fdsmask)
#else
#if	defined(FD_SET)
	fd_set	readfds, writefds;
#define _FD_SET(sock,var) FD_SET(sock,&var)
#define _FD_CLR(sock,var) FD_CLR(sock,&var)
#define _FD_ZERO(var) FD_ZERO(&var)
#define _FD_ISSET(i,var) FD_ISSET(i,&var)
#else /* No AIX, nor FD_SET */
	long	readfds, writefds;	/* On most other machines */
#define _FD_SET(sock,var) var |= (1 << sock)
#define _FD_CLR(sock,var) var &= ~(1 << sock)
#define _FD_ZERO(var) var = 0
#define _FD_ISSET(i,var) ((var & (1 << i)) != 0)
#endif /* no AIX, nor FD_SET */
#endif /* no AIX, nor FD_SET */

	/* If the passive connection acceptance didn't come online
	   for some reason (channel FD is  -1), retry it every once
	   in a while, until we have it. */
	if (PassiveSocketChannel == -1 && MustShutDown == 0) {
	  if (passiv_reinit < 1) {
	    /* Try to open it.. */
	    init_passive_tcp_connection(0);
	    passiv_reinit = 20; /* 20 visits later again.
				   5 to 20 seconds.. */
	    if (PassiveSocketChannel >= 0)
	      logger(1,"Successfull at reiniting the PassiveSocketChannel\n");
	  }
	  --passiv_reinit;
	}

    again:

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;	/* 1 second timeout */

	_FD_ZERO(readfds);
 	_FD_ZERO(writefds);

	has_pending_connects = 0;

	if (CommandSocket >= 0) {
	  if (CommandSocket >= FdWidth)
	    FdWidth = CommandSocket+1;

	  _FD_SET(CommandSocket, readfds);

	  /*if (passiv_reinit < 1)
	    logger(2,"poll(): CommandSocket fd=%d\n",CommandSocket);*/
	}

	if (file_queuer_pipe >= 0) {
	  if (file_queuer_pipe >= FdWidth)
	    FdWidth = file_queuer_pipe+1;

	  _FD_SET(file_queuer_pipe, readfds);

	  /*if (passiv_reinit < 1)
	    logger(2,"poll(): file_queuer_pipe fd=%d\n",file_queuer_pipe);*/
	}

/* Add the sockets used for NJE communication */
	for (j = 0; j < MAX_LINES; ++j) {
	  if (IoLines[j].HostName[0] == 0) continue; /* No defined line */

	  if (IoLines[j].socket >= FdWidth)
	    FdWidth = IoLines[j].socket+1;
	  if ((IoLines[j].state != INACTIVE) &&
	      (IoLines[j].state != SIGNOFF) &&
	      (IoLines[j].state != RETRYING) &&
	      (IoLines[j].state != LISTEN) &&
	      (IoLines[j].socket >= 0)) {

#if	defined(NBCONNECT) || defined(NBSTREAM)
	    /* Can't do connects which take time to finish.. */

	    if (IoLines[j].socketpending >= 0) {
	      /* A pending open.. */

	      _FD_SET(IoLines[j].socket, writefds);

	      has_pending_connects = 1;
	    } else 
#endif
	    {
	      /* Normal stream, already open */
	      _FD_SET(IoLines[j].socket, readfds);
	    }
#if	NBSTREAM
	    if (IoLines[j].state == ACTIVE) {
	      if (IoLines[j].WritePending)
		/* We have a write pedning */
		_FD_SET(IoLines[j].socket, writefds);

	      /*
		 Now THIS is hairy (as if you didn't know it..)
		 We do accelerated (not waiting timeout) implicitic
		 ack, when there the  call-ack-flag is set, AND there
		 are active streams, OR the flag is set, AND shutdown
		 is not imminent, AND there are files in queue.

	       */

	      else if ((IoLines[j].flags & F_CALL_ACK) != 0	&&
		       ((IoLines[j].ActiveStreams != 0)		||
			((IoLines[j].flags & F_SHUT_PENDING) == 0 &&
			 (IoLines[j].QueuedFilesWaiting > 0))))
		/* Or we are expected to call ACK */
		_FD_SET(IoLines[j].socket, writefds);

	    }
#endif	/* NBSTREAM */
	  }
	}

	/* Queue a select for the passive end;
	   do it only if we did succeed binding it */

	if (PassiveReadChannel >= 0) {
	  if (PassiveReadChannel >= FdWidth)
	    FdWidth = PassiveReadChannel+1;

	  _FD_SET(PassiveReadChannel, readfds);

	}

	/* Do only one accept on the passive side at the time.
	   PassiveReadChannel is needed for link login,
	   others can wait.				*/

	if (PassiveReadChannel < 0 &&
	    PassiveSocketChannel >= 0) {
	  if (PassiveSocketChannel >= FdWidth)
	    FdWidth = PassiveSocketChannel+1;

	  /* Expecting connections on it */
	  _FD_SET(PassiveSocketChannel, readfds);
	}

	if ((nfds = select(FdWidth,
			   &readfds,
#if	defined(NBSTREAM) || defined(WRITESELECT)
			   &writefds,
#else
			   has_pending_connects ? &writefds : NULL,
#endif
			   NULL, &timeout)) == -1) {
	  if (errno == EINTR) return; /* Ah well, happens at the debugger */
	  logger(1, "UNIX, Select error, error: %s\n", PRINT_ERRNO);
	  bug_check("UNIX, Select error");
	}

	if (nfds == 0) {		/* Nothing is there */
	  /* We are done with input. When TcpIp sends data,
	     it sets the F_CALL_ACK flag, so we need to call
	     handle-Ack from here.
	   */
#ifdef NBSTREAM
	  timeout.tv_sec = 0;
	  timeout.tv_usec = 0;
	  _FD_ZERO(writefds);
	  FdWidth = 0;
	  for (j = 0; j < MAX_LINES; ++j)
	    if (IoLines[j].HostName[0] != 0	&&
		IoLines[j].socket >= 0		&&
		IoLines[j].WritePending == NULL	&&
		(IoLines[j].flags & F_CALL_ACK)) {
	      _FD_SET(IoLines[j].socket, writefds);
	      if (IoLines[j].socket >= FdWidth)
		FdWidth = IoLines[j].socket;
	    }
	  nfds = select(FdWidth+1,NULL,&writefds,NULL,&timeout);
	  for (j = 0; j < MAX_LINES; ++j) {
	    if (IoLines[j].HostName[0] != 0	&&
		IoLines[j].socket >= 0		&&
		_FD_ISSET(IoLines[j].socket, writefds)) {
	      IoLines[j].flags &= ~ F_CALL_ACK;
#ifdef USE_XMIT_QUEUE
	      dequeue_xmit_queue(j);
#endif
	      handle_ack(j, EXPLICIT_ACK);
	    }
	  }
#else	/* Not NBSTREAM */
	  for (j = 0; j < MAX_LINES; ++j) {
	    if ((IoLines[j].flags & F_CALL_ACK) != 0) {
	      IoLines[j].flags &= ~ F_CALL_ACK;
#ifdef USE_XMIT_QUEUE
	      dequeue_xmit_queue(j);
#endif
	      handle_ack(j, EXPLICIT_ACK);
	    }
	  }
#endif
	  return;
	}

	/* Check which channels have data to read.
	   Differentiate between 2 types of channels:
	   - the command socket and,
	   - NJE/TCP channels.				*/

	if (CommandSocket >= 0 &&
	    _FD_ISSET(CommandSocket, readfds)) {
	  parse_op_command(&CommandSocket);
	  done_only_implicitic_ack = 0;
	}

	if (file_queuer_pipe >= 0 &&
	    _FD_ISSET(file_queuer_pipe, readfds)) {
	  parse_file_queue(file_queuer_pipe);
	  done_only_implicitic_ack = 0;
	}

	/* Check if there is a connection to be accepted */
	if (PassiveSocketChannel >= 0 &&
	    _FD_ISSET(PassiveSocketChannel, readfds)) {
	  accept_tcp_connection();
	  done_only_implicitic_ack = 0;
	}

	/* We've accepted some connection;
	   now see whether there is something to read there */
	if (PassiveReadChannel >= 0 &&
	    _FD_ISSET(PassiveReadChannel, readfds)) {
	  read_passive_tcp_connection();
	  done_only_implicitic_ack = 0;
	}


	for (j = 0; j < MAX_LINES; ++j)
	  if (IoLines[j].HostName[0] != 0	&&
	      IoLines[j].socket >= 0) {

	    /* Handle all read-streams -- well, most.. */

	    if (_FD_ISSET(IoLines[j].socket, readfds)) {
	      unix_tcp_receive(j, &IoLines[j]);
	      done_only_implicitic_ack = 0;
	    }

	    /* Handle all write-streams */

	    if (IoLines[j].socket >= 0 && /* we may done read() on it and
					     got zero-size packet -> eof.. */
		_FD_ISSET(IoLines[j].socket, writefds)) {

#if	defined(NBSTREAM)||defined(NBCONNECT)
	      /* Check possible pending sockets.. */
	      if (IoLines[j].socketpending >= 0) {
		init_active_tcp_connection(j,1); /* Finalize that open */
		done_only_implicitic_ack = 0;
	      } else
#endif /* NB-connect/stream */
	      /* now handle writes */
#ifdef	NBSTREAM
	      if (IoLines[j].WritePending != NULL) {
		tcp_partial_write(j, 0);
#ifdef USE_XMIT_QUEUE
		dequeue_xmit_queue(j);
#endif
		done_only_implicitic_ack = 0;
	      } else
#endif /* NBSTREAM */

		/* Check if the socket is free to write, and we have
		   F_CALL_ACK -flag set */

		if ((IoLines[j].flags & F_CALL_ACK) != 0) {
		  IoLines[j].flags &= ~ F_CALL_ACK;
#ifdef USE_XMIT_QUEUE
		  dequeue_xmit_queue(j);
#endif
		  handle_ack(j, EXPLICIT_ACK);
		}
	    }
	  }
	/* Loop here for a moment if there is work, however no more
	   than 20 rounds before going to the main-program. */
	if (++againcount < 20) goto again;
	/* if (!done_only_implicitic_ack) goto again; */
}

/* ================================================================ */
static void
parse_file_queue(fd)
const int fd;
{
	unsigned char line[LINESIZE];
	int size;
	long FileSize;
	int rc;
	extern int file_queuer_pipe;
	extern int file_queuer_cnt;

	if ((rc = readn(fd,line,1)) == 1) {
	  size = line[0];
	  rc = readn(fd,line+1,size);
	  if (rc == size) {
	    FileSize = ((line[1] << 8) + line[2]) * 512;
logger(1,"parse_file_queuer(): Queue file \"%s\" siz=%d\n",line+3,FileSize);
	    queue_file(line+3, FileSize, NULL, NULL);
	  } else {
	    logger(1,"parse_file_queue(): rc != size on nread()\n");
	  }
	  ++file_queuer_cnt;
	}
	if (rc < 1) /* -1 (=err), or 0 (=EOF) */ {
	  close(file_queuer_pipe);
	  file_queuer_pipe = -1;
logger(1, "parse_file_queuer() cnt=%d\n", file_queuer_cnt);
	}
}

/* ================================================================ */

/*
 | Read from the socket and parse the command.
 */
static void
parse_op_command(CommandSocket)
int *CommandSocket;
{
	char	*p, line[LINESIZE];
	int	size = 0;

	if (cmdbox == CMD_FIFO || cmdbox == CMD_FIFO0) {

	  if ((size = read(*CommandSocket, line, sizeof line)) < 0) {
	    logger(1,"UNIX: CommandSocket read failed: %s\n",PRINT_ERRNO);
	    return;
	  }

	  if (cmdbox == CMD_FIFO0) {
	    if (size == 0) {
	      close(*CommandSocket);
	      *CommandSocket = open(COMMAND_MAILBOX,O_RDONLY|O_NDELAY,0);
	      return;
	    }
	  }
	} else if (cmdbox == CMD_SOCKET) {
#ifndef AF_UNIX
	  bug_check("UNIX: CMDMAILBOX has accepted SOCKET mode without AF_UNIX!\n");
#else

	  int	NewSock, i;
	  struct  sockaddr_un  cli_addr;
	  
	  i = sizeof(cli_addr);	

	  if ((NewSock = accept(*CommandSocket,
				(struct sockaddr *)&cli_addr, &i)) < 0) {
	    logger(1,"UNIX: Can't Accept control connection !\n");
	    return;
	  } else {	

	    if ((size = read(NewSock, &line, sizeof line)) < 0) {
	      logger(1,"UNIX: Can't read from control socket !\n");
	    }
	    close(NewSock);
	  }
#endif
	} else if (cmdbox == CMD_UDP) {

	  if ((size = recv(*CommandSocket, line, sizeof line, 0)) <= 0) {
	    perror("Recv");
	    return;
	  }
	}

	logger(5,"UNIX: COMMAND_MAILBOX received %d bytes.\n",size);

	if (size < 5) return; /* Hmm... */
	line[size] = '\0';
	if ((p = strchr(line+4, '\n')) != NULL) *p = '\0';

	/* Match the access key.. */
	if (socket_access_key != *(u_int32 *)&(line[0]) ||
	    *(u_int32 *)&(line[0]) == 0) {
	  logger(1, "UNIX: Somebody spoofed command access, key mismatch!\n");
	  return;
	}

	/* Parse the command. In most commands, the parameter is
	   the username to broadcast the reply to. */
	parse_operator_command(line+4,size-4);
}


/* ================================================================
   handle_*() routines by Matti Aarnio <mea@nic.funet.fi>
 */
#ifdef BSD_SIGCHLDS

void
handle_childs(n)  /* signal(SIGCHLD,handle_childs); */
const int n;
{
#ifdef	_POSIX_SOURCE	/* WE DO  waitpid()  */
	int status;
	/* int options; */
	int pid;

	for (;;) {

	  /* It seems that normal wait() without explicitic calls to this
	     procedure can work also.  All tests indicate that polled
	     handle_childs() just returns because of (pid <= 0). Always.
	     Interrupt based handling has taken care of dead child.
	     This is true at least on  SunOS 4.0.3			*/

	  pid = waitpid (-1 /* Any child */, &status, WNOHANG);
	  if (pid <= 0) return ; /* No childs waiting for us. */

	  /* We salvage dead childrens and make sure we don't gather zombies */
	  if (WIFSTOPPED(status))
	    logger(2,"SIGCHLD: child stopped! pid=%d, signal=%d\n",
		   pid,WSTOPSIG(status));
	  else if (WIFSIGNALED(status))
	    logger(2,"SIGCHLD: child sig-terminated, pid=%d, signal=%d%s\n",
		   pid,WTERMSIG(status),(0200 & status)?", core dumped":"");
	  else if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
	    logger(2,"SIGCHLD: child exited, pid=%d, status=%d\n",pid,
		   WEXITSTATUS(status));
	}
	/* NOT REACHED */
#else /* !_POSIX_SOURCE */
	union wait status;
	struct rusage rusage;
	int pid;

	for (;;) {

	  /* It seems that normal wait() without explicitic calls to this
	     procedure can work also.  All tests indicate that polled
	     handle_childs() just returns because of (pid <= 0). Always.
	     Interrupt based handling has taken care of dead child.
	     This is true at least on  SunOS 4.0.3			*/

	  pid = wait3 (&status,WNOHANG,&rusage);
	  if (pid <= 0) return ; /* No childs waiting for us. */

	  /* We salvage dead childrens and make sure we don't gather zombies */
	  if (WIFSTOPPED(status))
	    logger(2,"SIGCHLD: child stopped! pid=%d, signal=%d\n",
		   pid,status.w_stopsig);
	  else if (WIFSIGNALED(status))
	    logger(2,"SIGCHLD: child sig-terminated, pid=%d, signal=%d%s\n",
		   pid,status.w_termsig,status.w_coredump?", core dumped":"");
	  else if (WIFEXITED(status) && status.w_retcode != 0)
	    logger(2,"SIGCHLD: child exited, pid=%d, status=%d\n",pid,
		   status.w_retcode);
	}
	/* NOT REACHED */
#endif /* !_POSIX_SOURCE */
}
#endif /* BSD_SIGCHLDS */


void
handle_sighup(n) /* signal(SIGHUP,handle_sighup); */
const int n;
{
	/*  Right, we have been HUPed.  Thus we must go and re-read
	    exit config databases.  Should we also drop connections ? */

	extern FILE *LogFd; /* In main.c */

	signal(SIGHUP,handle_sighup); /* Keep it active on SYSVs, doesn't
					 hurt BSDs.. */

	logger(1,"==============================================\n");
	logger(1,"SIGHUP: Rescanning exits, restarting log file.\n");
	logger(1,"==============================================\n");

	close_route_file();
	rscsacct_close();
	fileid_db_close();

	if (open_route_file() == 0) {
	  logger(1,"******* AIEE!!!  CAN'T RE-OPEN ROUTE FILE! *****\n");
	  send_opcom("FUNET-NJE ABORTING! Problems RE-OPENING the route table!");
	  exit(1);
	}

	if (*FileExits)
	  read_exit_config(FileExits); /* ==0 OK, others: error */

	if (*MsgExits)
	  read_message_config(MsgExits); /* ==0 OK, others: error */

	if (LogFd) {  /* Close also log file, and mark it closed */
	  fclose(LogFd);
	  LogFd = NULL;
	}

	rscsacct_open(RSCSACCT_LOG);

	logger(1,"================================================\n");
	logger(1,"SIGHUP: Rescanning done, New(?) log file opened.\n");
	logger(1,"================================================\n");
}


void
log_line_stats()
{

	register int i;
	struct STATS *S, *stats;

	logger(0,"STATS: Line statistics over whole FUNETNJE process runtime:\n\n");

	for (i=0; i<MAX_LINES; ++i) {
	  S     = & IoLines[i].sumstats;
	  stats = & IoLines[i].stats;

	  S->TotalIn     += stats->TotalIn;     stats->TotalIn     = 0;
	  S->TotalOut    += stats->TotalOut;    stats->TotalOut    = 0;
	  S->WaitIn      += stats->WaitIn;      stats->WaitIn      = 0;
	  S->WaitOut     += stats->WaitOut;     stats->WaitOut     = 0;
	  S->AckIn       += stats->AckIn;       stats->AckIn       = 0;
	  S->AckOut      += stats->AckOut;      stats->AckOut      = 0;
	  S->RetriesIn   += stats->RetriesIn ;  stats->RetriesIn   = 0;
	  S->RetriesOut  += stats->RetriesOut;  stats->RetriesOut  = 0;
	  S->MessagesIn  += stats->MessagesIn;  stats->MessagesIn  = 0;
	  S->MessagesOut += stats->MessagesOut; stats->MessagesOut = 0;

	  if (IoLines[i].HostName[0] == 0)
	    continue; /* Nothing on this line */

	  logger(0,"STATS: line %d  Host `%s'\n",i,IoLines[i].HostName);
	  logger(0,"STATS: line %d  TotalIn %d  TotalOut %d  WaitIn %d  WaitOut %d\n",
		 i,S->TotalIn,S->TotalOut,S->WaitIn,S->WaitOut);
	  logger(0,"STATS: line %d  AckIn %d  AckOut %d  MsgsIn %d  MsgsOut %d\n",
		 i,S->AckIn,S->AckOut,S->MessagesIn,S->MessagesOut);
	  logger(0,"STATS: line %d  RetriesIn %d  RetriesOut %d\n\n",
		 i,S->RetriesIn,S->RetriesOut);
	}
}


void
handle_sigterm(n) /* signal(SIGTERM,handle_sigterm); */
const int n;
{
	/*  Right, we have been TERMed.
	    Thus we must go and close everything.  Flush counters... */

	signal(SIGTERM,SIG_DFL);	/* Next TERM -> kill me... */

	logger(0,"===============================\n");
	logger(0,"SIGTERM: Terminating, counters:\n");
	logger(0,"===============================\n");

	log_line_stats();
}

void
handle_sigusr1(n) /* signal(SIGUSR1,handle_sigusr1); */
const int n;
{
	/*  Right, we have been USR1ed.
	    Thus we must go and close everything.  Flush counters... */

	signal(SIGUSR1,handle_sigusr1);

	logger(0,"==================\n");
	logger(0,"SIGUSR1: Counters:\n");
	logger(0,"==================\n");

	log_line_stats();
}

/* We timeout, with this ALARM */

int alarm_happened = 0;

void
handle_sigalrm(n)
const int n;
{
	signal(SIGALRM,handle_sigalrm);
	alarm_happened = 1;
}


/*========================== FILES section =========================*/
/*
 | Find the next file matching the given mask.
 | Note: We assume that the file mask has the format of:
 |	/dir1/.../FN*.Extension
 |
 | The FileMask tells us whether this is first call (it has a file name),
 | or whether it is not (*FileName = Null). In later case, we use internally
 | (statically) saved values.
 |
 | Input:  FileMask - The mask to search.
 |         context  - Should be passed by ref. Must be zero before search, and
 |                    shouldn't be modified during the search.
 | Output: find_file() - 0 = No more files;  1 = Matching file found.
 |         FileName - The name of the found file.
 */
int find_file(FileMask, FileName, context)
const char *FileMask;
char	*FileName;
DIR	**context;
{
	DIRENTTYPE	*dirp;
	static char	Directory[LINESIZE],
			File_Name[LINESIZE], Extension[LINESIZE];
	char	*p;
	struct stat	stats;

	/* From the file mask, get the directory name, the file name,
	   and the extenstion.						*/

	if (*FileMask != 0) {	/* First time */
	  strcpy(Directory, FileMask);
	  if ((p = strrchr(Directory, '/')) == NULL) {
	    logger(1, "UNIX, Illegal file mask='%s', not an absolute path\n",
		   FileMask);
	    return 0;
	  } else
	    *p++ ='\0';
	  strcpy(File_Name, p);
	  if ((p = strchr(File_Name, '*')) == NULL) {
	    logger(1, "UNIX, Illegal file mask='%s', no `*' in it\n",
		   FileMask);
	    return 0;
	  }
	  *p++ = '\0';
	  if ((p = strchr(p, '.')) == NULL)
	    *Extension = '\0';
	  else
	    strcpy(Extension, p);

	  /* Open the directory */

	  if ((*context = opendir(Directory)) == NULL) {
	    logger(1, "UNIX, Can't open dir (%s) error: %s\n",
		   Directory, PRINT_ERRNO);
	    return 0;
	  }
	  logger(4,"UNIX: find_file(%s -> `%s' dir)\n",
		 FileMask,Directory);
	}

	/* Look for the next file name */

	for (dirp = readdir(*context);
	     dirp != NULL;
	     dirp = readdir(*context)) {
	  int dnamlen = strlen(dirp->d_name);
	  /* dirp->d_namlen  is not available everywhere.. */
	  if ((dnamlen > 0) &&
	     ((memcmp(dirp->d_name, File_Name, strlen(File_Name)) == 0) ||
	      (*File_Name == '\0'))) {
	    if (dirp->d_name[0] != '.') { /* Name beginning with '.' is
					     not a ready-to-submit one! */
		(dirp->d_name)[dnamlen] = '\0';
		sprintf(FileName, "%s/%s",
			Directory, dirp->d_name);
		stat( FileName,&stats );
		if (S_ISREG(stats.st_mode)) {
		  /* Inform about regular files ONLY. */
		  /* XXX: S_ISREG() could be used instead ? */
		  logger(4,"UNIX: find_file(`%s' dir) -> `%s' file\n",
			 Directory,FileName);
		  return 1;
		}
	      }
	  }
	}
	logger(3,"UNIX: find_file(`%s' dir) end.\n",Directory);
	closedir(*context);
	return 0;
}


/*
 | Open the file which will be transmitted. Save its FD in the IoLine
 | structure. It also calls the routine that parses our envelope in the file.
 | Currently handles only one output stream.
 | Set the flags to hold the F_IN_HEADER flag so we know to start reading our
 | header in normal ASCII mode.
*/
int
open_xmit_file(Index, Stream, FileName)
const int	Index;		/* Index into IoLine structure */
const int	Stream;
const char	*FileName;
{
	struct	LINE	*Line;
	FILE	*fd;

	Line = &(IoLines[Index]);

	/* Open the file */
	if ((fd = fopen(FileName, "r+")) == NULL) { /* Open file. */
	  logger(1, "UNIX, name='%s', error: %s\n", FileName, PRINT_ERRNO);
	  return 0;
	}

	Line->InFds[Stream] = fd;

	strcpy(Line->OutFileParams[Stream].SpoolFileName, FileName);
	if (parse_envelope(fd, &Line->OutFileParams[Stream], 1)<0) {
	  fclose(fd);
	  logger(1,"UNIX: file `%s' does not have HUJINJE headers\n",
		 FileName);
	  return 0;
	}

	return 1;
}


/*
 | Open the file to be received. The FD is stored in the IoLines
 | structure. The file is created in BITNET_QUEUE directory, and is called
 | RECEIVE_TEMP.TMP; It'll be renamed later to a more sensible name.
 |  Caller must make sure that the stream number is within range.
*/
int
open_recv_file(Index, Stream)
int	Index,		/* Index into IoLine structure */
	Stream;	/* In the range 0-7 */
{
	FILE	*fd;
	char	FileName[LINESIZE];
	struct	LINE	*Line;
	struct	FILE_PARAMS *FP;
	int	oldumask;

	Line = &(IoLines[Index]);
	FP = &(Line->InFileParams[Stream]);

/* Create the filename in the queue */
	sprintf(FileName, "%s/%s%d_%d.%s", BITNET_QUEUE, TEMP_R_FILE,
		Index, Stream, TEMP_R_EXT);
	strcpy(FP->SpoolFileName, FileName);

	/* logger(3,"UNIX: open_recv_file(%s:%d)\n",
	       Line->HostName,Stream); */

	Line->SizeSavedJobHeader[Stream] = 0;
	Line->SizeSavedDatasetHeader[Stream] = 0;
	Line->SizeSavedJobTrailer[Stream] = 0;
	FP->RecordsCount = 0;
	FP->FileName[0] = FP->FileExt[0] = FP->JobName[0] = 0;
	FP->type = 0;

	oldumask = umask(077);
	/* Open the file */
	if ((fd = fopen(FileName, "w")) == NULL) {	/* Open file. */
	  umask(oldumask);
	  logger(1, "UNIX, name='%s', error: %s\n", FileName, PRINT_ERRNO);
	  return 0;
	}
	umask(oldumask);

	Line->OutFds[Stream] = fd;
	GETTIME(&FP->RecvStartTime);
	FP->flags = 0;
	return 1;
}


/*
 |  Write the given string into the file.
*/
int
uwrite(Index, Stream, string, size)
const void	*string;	/* Line descption */
const int	Index, Stream, size;
{
	FILE	*fd;

	/* We write ONLY BINARY LINES.   recv_file.c does the ASCII header! */

	short unsigned int Size = htons(size);

	fd = IoLines[Index].OutFds[Stream];

	if (fwrite(&Size, 1, sizeof(Size), fd) != sizeof(Size)) {
	  logger(1, "UNIX: Can't fwrite, error: %s\n", PRINT_ERRNO);
	  return 0;
	}
	if (fwrite(string, 1, size, fd) != size) {
	  logger(1, "UNIX: Can't fwrite, error: %s\n", PRINT_ERRNO);
	  return 0;
	}
	IoLines[Index].OutFilePos[Stream] = ftell(fd);

	return 1;
}

/*
 | Read from the given file.
 | The function returns the number of
 | characters read, or -1 if error.
*/
int
uread(Index, Stream, string, size)
/* Read one record from file */
unsigned char	*string;	/* Line descption */
const int	Stream, Index, size;
{
	FILE	*fd;
	unsigned short NewSize;
	static int read_pos;

	fd = IoLines[Index].InFds[Stream];

	read_pos = ftell(fd);
	IoLines[Index].InFilePos[Stream] = read_pos;

	if (fread(&NewSize, sizeof(NewSize), 1, fd) != 1)
	  return -1;	/* Probably end of file */
	NewSize = ntohs(NewSize);
	if (NewSize > size) {	/* Can't reduce size, so can't recover */
	  logger(1, "Unix, Uread, have to read %d into a buffer of only %d.  File offset: %d, Filename: %s\n",
		 NewSize, size, ftell(fd)-2,
		 IoLines[Index].OutFileParams[Stream].SpoolFileName
		 );
	  /* bug_check("Uread - buffer too small"); */
	  /* XXX: [mea]  Should cause transmission abort,
			 and move file into error area.. */
	  return -2;
	}
	if (fread(string, NewSize, 1, fd) == 1) {
	  return NewSize;
	} else {
	  logger(2, "UNIX: Uread errno = %d\n", PRINT_ERRNO);
	  return -1;
	}
}


/*
 | Close and Delete a file given its index into the Lines database.
 */
void
delete_file(Index, direction, Stream)
int	Index, direction, Stream;
{
	FILE	*fd;
	char	*FileName;

	if (direction == F_INPUT_FILE) {
	  fd = IoLines[Index].InFds[Stream];
	  IoLines[Index].InFds[Stream] = NULL;
	  FileName = IoLines[Index].OutFileParams[Stream].SpoolFileName;
	  if (fd)
	    IoLines[Index].InFilePos[Stream] = ftell(fd);
	} else {
	  fd = IoLines[Index].OutFds[Stream];
	  IoLines[Index].OutFds[Stream] = NULL;
	  FileName = IoLines[Index].InFileParams[Stream].SpoolFileName;
	  if (fd)
	    IoLines[Index].OutFilePos[Stream] = ftell(fd);
	}

	if (!fd) return; /* Already closed */

	if (fclose(fd) == -1)
	  logger(1, "UNIX: Can't close file, error: %s\n", PRINT_ERRNO);

	if (unlink(FileName) == -1) {
	  logger(1, "UNIX: Can't delete file '%s', error: %s\n",
		 FileName, PRINT_ERRNO);
	}
}

/*
 | Close a file given its index into the Lines database.
 | The file size is returned.
 */
int	/* [mea] add 'int' here.. */
close_file(Index, direction, Stream)
int	Index, direction, Stream;
{
	FILE	*fd;
	long	FileSize;

	if (direction == F_INPUT_FILE)
	  fd = ((IoLines[Index].InFds)[Stream]);
	else
	  fd = ((IoLines[Index].OutFds)[Stream]);

	if (fd) {
	  fseek(fd,0,2);			/* To the EOF */
	  FileSize = ftell(fd);
	}

	else FileSize = 0;

	if (fd && fclose(fd) == -1)
	  logger(1, "UNIX: Can't close file, error: %s\n", PRINT_ERRNO);

	if (direction == F_INPUT_FILE) {
	  IoLines[Index].InFds[Stream] = NULL;
	  IoLines[Index].InFilePos[Stream] = FileSize;
	} else {
	  IoLines[Index].OutFds[Stream] = NULL;
	  IoLines[Index].OutFilePos[Stream] = FileSize;
	}

	return FileSize;
}


/*
 | Rename the received file name to a name created from the filename parameters
 | passed. The received file is received into a file named NJE_R_TMP.TMP and
 | renamed to its final name after closing it.
 | The flag defines whether the file will renamed according to the queue name
 | (RN_NORMAL) or will be put on hold = extenstion = .HOLD (RN_HOLD), or on
 | abort = .HOLD$ABORT (RN_HOLD$ABORT).
 | The name of the new file is returned as the function's value.
 | If Direction is OUTPUT_FILE, then we have to abort the sending file.
 | Note: Structures for reived files starts with IN (Incoming file) except
 |       from the FABS and RABS which start with OUT (VMS output file).
 */
char *
rename_file(FileParams, flag, direction)
struct FILE_PARAMS *FileParams;
const int flag, direction;
{
	char	InputFile[LINESIZE], ToNode[30], UnderScores[9], *p;
	static char	line[LINESIZE];/* Will return here the new file name */

	if (direction == F_OUTPUT_FILE) { /* The file just received */
	  /* FileParams = &IoLines[Index].InFileParams[Stream]; */
	} else {	/* The sending file - change the flag to ABORT */
	  /* FileParams = &IoLines[Index].OutFileParams[Stream];*/
	}
	strcpy(InputFile, FileParams->SpoolFileName);
	sprintf( line, "%s/",BITNET_QUEUE );

	if (flag == RN_NORMAL)
	  strcat(line, FileParams->line);
	else if (flag == RN_JUNK)
	  strcat(line, "BAD-JUNK");
	else
	  strcat(line, "HOLD-ABORT");
	strcat(line,"/");

	if ((p = strchr(FileParams->To, '@')) != NULL)
		strcpy(ToNode, ++p);
	else	*ToNode = '\0';

	p = line + strlen(line);

	/* XXX: Propably this  stat()  is superficial.. */
	/* stat(InputFile,&FileParams->FileStats); /* The file MUST exist.. */
	/* Name to be unique within the disk, and at most 14 chars.. */
	memcpy(UnderScores,"________",8);
	UnderScores[8-strlen(ToNode)] = 0; /* Chop it off.. */
	sprintf(p, "%s%s%06ld", ToNode, UnderScores,
		(long)FileParams->FileStats.st_ino);

	/* What line will it go to ??? -- rename only if the names DIFFER */
	if (strcmp(InputFile, line) != 0 &&
	    rename(InputFile, line) == -1) {
	  make_dirs( line );	/* [mea] Make sure we have this dir... */
	  if (rename(InputFile, line) == -1) {
	    logger(1, "UNIX: Can't rename '%s' to '%s'. Error: %s\n",
		   InputFile, line, PRINT_ERRNO);
	  } else strcpy( FileParams->SpoolFileName,line );
	} else strcpy( FileParams->SpoolFileName,line );

	return line;	/* Return the new file name */
}


int
get_file_size(FileName)
const char	*FileName;
{
	struct	stat	Stat;

	if (stat(FileName, &Stat) == -1) {
	  logger(1, "UNIX, Can't stat file '%s'. error: %s\n",
		 FileName, PRINT_ERRNO);
	  return 0;
	}
	return Stat.st_size;
}


/*
 | Return the date in IBM's format. IBM's date starts at 1/1/1900, and UNIX
 | starts at 1/1/1970; IBM use 64 bits, where counting the microseconds at
 | the 50 or 51 bit (thus counting to 1.05... seconds of the lower bit of the
 | higher word). In order to compute the IBM's time, we compute the number of
 | days passed from Unix start time, add the number of days between 1970 and
 | 1900, thus getting the number of days since 1/1/1900. Then we multiply by
 | the number of seconds in a day, add the number of seconds since midnight
 | and write in IBM's quadword format - We count in 1.05.. seconds interval.
 | The lower longword is zeroed.
 */

#define	IBM_TIME_ORIGIN	2106649018L	/* Number of seconds between
					   1900 and 1970 */
#define	BIT_32_SEC	1.048565841	/* The number of seconds
					   the 32th bit counts */


void
ibm_time(QuadWord)
u_int32	*QuadWord;
{
	QuadWord[0] = htonl((u_int32) (((float)time(0) / BIT_32_SEC) +
				       IBM_TIME_ORIGIN));
	QuadWord[1] = 0;
}

/* Return UNIX time from given IBM time.. [mea] */
time_t
ibmtime2unixtime(QuadWord)
const u_int32 *QuadWord;
{
	u_int32 t = (u_int32)((float)(ntohl(QuadWord[0])
				      - (u_int32)IBM_TIME_ORIGIN)
			      * BIT_32_SEC );
	return (time_t)t;
}

/*
 *  int makedir( char *path )
 *
 * Makes recursively given set of dirs (with current UID/GID)
 * thru whole path.  Given path must be absolute!  '/foo/bat/'
 * (By [mea] - Lifted from GNU gtar)
 */

int
make_dirs(pathname)
	char *pathname;
{
	char *p;			/* Points into path */
	int madeone = 0;		/* Did we do anything yet? */
	int save_errno = errno;		/* Remember caller's errno */
	int check;

	for (p = strchr(pathname, '/'); p != NULL; p = strchr(p+1, '/')) {
	  /* Avoid mkdir of empty string, if leading or double '/' */
	  if (p == pathname || p[-1] == '/')
	    continue;
	  /* Avoid mkdir where last part of path is '.' */
	  if (p[-1] == '.' && (p == pathname+1 || p[-2] == '/'))
	    continue;
	  *p = 0;				/* Truncate the path there */
	  check = mkdir (pathname, 0770);	/* Try to create it as a dir */
	  if (check == 0) {
	    madeone++;		/* Remember if we made one */
	    *p = '/';
	    continue;
	  }
	  *p = '/';
	  if (errno == EEXIST)		/* Directory already exists */
	    continue;
	  /*
	   * Some other error in the mkdir.  We return to the caller.
	   */
	  break;
	}

	errno = save_errno;		/* Restore caller's errno */
	return madeone;			/* Tell them to retry if we made one */
}



void
read_ebcasc_table(filepath)
char *filepath;
{
	int fd;
	static unsigned char ebuf[8+512+10];
	int rc;

	if (!*filepath) return;

	fd = open(filepath,O_RDONLY,0);
	if (fd < 0) {
	  logger(1,
		 "UNIX: read_ebcasc_table('%s') -- bad file path, errno=%d\n",
		 filepath,errno);
	  return;
	}
	rc = read(fd,ebuf,sizeof(ebuf));
	close(fd);
	if (rc != 522 ||
	    memcmp("ASC<=>EBC\n",ebuf,10) != 0) {
	  logger(1,"UNIX: configuration item EBCDICTBL defined, but file pointed by it is not of proper format.\n");
	  return;
	}
	memcpy(ASCII_EBCDIC,ebuf+10,    256);
	memcpy(EBCDIC_ASCII,ebuf+10+256,256);

	/* Right, we are called in only once..  Reuse that buffer */

#ifdef  HAS_PUTENV
        sprintf(ebuf,"EBCDICTBL=%s",filepath);
        putenv(ebuf);
#else
        setenv ("EBCDICTBL", filepath, 1);
#endif
}
