/* UNIX_TCP.C	V2.4-mea
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
 | It works accoriding to VMnet specs, except that we not send FAST-OPEN
 |
 | When a link cannot be established, it'll be retried after 5 minutes.
 | The line is placed in RETRYING state during that period.
 | When a line fails, we try to re-initiate the connection.
 | After doing LISTEN on a line, we put it in LISTEN state.
 | When a connection is attempted on this line, Select() will notify
 | that there is data ready to read on that line. In this case, we
 | issue the accept.
 | In order to not touch the upper layers, we still queue a timeout when a read
 | is queued. However, when the timer expires we simply re-queue it.
 |
 | The function close_unix_tcp_channel() is called by Restart-channel is the
 | line's state is not one of the active ones.
 */
#include "consts.h"
#include "headers.h"
#include "prototypes.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

extern struct ENQUIRE Enquire;

#define	VMNET_IP_PORT		175
#define	EXPLICIT_ACK		0
#define	IMPLICIT_ACK		1

static struct	sockaddr_in	PassiveSocket;
int		PassiveSocketChannel = -1,	/* On which we listen */
		PassiveReadChannel = -1;   /* On which we wait for an initial
					      VMnet control record */

static u_int32	get_host_ip_address();

extern int	errno;
#ifdef __DARWIN_UNIX03
extern const int	sys_nerr;	/* Maximum error number recognised */
#else
extern int	sys_nerr;	/* Maximum error number recognised */
#endif
/* extern char	*sys_errlist[];	*/ /* List of error messages */
#define	PRINT_ERRNO	(errno > sys_nerr ? "***" : sys_errlist[errno])


/*  Systems have differing ways to define NON-blocking IO..
    Basically again the  SysV vs. BSD.. */
int
setsockblocking(sock,on)
int sock, on;
{
#if defined(O_NONBLOCK)	/* This means POSIX.1 ?? */
	int flags = fcntl(sock, F_GETFL, 0);
	if (flags == -1) return -1; /* Failure */
	if (!on)
	  flags |= O_NONBLOCK;	/* Set it ON  -- non-blocking     */
	else
	  flags &= ~O_NONBLOCK;	/* Set it OFF -- blocking */
	return fcntl(sock, F_SETFL, flags);
#else
#ifdef	FIONBIO	/* We think existance of this indicates BSD style.. */
	return ioctl(sock,FIONBIO,&on);

#else		/* Here it is SYSV style.. */
	int flags = fcntl(sock, F_GETFL, 0);
	if (flags == -1) return -1; /* Failure */
	if (!on)
	  flags |= FNDELAY;	/* Set it ON  -- non-blocking     */
	else
	  flags &= ~FNDELAY;	/* Set it OFF -- blocking */
	return fcntl(sock, F_SETFL, flags);
#endif
#endif
}

/*
 | Create a local socket.
 | If we are the initiating side, queue a non-blocking receive. If it fails,
 | close the channel, and re-queue a retry for 5 minutes later.
 */
