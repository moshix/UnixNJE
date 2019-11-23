/*	FUNET-NJE/HUJI-NJE		Client utilities
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 26-Sep-1993
 */

/* Slightly edited copy of this is inside the   file_queue.c! */

#include "consts.h"
#include "prototypes.h"
#include "clientutils.h"

#ifdef AF_UNIX
#include <sys/un.h>
#endif
#include <sysexits.h>

#if 0
typedef enum { CMD_FIFO, CMD_SOCKET, CMD_UDP, CMD_UNKNOWN } cmdbox_t;
static cmdbox_t cmdbox = CMD_UNKNOWN;
#endif
static int gotalarm = 0;

static void send_alarm()
{
  gotalarm = 1;
}

int  send_cmd_msg(cmdbuf,cmdlen,offlineok)
const void *cmdbuf;
const int cmdlen, offlineok;
{
	u_int32	accesskey;
	int	oldgid, rc = 0, fd;
	char	buf[LINESIZE];
	char	buflen = cmdlen+4;
	FILE	*pidfile;
	int	hujipid = 0;
	extern	int errno;
	char *socknam = COMMAND_MAILBOX;

	/* Skip the initial token.. */
	while (*socknam != ' ' && *socknam != '\t' && *socknam != 0)
	  ++socknam;
	while (*socknam == ' ' || *socknam == '\t')
	  ++socknam;

	gotalarm = 0;

	oldgid = getgid();
	setgid(getegid());

	if ((pidfile = fopen(PID_FILE,"r"))) {
	  fscanf(pidfile,"%d",&hujipid);
	  fclose(pidfile);
	}

	/* XX: Multi-machine setups ??? */

	if (!hujipid ||( kill(hujipid,0) && errno == ESRCH)) {
	    if (offlineok) return 0;
	    logger(1,"NJECLIENTLIB: NJE transport module not online!\n");
	    return EX_CANTCREAT;
	}


	sprintf(buf,"%s/.socket.key",BITNET_QUEUE);
	accesskey = 0;
	if ((fd = open(buf,O_RDONLY,0)) >= 0) {
	  read(fd,(void*)&accesskey,4);
	  close(fd);
	} else {
	  logger(1,"NJECLIENTLIB: Can't read access key, errno=%s\n",PRINT_ERRNO);
	  /* DON'T QUIT QUITE YET! */
	}
	*(u_int32*)&buf[0] = accesskey;
	memcpy(buf+4,cmdbuf,cmdlen);

	if (*COMMAND_MAILBOX == 'F' || /* FIFO */
	    *COMMAND_MAILBOX == 'f') {

	  int fd;
	  

	  signal(SIGALRM, send_alarm);
	  alarm(60);		/* 60 seconds.. */

	  fd = open(socknam,O_WRONLY,0);
	  if (fd < 0) {
	    if (errno == EACCES) {
	      logger(1,"NJECLIENTLIB:  No permission to open W-only the `%s'\n",
		     socknam);
	      return EX_NOPERM;
	    } else {
	      logger(1,"NJECLIENTLIB: Failed to open CMDMAILBOX into WRONLY mode!  Error: %s\n",PRINT_ERRNO);
	      if (offlineok)
		return 0;
	      else
		return EX_CANTCREAT;
	    }
	  }

	  write(fd, buf, buflen);
	  close(fd);

	} else if (*COMMAND_MAILBOX == 'S') /* SOCKET */ {

#ifndef AF_UNIX
	  logger(1,"NJECLIENTLIB: This platform does not support AF_UNIX SOCKET-type IPC\n");
	  return EX_SOFTWARE;
#else

	  int	Socket, i;
	  struct	sockaddr_un	SocketName;

	  /* Create a local socket */
	  if ((Socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
	    perror("Can't create command socket");
	    return(errno);
	  }

	  memset((void *) &SocketName, 0, sizeof(SocketName));

	  /* Create the connect request. We connect to our local machine */
	  SocketName.sun_family = AF_UNIX;
	  strcpy(SocketName.sun_path, socknam);

	  i = sizeof(SocketName.sun_family) + strlen(SocketName.sun_path);

	  signal(SIGALRM, send_alarm);
	  alarm(60);		/* 60 seconds.. */

	  if (connect(Socket, (struct sockaddr *)&SocketName, i) < 0) {
	    perror("Access not permitted");
	  } else if (write(Socket, buf, buflen) < 0) {
	    perror("Can't send command");
	    if (offlineok)
	      return 0;
	    return EX_CANTCREAT;
	  }
	  close(Socket);

#endif

	} else if (*COMMAND_MAILBOX == 'U') { /* UDP */

	  int	Socket;
	  int	portnum = 175;
	  u_int32 ui;
	  struct	sockaddr_in	SocketName;

	  /* Create a local socket */
	  if ((Socket = socket( AF_INET, SOCK_DGRAM, 0)) == -1) {
	    perror("Can't create command socket");
	    if (offlineok)
	      return 0;
	    else
	      return EX_IOERR;
	  }

	  /* Create the connect request. We connect to our local machine */
	  memset((void *) &SocketName, 0, sizeof(SocketName));
	  SocketName.sin_family = AF_INET;
	  ui = inet_addr(socknam);
	  portnum = 175;	/* The VMNET port - except on UDP.. */
	  sscanf(socknam,"%*s %d",&portnum); /* Fail or no.. */
	  SocketName.sin_port   = htons(portnum);
	  if (ui == 0xFFFFFFFF)  ui = INADDR_LOOPBACK;
	  SocketName.sin_addr.s_addr = ui; /* Configure if you can */
	
	  rc = 0;
	  if (sendto(Socket, buf, buflen, 0,
		     &SocketName,sizeof(SocketName)) == -1) {
	    perror("Can't send command");
	    if (!offlineok)
	      rc = EX_IOERR;
	  }

	  close(Socket);

	} else {
	  logger(1,"NJECLIENTLIB: The CMDMAILBOX is not configured properly, known interface-types are (Socket, Fifo, UDP)!\n");
	  setgid(oldgid);
	  return EX_SOFTWARE;;
	}

	alarm(0);
	setgid(oldgid);
	return rc;
}