void
init_active_tcp_connection(Index,finalize)
const int	Index, finalize;
{
	struct	sockaddr_in	*SocketName;
	int	Socket;		/* The I/O channel */
	struct	VMctl	ControlBlock;
	unsigned char	HostName[128];
	register int	i;
	struct	LINE	*Line = &IoLines[Index];

	SocketName = &(Line->SocketName);

	Line->state = TCP_SYNC; /* Expecting reply for
				   the control block */
	/* Get the IP address */
	if (*Line->device != '\0')
	  strcpy(HostName, Line->device);
	else
	  strcpy(HostName, Line->HostName);

#if	defined(NBCONNECT)||defined(NBSTREAM)
	if (!finalize) {
#endif
	  /* A fresh start.. */

	  if (Line->socket >= 0)
	    close(Line->socket);

	  Line->socket = -1;
	  Line->socketpending = -1;

	  /* Create a local socket */
	  if ((Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
	    logger(1, "UNIX_TCP, Can't get local socket. (Line %s) error: %s\n",
		   Line->HostName, PRINT_ERRNO);
	    Line->state = INACTIVE;
	    close(Line->socket);
	    return;
	  }

	  Line->socket = Socket;
#if	defined(NBCONNECT)||defined(NBSTREAM)
	  Line->socketpending = Socket; /* Mark that we are doing it.. */
	  setsockblocking(Socket,0);
#else
	  Line->socketpending = -1;
#endif

#if 1  /* Hmm...  this does force the local end.. */
	  {
	    int on = 1;
	    setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR,
		       (char*)&on, sizeof(on));
	  }

	  SocketName->sin_family = AF_INET;
	  SocketName->sin_port   = 0; /* Any port */
	  SocketName->sin_addr.s_addr
	    = get_host_ip_address(Index,IP_ADDRESS);
	  bind(Socket,(struct sockaddr *)SocketName,sizeof(struct sockaddr));
#endif

	  SocketName->sin_family = AF_INET;
	  SocketName->sin_port = htons(Line->IpPort);

	  if ((int32)(SocketName->sin_addr.s_addr
		    = get_host_ip_address(Index, HostName)) == -1) { /*Error!*/
	    logger(1,"Resolving hostname %s failed! (Link %s)\n",HostName,
		   Line->HostName);
	    Line->state = RETRYING;
	    close(Socket);
	    Line->socket = 0;
	    return;
	  }
	  logger(4,"UNIX_TCP: Resolved `%s' to address `%s'\n",HostName,
		 inet_ntoa(SocketName->sin_addr));
	
	  /* ?? Do the connection trial in a subprocess to not block
	     the parent one */

	  if (connect(Socket, (struct sockaddr *)SocketName,
		      sizeof(struct sockaddr)) == -1) {
#if	defined(NBCONNECT)||defined(NBSTREAM)
	    /* We MAY get  EINPROGRESS, as we have this socket defined as
	       NON-Block -- to get past a lengthy timeout..
	       We do a select-for-write for it to see when it becomes
	       fully operational, and then we do the pending treatment,
	       the thing below there..   The S-f-W does flag up also
	       when the connection atLinet times out, or gets refused,
	       so below it may hit with SIGPIPE/EPIPE.. */
	    if (errno == EINPROGRESS) return; 
#endif

	    logger(2, "UNIX_TCP, Can't connect. (Line %s) error: %s\n",
		   Line->HostName, PRINT_ERRNO);
	    Line->state = RETRYING;
	    close(Socket);
	    Line->socket = -1;
	    Line->socketpending = -1;
	    return;
	  }
#if	defined(NBCONNECT)||defined(NBSTREAM)
	}
#endif

	/* Now we are finishing it! */

	Line->socketpending = -1;
#if	defined(NBCONNECT)||defined(NBSTREAM)
	/* setsockblocking(Line->socket,1); */
#endif

	/* Send the initial connection block */
	Line->RecvSize = 0;
	Line->XmitSize = 0;
	Line->WritePending = NULL;
	Line->InBCB  = 0;
	/* Line->OutBCB = 0;
	   Line->flags &= ~ F_RESET_BCB; */

	/* Send the first control block */
	ASCII_TO_EBCDIC("OPEN    ", ControlBlock.type, 8);
	memcpy(ControlBlock.Rhost, E_BITnet_name, E_BITnet_name_length);
	i = strlen(Line->HostName);
	ASCII_TO_EBCDIC(Line->HostName, ControlBlock.Ohost, i);
	PAD_BLANKS(ControlBlock.Ohost, i, 8);
	ControlBlock.Rip = get_host_ip_address(Index, IP_ADDRESS);
	ControlBlock.Oip = get_host_ip_address(Index, HostName);
	ControlBlock.R = 0;

#ifdef DEBUG
	logger(5, "Writing OPEN control block to line #%d:\n", Index);
	trace(&ControlBlock, VMctl_SIZE, 5);
#endif

	if (writen(Line->socket, &ControlBlock, VMctl_SIZE) == -1) {
	  /* If the delayed open failed, be quiet */
	  if (finalize && errno == EPIPE) {
	    logger(2, "UNIX_TCP, Can't `delayed-connect'. (Line %s)\n",
		   Line->HostName, PRINT_ERRNO);
	    close_unix_tcp_channel(Index);
	    return;
	  }
	  logger(1,"UNIX_TCP, line=%s, Can't write control block, error: %s\n",
		 Line->HostName, PRINT_ERRNO);
	  close_unix_tcp_channel(Index);
	  return;
	}

#if	defined(NBSTREAM)
	setsockblocking(Line->socket,0);
#endif


#ifdef	USE_SOCKOPT
	{
#ifndef SOCKBUFSIZE
# define SOCKBUFSIZE 32*1024
#endif

	  int ssize = SOCKBUFSIZE, rsize = SOCKBUFSIZE;
	  int setval, setvalsz;

	  Socket = Line->socket;

	  while (ssize > 1024) {
	    int rc = setsockopt(Socket,SOL_SOCKET,SO_SNDBUF,
				(void*)&ssize,sizeof(ssize));
	    setvalsz = sizeof(setval);
	    if (rc >= 0)
	      rc = getsockopt(Socket,SOL_SOCKET,SO_SNDBUF,
			      (void*)&setval, &setvalsz);
	    if (rc >= 0 && setval != ssize)
	      rc = -1;
	    if (rc < 0)
	      ssize -= 1024;
	    else
	      break;
	  }

	  while (rsize > 1024) {
	    int rc = setsockopt(Socket,SOL_SOCKET,SO_RCVBUF,
				(void*)&rsize,sizeof(rsize));
	    setvalsz = sizeof(setval);
	    if (rc >= 0)
	      rc = getsockopt(Socket,SOL_SOCKET,SO_RCVBUF,
			      (void*)&setval, &setvalsz);
	    if (rc >= 0 && setval != rsize)
	      rc = -1;
	    if (rc < 0)
	      rsize -= 1024;
	    else
	      break;
	  }

	  logger(3,"UNIX_TCP: on init_active_tcp_connection(Line %s) setsockopt() SNDBUF = %d, RCVBUF = %d\n",Line->HostName,ssize,rsize);
	}
#endif

	queue_receive(Index);	/* Queue a receive on the line */
}


/*
 | Create a local socket. Bind a name to it if we must be the secondary side.
 */
void
init_passive_tcp_connection(TcpType)
const int TcpType;
{
	int on = 1;

	PassiveReadChannel = -1;	/* To signal that we haven't got
					   any connection yet */

	/* Create a local socket */
	if ((PassiveSocketChannel = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	  logger(1, "UNIX_TCP, Can't get local socket for passive channel. error: %s\n",
		 PRINT_ERRNO);
	  PassiveSocketChannel = -1; /* To signal others we
					did not succeed */
	  return;
	}

	setsockopt(PassiveSocketChannel, SOL_SOCKET, SO_REUSEADDR,
		   (char*)&on, sizeof(on));

	/* Now, bind a local name for it */
	PassiveSocket.sin_family = AF_INET;
	PassiveSocket.sin_port   = htons(VMNET_IP_PORT);
	PassiveSocket.sin_addr.s_addr = 0; /* Just this host */
	/* = get_host_ip_address(0,IP_ADDRESS);*//* ESPECIALLY on Multihomed
						    systems!  This makes it
						    to accept connect on only
						    the configured address. */

	if (bind(PassiveSocketChannel, (struct sockaddr *)&PassiveSocket,
		 sizeof(struct sockaddr_in)) == -1) {
	  logger(1, "UNIX_TCP, Can't bind (passive). error: %s\n",
		 PRINT_ERRNO);
	  close(PassiveSocketChannel);
	  PassiveSocketChannel = -1;	/* To signal others we
					   did not succeed */
	  return;
	}
	if (listen(PassiveSocketChannel, 2) == -1) {
	  logger(1, "UNIX_TCP, Can't listen (passive), error: %s\n",
		 PRINT_ERRNO);
	  close(PassiveSocketChannel);
	  PassiveSocketChannel = -1;	/* To signal others we
					   did not succeed */
	  return;
	}
}

/*
 | Some connection to our passive end. Try doing Accept on it; if successfull,
 | wait for input on it. This input will call Read-passive-TCP-connection.
 */
void
accept_tcp_connection()
{
	int	size;

	size = sizeof(struct sockaddr_in);
	if ((PassiveReadChannel = accept(PassiveSocketChannel,
					 (struct sockaddr*)&PassiveSocket,
					 &size)) == -1) {
	  logger(1, "UNIX_TCP, Can't accept on passive end. error: %s\n",
		 PRINT_ERRNO);
	  PassiveReadChannel = -1;
	}

#ifdef	USE_SOCKOPT
	{
	  int ssize = SOCKBUFSIZE, rsize = SOCKBUFSIZE;
	  int Socket = PassiveReadChannel;
	  int setval, setvalsz;

	  while (ssize > 1024) {
	    int rc = setsockopt(Socket,SOL_SOCKET,SO_SNDBUF,
				(void*)&ssize,sizeof(ssize));
	    setvalsz = sizeof(setval);
	    if (rc >= 0)
	      rc = getsockopt(Socket,SOL_SOCKET,SO_SNDBUF,
			      (void*)&setval, &setvalsz);
	    if (rc >= 0 && setval != ssize)
	      rc = -1;
	    if (rc < 0)
	      ssize -= 1024;
	    else
	      break;
	  }

	  while (rsize > 1024) {
	    int rc = setsockopt(Socket,SOL_SOCKET,SO_RCVBUF,
				(void*)&rsize,sizeof(rsize));
	    setvalsz = sizeof(setval);
	    if (rc >= 0)
	      rc = getsockopt(Socket,SOL_SOCKET,SO_RCVBUF,
			      (void*)&setval, &setvalsz);
	    if (rc >= 0 && setval != rsize)
	      rc = -1;
	    if (rc < 0)
	      rsize -= 1024;
	    else
	      break;
	  }
	  logger(3,"UNIX_TCP: on accept_tcp_connection() setsockopt() SNDBUF = %d, RCVBUF = %d\n",ssize,rsize);
	}
#endif
}

/*
 | Some input has been received for the passive end. Try receiving the OPEN
 | block in one read (and hopefully we'll receive it all).
 */
void
read_passive_tcp_connection()
{
	register int	i, size, Index;
	short	ReasonCode;	/* For signaling the other side,
				   why we NAK him */
	char	HostName[10], Exchange[10];
	struct	VMctl	ControlBlock;
	char	*remoteaddr;

	ReasonCode = 0;

	remoteaddr = inet_ntoa(PassiveSocket.sin_addr);

	/* Read input */
	if ((size = read(PassiveReadChannel, (void *)&ControlBlock,
			 sizeof(ControlBlock))) == -1) {
	  /* errno==EINTR is possible, but should not happen.. */
	  logger(1, "UNIX_TCP, Error reading VMnet block from [%s], error: %s\n",
		 remoteaddr, PRINT_ERRNO);
	  close(PassiveReadChannel);
	  PassiveReadChannel = -1; /* To signal it is closed */
	  return;
	}

#ifdef DEBUG
	logger(5, "Received %d chars for passive end (when expecting OPEN record):\n",
		size);
	trace(&ControlBlock, size, 5);
#endif

	/* Check that we've received enough information */
	if (size < VMctl_SIZE) {
	  logger(1, "UNIX_TCP, Received too small control record from [%s]\n",
		 remoteaddr);
	  logger(1, "      Expecting %d, received size of %d\n",
		 VMctl_SIZE, size);
	  goto RetryConnection;
	}

	/* Check first that this is an OPEN block.
	   If not - reset connection */
	EBCDIC_TO_ASCII(ControlBlock.type, Exchange, 8); Exchange[8] = '\0';
	if (strncmp(Exchange, "OPEN", 4) != 0) {
	  logger(1, "UNIX_TCP, Illegal control block '%s' received. Reason code=%d\n",
		 Exchange, ControlBlock.R);
	  goto RetryConnection;
	}

	/* OK, assume we've received all the information;
	   get the hosts names from the control block and check the names. */
	EBCDIC_TO_ASCII(ControlBlock.Rhost, HostName, 8); /* His name */
	despace(HostName,8);
	EBCDIC_TO_ASCII(ControlBlock.Ohost, Exchange, 8); /* Our name
							     (as he thinks) */
	despace(Exchange,8);

	/* Verify that he wants to call us */
	if (strncmp(Exchange, LOCAL_NAME, strlen(LOCAL_NAME)) != 0) {
	  logger(1, "UNIX_TCP, Host %s [%s] incorrectly connected to us (%s)\n",
		 HostName, remoteaddr, Exchange);
	  goto RetryConnection;
	}

	/* Look for its line */
	for (Index = 0; Index < MAX_LINES; Index++) {
	  if (strcmp(IoLines[Index].HostName, HostName) == 0) {
	    /* Found - now do some checks */
	    struct LINE *Line = &IoLines[Index];
	    logger(2,"UNIX_TCP: Line %s/%d passive channel read\n",
		   Line->HostName,Index);
	    if (Line->type != UNIX_TCP) { /* Illegal */
	      logger(1, "UNIX_TCP, host %s, line %d, is not a UNIX_TCP type but type %d\n",
		     Line->HostName,Index,Line->type);
	      goto RetryConnection;
	    }
	    if (Line->state != LISTEN &&
		Line->state != RETRYING) {
	      /* Break its previous connection */
	      if (Line->state == ACTIVE) {
		ReasonCode = 2; /* Link is active.. */
		goto RetryConnection;
	      } else if (Line->state == TCP_SYNC) {
		close(Line->socket);
		Line->socket = -1;
		Line->socketpending = -1;
		logger(1,"UNIX_TCP: read_passive_tcp_connection(%s/%d) -- the other end called us, we quit the active open attempt!, link state was %d\n",
		       Line->HostName,Index,Line->state);
		Line->state = LISTEN;
	      } else {
		/* ??? */
	      }
	    }
	    logger(2,"UNIX_TCP: Acking passive open on line %s/%d\n",
		   Line->HostName,Index);
	    if (Line->socket >= 0) {
	      logger(1,"UNIX_TCP: Passive open came in on line with existing stream! Closing the old one! (%s/%d)\n",
		     Line->HostName,Index);
	      close(Line->socket);
	    }
	    

	    /* Copy the parameters from the Accept block,
	       so we can post a new one */
	    ASCII_TO_EBCDIC("ACK     ", ControlBlock.type, 8);
	    Line->socket = -1;
	    Line->socketpending = -1;
	    init_link_state(Index);	/* Previous settings so that
					   init_link_state() won't close
					   the link right away, but will
					   init all bits/flags		*/
	    Line->socket = PassiveReadChannel;
	    PassiveReadChannel = -1;	/* We've moved it...		*/
	    Line->state    = DRAIN;	/* Starting in DRAINed state	*/

	    /* Send and ACK block - transpose the fields */
	    memcpy(Exchange, ControlBlock.Rhost, 8);
	    memcpy(ControlBlock.Rhost, ControlBlock.Ohost, 8);
	    memcpy(ControlBlock.Ohost, Exchange, 8);
	    i = ControlBlock.Oip;
	    ControlBlock.Oip = ControlBlock.Rip;
	    ControlBlock.Rip = i;
	    queue_receive(Index); /* Queue a receive for it */

#ifdef DEBUG
	    logger(5, "Writing ACK control block to line #%d:\n", Index);
	    trace(&ControlBlock, VMctl_SIZE, 5);
#endif

	    if (writen(Line->socket, &ControlBlock, VMctl_SIZE) < 0) {
	      logger(1,"UNIX_TCP: writen() on accept_tcp_connection() failed!");
	    }

#ifdef	NBSTREAM
	    setsockblocking(Line->socket,0);
#endif

	    return;
	  }
	}

	/* Line not found - log it, and dismiss the connection */
	logger(1, "UNIX_TCP, Can't find line for host '%s' [%s]\n",
	       HostName, remoteaddr);

	/* Send a reject to other side and re-queue the read */
RetryConnection:
	if (ReasonCode == 0) ReasonCode = 1;	/* Link not found */
	ASCII_TO_EBCDIC("NAK     ", ControlBlock.type, 8);
	ControlBlock.R = ReasonCode;
	memcpy(ControlBlock.Rhost, E_BITnet_name, E_BITnet_name_length);
	logger(2, "UNIX_TCP: writing passive-channel NAK to %s [%s] with reason code: %d\n",
	       HostName, remoteaddr, ReasonCode);

	writen(PassiveReadChannel, &ControlBlock, VMctl_SIZE);
	close(PassiveReadChannel);
	PassiveReadChannel = -1;
}



/*
 | Called when something was received from TCP. Receive the data and call the
 | appropriate routine to handle it.
 | When receiving,  the first 4 bytes are the count field. When we get the
 | count, we continue to receive until the count exceeded. Then we call the
 | routine that handles the input data.
 */
void
unix_tcp_receive(Index, line)
int	Index;
struct	LINE *line;
{
	struct LINE *Line = line;
	int	i, size = 0, rsize = 0;
	int	lerrno, buff_full = 0;
	char	Type[10];	/* Control block type */
	unsigned char	*p, *q;

	/* Line = &(IoLines[Index]); */
	dequeue_timer(Line->TimerIndex);	/* Dequeue the timeout */

	/* Append the data to our buffer */

	/* Buffer max size */
	rsize = sizeof(Line->InBuffer);

	/* how much fits into this buffer ? */
	if ((size = (rsize - Line->RecvSize)) <= 0) {
	  /* It is full */
	  buff_full = 1;
	  rsize = 0;
	  size  = 0;

	} else {
	  /* Not yet full, read something in */
	  errno = 0;
	  if ((rsize = read(Line->socket,
			    Line->InBuffer + Line->RecvSize, size)) == -1) {
	    if (errno == EINTR) {
	      queue_receive(Index); /* Requeue the read request */
	      return;		/* Well, come back later.. */
	    }
#ifdef NBSTREAM
	    if (errno == EWOULDBLOCK || errno == EAGAIN) {
	      queue_receive(Index); /* Requeue the read request */
	      return;
	    }
#endif

	    logger(1, "UNIX_TCP: Error reading, line %s, error: %s\n",
		   Line->HostName, PRINT_ERRNO);
	    Line->state = INACTIVE;
	    /* Will close line and put it into correct state */
	    restart_channel(Index);
	    return;
	  }

	  GETTIME(&Line->InAge);

	  Line->RdBytes += rsize;
	  size = rsize;
	  lerrno = errno;

#ifdef DEBUG
	  logger(2, "UNIX_TCP: Line %s, Received %d bytes\n",
		 Line->HostName, rsize);
	  trace(Line->InBuffer + Line->RecvSize, size, 5);
#endif
	  /* If we read 0 characters, it usually signals that other
	     side closed the connection */
	  if (size == 0 /*&& Line->RecvSize == 0*/) {
	    logger(1,"UNIX_TCP: Zero characters read - TCP's way to report EOF. Disabling line %s, errno = %d\n",
		   Line->HostName,lerrno);
	    Line->state = INACTIVE;
	    restart_channel(Index); /* Will close line and put
				       it into correct state */
	    return;
	  }

	  /* Register the current size */
	  Line->RecvSize += size;

	} /* .. not buffer full, we could read */


	/* If we are in the TCP_SYNC stage, then this is the reply
	   from other side */

	if (Line->state == TCP_SYNC) {
	  struct VMctl	*ControlBlock;

	  if (Line->RecvSize < 10) {	/* Too short block */
	    logger(1, "UNIX_TCP, Too small Open record received on line %s\n",
		   Line->HostName);
	    Line->state = INACTIVE;
	    restart_channel(Index); /* Will close line and put
				       it into correct state */
	    return;
	  }
	  ControlBlock = (struct VMctl*)Line->InBuffer;
	  EBCDIC_TO_ASCII(ControlBlock->type, Type, 8); Type[8] = '\0';
	  if (strcmp(Type, "ACK     ") != 0) { /* Something wrong */
	    if (strcmp(Type,"NAK     ")==0)
	      logger(1, "UNIX_TCP: Got a NAK on OPEN attempt of the line %s\n",
		     Line->HostName);
	    else
	      logger(1, "UNIX_TCP: Illegal control record '%s', line %s\n",
		     Type, Line->HostName);
	    /* Print error code */
	    switch (ControlBlock->R) {
	      case 0x01:
		  logger(1,"     Link could not be found\n");
		  break;
	      case 0x02:
		  logger(1,"     Link is in active state at other host\n");
		  break;
	      case 0x03:
		  logger(1,"     Other side is attempting active open\n");
		  break;
	      default:
		  logger(1,"     Illegal error code %d\n", ControlBlock->R);
		  break;
	    }
	    Line->state = INACTIVE;
	    restart_channel(Index); /* Will close line and put
				       it into correct state */
	    return;
	  }
	  Line->RetryIndex = 0;	/* That was successfull, use short period! */

	  /* It's ok - set channel into DRAIN and send the first Enquire */
	  Line->state = DRAIN;

	  /* Send an enquiry there */
	  send_data(Index, &Enquire, sizeof(struct ENQUIRE), SEND_AS_IS);

	  queue_receive(Index);
	  Line->TcpState = 0;
	  Line->RecvSize = 0;
	  return;
	}

	/* Loop over the received InBuffer, and append characters as needed */

	/* Protocol sends:
	     TTB(LN=TTB_SIZE+VMnet_length+TTR_SIZE) <VMnet_blocks> TTR(LN=0)
	   where  <VMnet_blocks> is one or more of following pairs:
	     TTR(LN=TTR_SIZE+VMnet_data_size) <VMnet_data>
	   repeating as long as it can fit into the buffer.
	 */

	if (Line->TcpState == 0) {	/* New InBuffer */
	  if (Line->RecvSize >= 4) {	/* We can get the size */
	    int NewSize =		/* Accumulate it... */
	      (((unsigned char)(Line->InBuffer[2])) << 8) +
		(unsigned char)(Line->InBuffer[3]);
	    if (NewSize <= Line->RecvSize)
	      Line->TcpState = NewSize; /* Got enough! */
	    else {
	      queue_receive(Index); /* Requeue the read request */
	      if (buff_full) {
		logger(1,
		       "UNIX_TCP: VMNET TTB read size failure;  TTBsize=%d\n",
		       NewSize);
		restart_channel(Index);
	      }
	      return;
	    }
	  } else {		/* We need at least 4 bytes */
	    queue_receive(Index); /* Requeue the read request */
	    if (buff_full)
	      bug_check("UNIX_TCP: VMNET TTB READ SIZE FAILURE (impossible variant)!\n");
	    return;
	  }
	}

	/* logger(2,"UNIX_TCP: unix_tcp_receive(%s) RecvSize=%d, first TTBsize=%d\n",
	   Line->HostName,Line->RecvSize,Line->TcpState); */

	while (Line->RecvSize >= 4) { /* Loop over the InBuffer */

	  Line->TcpState =  ((((unsigned char)(Line->InBuffer[2])) << 8) +
			     (unsigned char)(Line->InBuffer[3]));

	  /* logger(2,"UNIX_TCP: unix_tcp_receive(%s) (loop) RecvSize=%d, first TTBsize=%d\n",
	     Line->HostName,Line->RecvSize,Line->TcpState); */

	  if (Line->TcpState > Line->RecvSize) break; /* Out of data.. */

	  p = Line->InBuffer;	/* p  points to the First TTR */
	  i = Line->TcpState;	/* Size of TTB contents */
	  p += TTB_SIZE;
	  i -= TTB_SIZE;
	  for (;i >= TTR_SIZE;) {

	    int	 TTRlen;
	    struct TTR ttr;

	    memcpy(&ttr, p, TTR_SIZE); /* Copy to our struct */
	    if (ttr.LN == 0) break; /* End of InBuffer */

	    p += TTR_SIZE;
	    TTRlen = ntohs(ttr.LN);
	    /* Check if the size in TTR is less than
	       the remained size. If not - bug */
	    if (TTRlen > i) {
	      logger(1, "UNIX_TCP, Line %s, Size in TTR(%d) longer than input left(%d) at offset %d\n",
		     Line->HostName, TTRlen, i, (p - Line->InBuffer));
	      restart_channel(Index);
	      return;
	    }

	    /* Check whether FAST-OPEN flag is on.
	       If so - set our flag also */

	    if ((ttr.F & 0x80) != 0) /* Yes */
	      Line->flags |= F_FAST_OPEN;
	    else		/* No - clear the flag */
	      Line->flags &= ~F_FAST_OPEN;

	    /* No wait-a-bit just because of input arrived ?? */
	    /* Line->flags |= F_WAIT_V_A_BIT; */
	    input_arrived(Index, 1 /* Success status */, p, TTRlen);
	    if (Line->state != ACTIVE) {
	      /* We restarted! */
	      i = 0;
	      break;
	    }
	    p += TTRlen;
	    i -= (TTRlen + TTR_SIZE);
	  }

	  /* Place pointer to the NEXT thing in buffer.. */
	  p = Line->InBuffer + Line->TcpState;
	  i = Line->RecvSize - Line->TcpState; /* that much left.. */
	  if (i < 0) i = 0; /* A link restart may happen.. */
	  Line->RecvSize = i;

	  buff_full = 0;

	  if (i > 0) {		/* rest of InBuffer is next frame */
	    q = Line->InBuffer;
	    memcpy(q, p, i);	/* Re-align the InBuffer */
	    if (i >= 4) {	/* Enough for getting the length */
	      /* Size of next frame */
	      Line->TcpState = 
		(((unsigned char)(Line->InBuffer[2])) << 8) +
		  (unsigned char)(Line->InBuffer[3]);
	      continue;
	    } else
	      Line->TcpState = 0; /* Next read will get enough
				     data to complete count */
	  } else {
	    Line->TcpState = 0; /* New frame */
	  }
	}


	if (Line->TcpState > sizeof(Line->InBuffer)) {
	  logger(1,"UNIX_TCP: unix_tcp_receive(%s): Line->TcpState=%d, which is oversize!\n",
		 Line->HostName,Line->TcpState);
	  restart_channel(Index);
	  return;
	}

	/* logger(2,"UNIX_TCP: unix_tcp_receive(%s) left over RecvSize=%d\n",
	   Line->HostName,Line->RecvSize); */

	/* Reserve wait-a-bit for output queue holding */
	/* Line->flags &= ~F_WAIT_V_A_BIT; */
	if (Line->state == ACTIVE)
	  handle_ack(Index, IMPLICIT_ACK);

	queue_receive(Index);	/* Requeue the read request */
}



#ifdef	NBSTREAM
void
tcp_partial_write(Index,flg)
const int Index;
const int flg;
{
	struct	LINE	*Line = &IoLines[Index];
	int rc;

	do {
	  errno = 0;
	  rc = write(Line->socket,Line->WritePending,Line->XmitSize);
	  if (rc == -1 && errno != EAGAIN &&
	      errno != EWOULDBLOCK && errno != EINTR) {
	    logger(1, "UNIX_TCP, tcp_partial_write(): line %s, flg %d error: %s\n",
		   Line->HostName, flg, PRINT_ERRNO);
	    restart_channel(Index);
	    return;
	  }
	} while (errno == EINTR);

	if (rc < 0) {

	  logger(3,"UNIX_TCP: Blocking pending writer on line %s, flg %d, size left=%d\n",
		 Line->HostName, flg, Line->XmitSize);

	  Line->flags |= F_WAIT_V_A_BIT;
	  return;
	}
	if (rc < Line->XmitSize) {	/* A Partial write ! */
	  Line->WritePending = (void*)((char*)(Line->WritePending) + rc);
	  Line->XmitSize -= rc;
	  Line->flags |= F_WAIT_V_A_BIT;
	  Line->WrBytes += rc;

	  logger(3,"UNIX_TCP: pending writer on line %s, flg %d, size left=%d\n",
		 Line->HostName, flg, Line->XmitSize);

	  GETTIME(&Line->XmitAge);
	  return;
	}

	Line->WrBytes += rc;
	Line->XmitSize = 0;
	Line->WritePending = NULL;
	Line->flags &= ~F_WAIT_V_A_BIT;	/* All out, clear this	*/
	if (Line->state == ACTIVE)
	  Line->flags |= F_CALL_ACK;	/* Main loop will call Handle-Ack */
	GETTIME(&Line->XmitAge);
}
#endif

#ifdef USE_XMIT_QUEUE
int
dequeue_xmit_queue(Index)
const int Index;
{
	struct LINE	*Line = &IoLines[Index];

	/* Test whether we have a queue of pending transmissions.
	   If so, send them */

	if (Line->FirstXmitEntry != Line->LastXmitEntry
#ifdef NBSTREAM
	    && Line->WritePending == NULL
#endif
	    ) {
		int size;
		char *p, *q;

		Line->flags |= F_WAIT_V_A_BIT;
		size = (Line->XmitQueueSize)[Line->FirstXmitEntry];
		Line->XmitSize = size;
		p = Line->XmitBuffer;
		q = (Line->XmitQueue)[Line->FirstXmitEntry];
		memcpy(p, q, size);
		free(q);	/* Free the memory allocated for it */
		Line->FirstXmitEntry = ++(Line->FirstXmitEntry) % MAX_XMIT_QUEUE;

/* logger(2,"UNIX_TCP: Dequeued XMIT buffer of size %d on line %s\n",
   size,Line->HostName); */

		send_unix_tcp(Index, p, size);
	}
	/* Return a flag about the queue status */
	return (Line->FirstXmitEntry != Line->LastXmitEntry);
}
#endif

/*
 | Write a block to TCP.
 */
void
send_unix_tcp(Index, line, size)
const int	Index, size;
const void	*line;
{
	struct	LINE	*Line = &IoLines[Index];

#ifdef  NBSTREAM	/* ++++++++++++ NBSTREAM ++++++++++++++ */
	if (Line->WritePending != NULL) {
	  bug_check("Line  WritePending  pointer non-zero at  send_unix_tcp()!\n");
	}
	Line->WritePending = line;
	tcp_partial_write(Index,1);
#else			/* ++++++++++++ NBSTREAM ++++++++++++++ */


	int rc;
	int wsize = size;
	int loops = 5;
	int sleepy;
	struct timeval expiry, now0, now, timeout;
	struct timezone zones;
	const char *buf = (char *) line;
#ifdef AIX
	struct {
		fd_set  fdsmask;
		fd_set  msgsmask;
	} writefds;
#else	/* !AIX */
#ifdef	FD_SET
	fd_set	writefds;
#else /* No AIX, nor FD_SET */
	long	writefds;	/* On most other machines */
#endif /* no AIX, nor FD_SET */
#endif /* no AIX, nor FD_SET */

#ifdef	AIX
 	FD_ZERO(&writeds.fdsmask);
#else	/* !AIX */
#ifdef	FD_SET
	FD_ZERO(&writefds);
#else
	writefds = 0;
#endif
#endif

	gettimeofday(&now0,&zones);
	expiry = now0;
	expiry.tv_sec += 15;	/* 15 sec expiry.. */

#if	!defined(NBSTREAM)
	setsockblocking(Line->socket,0);
#endif

	do {

	  int nfds = 0;

	  gettimeofday(&now,&zones);
	  sleepy = timevalsub(&timeout,&expiry,&now);

	  if (sleepy < 0) continue; /* Timed out.. */


#ifdef	USE_POLL
	  {
#include <stropts.h>
#include <poll.h>

	    struct pollfd pfd;
	    int sleepmses = timeout.tv_sec * 1000 + timeout.tv_usec/1000;
	    pfd.fd     = Line->socket;
	    pfd.events = POLLOUT;
	    pfd.revents = 0;

	    nfds = poll(&pfd, 1, sleepmses);
	    if (nfds == -1) {
	      if (errno == EINTR) continue; /* burb.. */
	      logger(1,"UNIX_TCP: Delayed write poll error: %s\n",
		     PRINT_ERRNO);
	      restart_channel(Index);
	      return;
	    }
	    if (nfds == 0) {
	      /* TIMEOUT! */
	      break;
	    }
	    if (pfd.revents & (POLLNVAL | POLLHUP | POLLERR)) {
	      logger(1,"UNIX_TCP: Delayed write poll event error: %s%s%s\n",
		     (pfd.revents & POLLNVAL) ? "NVAL " : "",
		     (pfd.revents & POLLHUP)  ? "HUP "  : "",
		     (pfd.revents & POLLERR)  ? "ERR"   : "");
	      restart_channel(Index);
	      return;
	    }
	  }
#else	/* ! USE_POLL */

	  {
	    /* Do a select() to see if there is something where we can
	       write..  If not, it may take a moment to leave the thing
	       with a timeout.. */

	    int FdWidth = Line->socket+1;
#ifdef	AIX
	    FD_SET(Line->socket, &writeds.fdsmask);
#else
#ifdef	FD_SET
	    FD_SET(Line->socket, &writefds);
#else
	    writefds |= 1 << Line->socket;
#endif
#endif

	    if ((nfds = select(FdWidth,NULL,&writefds,NULL,&timeout)) == -1) {
	      --loops;
	      if (errno == EINTR) continue;
	      logger(1,"UNIX_TCP: Delayed write select error: %s\n",
		     PRINT_ERRNO);
	      restart_channel(Index);
	      return;
	    }
	    if (nfds == 0) {
	      /* TIMEOUT! */
	      break;
	    }
	  }
#endif	/* !USE_POLL */

#if 0 /* Don't read() -- doesn't seem to work.. */
	  /* While something to write, at first read off what
	     there possibly was to read.. */

	  {
	    int ssize, rsize;
	    rsize = 0;
	    if ((ssize = (sizeof(Line->InBuffer) - Line->RecvSize)) > 0) {
	      rsize = read(Line->socket,
			   &Line->InBuffer[Line->RecvSize], ssize);
	      if (rsize > 0)
		/* Ok, we HAVE something in the buffer! */
		Line->RecvSize += rsize;
	    }
	  }
#endif
	  rc = write(Line->socket,buf,wsize);
	  if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
	    /* How come ?????? */
	    continue;
	  }
	  if (rc < 0 && errno == EINTR && --loops > 0) continue;
	  if (rc < 0) {
	    logger(1,"UNIX_TCP: Writing to the TCP: Line %s, error: %s\n",
		   Line->HostName, PRINT_ERRNO);
	    restart_channel(Index);
	    return;
	  }
	  if (loops <= 0) break;

	  buf   += rc;
	  wsize -= rc;

	  Line->WrBytes += rc;
	    
	  /* While something to write */
	} while (wsize > 0 && sleepy >= 0 && loops >= 0);

	if (wsize > 0) {
	  /* BUG! */
	  logger(1,"UNIX_TCP: Writing to the TCP line %s/%d, sleep (%ds) expired!\n",
		 Line->HostName,Index,time(NULL)-now0.tv_sec);
	  restart_channel(Index);
	  return;
	}

#if	!defined(NBSTREAM)
	/* Turn off the non-block mode */
	setsockblocking(Line->socket,1);
#endif

	GETTIME(&Line->XmitAge);

	/* and purge the write buffer pointer */
	Line->XmitSize = 0;
	Line->WritePending = NULL;

	Line->flags &= ~F_WAIT_V_A_BIT;
	if (Line->state == ACTIVE)
	  Line->flags |= F_CALL_ACK;	/* Main loop will call Handle-Ack */

#endif	/* !NBSTREAM */
}


/*
 | Deaccess the channel and close it. If it is a primary channel,
 | queue a restart for it after 5 minutes to establish the link again.
 | Don't do it immediately since there is probably a fault at the remote
 | machine.   If this line is a secondary, nothing has to be done.
 | We have an accept active always. When the connection will be accepted,
 | the correct line will be located by the accept routine.
 */
void
close_unix_tcp_channel(Index)
int	Index;
{
	close(IoLines[Index].socket);
	IoLines[Index].socket = -1;
	IoLines[Index].socketpending = -1;

	/* Re-start the channel. */
	IoLines[Index].state = RETRYING;
}


/*
 | Get the other host's IP address using name-server routines.
 */
static u_int32	/* RETURNED IN NETWORK ORDER! */
get_host_ip_address(Index, HostName)
const int Index;
const char *HostName;	/* Name or 4 dotted numbers */
{
	u_int32	address;
	struct hostent *hent;
	extern struct	hostent *gethostbyname();

	if ((address = inet_addr(HostName)) != 0xFFFFFFFFUL)
	  return address;

	errno = 0;
	while (NULL == (hent = gethostbyname(HostName))) {
	  logger(1, "UNIX_TCP: Line %s: Can't resolve '%s', error: %s\n",
		 IoLines[Index].HostName, HostName, PRINT_ERRNO);
	  return -1;
	}
	memcpy( &address,hent->h_addr,4 );
	return address;
}



/*
 * writen() -- At least POSIX-systems do tend to return -1, with
 *	       errno = EINTR for various reasons.  If so, retry..
 */
int
writen(fd,buf,len)
const int fd, len;
const void *buf;
{
	const unsigned char *cbuf = (unsigned char *)buf;
	int rc = 0;
	int pos = 0;
	int sleepcnt = 15;		/* Give us 15 seconds ? */
	int loops = 5;


#if	!defined(NBSTREAM)
	setsockblocking(fd,0);
#endif

	while (pos < len && sleepcnt >= 0) {
	  rc = write(fd, cbuf+pos, len-pos);
	  if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
	    sleep(1);
	    --sleepcnt;
	    continue;
	  }
	  if (rc < 0 && errno == EINTR && (--loops > 0)) continue;
	  if (rc < 0)
	    return pos ? pos : rc;  /* If written something already.. */

	  if (rc < (len-pos))
	    logger(2,"UNIX: writen(fd,buf,len=%d) at pos %d wrote %d, errno=%d\n",
		   len,pos,rc,errno);

	  pos += rc;	/* Advance the pointer.. */
	  rc = 0;
	}

#if	!defined(NBSTREAM)
	/* Turn off the non-block mode */
	setsockblocking(fd,1);
#endif

	if (sleepcnt < 0) return -1;

	return rc+pos;
}


/*
 * readn() -- At least POSIX-systems do tend to return -1, with
 *	      errno = EINTR for various reasons.  If so, retry..
 */
int
readn(fd,buf,len)
const int fd, len;
void *buf;
{
	unsigned char *cbuf = (unsigned char *)buf;
	int rc = 0;
	int pos = 0;
	int loops = 5;
	int sleepcnt = 15;

#ifndef	NBSTREAM
	setsockblocking(fd,0);
#endif

	while (pos < len && sleepcnt >= 0) {
	  rc = read(fd, cbuf+pos, len-pos);

	  if (rc == 0) return pos; /* It may return 0 size == EOF */

	  if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
	    --sleepcnt;
	    sleep(1);
	    continue;
	  }
	  if (rc < 0 && errno != EINTR) {
	    return pos ? pos : rc; /* If written something already.. */
	  }

	  if (rc < 0 && (--loops > 0)) rc = 0; /* Just EINTR.. */

	  if (rc < (len-pos))
	    logger(2,"UNIX: readn(fd,buf,len=%d) at pos %d read %d, errno=%d\n",
		   len,pos,rc,errno);

	  pos += rc;	/* Advance the pointer.. */
	  rc = 0;
	}

#if	!defined(NBSTREAM)
	/* Turn off the non-block mode */
	setsockblocking(fd,1);
#endif

	if (sleepcnt < 0) return -1;

	return rc+pos;
}
