/* CONSTS   5.8-mea1.1
 | Copyright (c) 1988,1989,1990,1991 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use oR misuse of this software.

 | A plenty of modifications, Copyright (c) Finnish University and Research
 | Network, FUNET, 1990, 1991, 1993, 1994

 | The constants used by the various programs.
 | We can currently process files destined to one receiver only.
 | NOTE: If this file is included in the MAIN.C file (which defines the symbol
 |       MAIN), we define some global vars. If not, we define them as external.
 |       After an upgrade of EXOS, check that this file is ok.
 |
 | When running on UNIX systems, the function SIZEOF doesn't return always the
 | real size. For dealing with this case, near a structure definition we
 | use define also its size in bytes.

 | mea1.1 - 10-Oct-93 -- removed last remnants of HUJI vs. FUNET
 |			 conditional compilation

 */

/* Some hard-ware dependencies */
#ifndef __U_INT32
#if	defined(__alpha__) /* 64 bit.. */
#define __U_INT32
typedef unsigned int u_int32;
typedef int int32;
typedef unsigned short u_int16;
typedef short int16;
#else
#define __U_INT32
typedef unsigned long u_int32;
typedef long int32;
typedef unsigned short u_int16;
typedef short int16;
#endif
#endif

#include "site_consts.h"
/*   VMS sepcific code   */
#ifdef VMS
#define	NULL	(void *) 0
#include <rms.h>	/* File descriptors for RMS I/O */
unsigned long	DMF_routine_address;	/* The address of DMF framing routine */
#endif

/*   Unix specific includes   */
#ifdef unix
#define	UNIX
#include <stdio.h>	/* Unix standard file descriptors */
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h> /* Sockets definitions */
extern int	errno;

#ifndef NO_GETTIMEOFDAY
# include <sys/time.h>
# define TIMETYPE struct timeval
# define GETTIME(x)	gettimeofday(x,NULL)
# define GETTIMESEC(x)	(x.tv_sec)
#else
# define TIMETYPE time_t
# define GETTIME(x)	time(x)
# define GETTIMESEC(x)	(x)
#endif


#ifndef CONFIG_FILE
#define	CONFIG_FILE	"/etc/funetnje.cf"
#endif
#ifndef PID_FILE
#define PID_FILE	"/etc/funetnje.pid"
#endif

#endif	/* unix */

/* ************************* SYSTEM FEATURE SELECTION ********************* */

#if !defined(NO_NBSTREAM) && !defined(NBCONNECT) && !defined(NBSTREAM)
# define NBSTREAM 1
# define USE_XMIT_QUEUE 1
#endif
#if !defined(NO_SOCKOPT) && !defined(USE_SOCKOPT)
# define USE_SOCKOPT 1
#endif
#if !defined(NO_ENUM_TYPES) && !defined(USE_ENUM_TYPES)
# define USE_ENUM_TYPES 1
#endif

#ifndef MAX_INFORM
#define MAX_INFORM      8
#endif
#ifndef ABSMAX_LINES
#define ABSMAX_LINES    10      /* Maximum 10 lines at present */
#endif

/* if MULTINET or DEC_TCP were defined, define MULTINET_or_DEC
   so we can compile code common to both			*/

#ifdef MULTINET
# define MULTINET_or_DEC
#endif
#ifdef DEC_TCP
# define MULTINET_or_DEC
#endif

#define	MAX_BUF_SIZE	10240	/* Maximum buffer size			*/
#define	MAX_ERRORS	10	/* Maximum recovery trials before restart */
#define	MAX_XMIT_QUEUE	20	/* Maximum pending xmissions for reliable
				   links				*/
#define	MAX_STREAMS	7	/* Maximum streams we support per line	*/

/* VMS treats each external variable as a separate PSECT
   (C is not yet a native language on it...).
   Hence, we use GLOBALDEF and GLOBALREF instead of externals.
*/
#ifdef VMS
#define	INTERNAL	globaldef
#define	EXTERNAL	globalref
#else
#define	INTERNAL	/* Nothing */
#define	EXTERNAL	extern
#endif

INTERNAL int	MAX_LINES;		/* Dynamic! [mea]		*/
#ifdef MAIN
INTERNAL char	BITNET_QUEUE[80];	/* The BITnet queue directory	*/
INTERNAL char   COMMAND_MAILBOX[80];	/* The command channel		*/
INTERNAL char	LOCAL_NAME[10];		/* Our BITnet name (In ASCII)	*/
INTERNAL char	LOCAL_ALIAS[10];	/* Our alias name		*/
INTERNAL char	IP_ADDRESS[80];		/* Our IP address		*/
INTERNAL char	LOG_FILE[80];		/* The LogFile name		*/
INTERNAL char	RSCSACCT_LOG[80];	/* The RSCS-like accounting file*/
INTERNAL char	EBCDICTBL[80];		/* Filename of the EBCDICTBL(5) */
INTERNAL char	TABLE_FILE[80];		/* The routing table		*/
INTERNAL char	ADDRESS_FILE[80];	/* The name of the file holding
					   the DMF routine's address	*/
INTERNAL char	InformUsers[MAX_INFORM][20];	/* Users to inform when
						   a line change state	*/
INTERNAL int	InformUsersCount;	/* How many are in the previous
					   array			*/
INTERNAL char	DefaultRoute[16];	/* Default route node (if exists) */
INTERNAL char	DefaultForm[9];		/* Default for FORM value	*/
INTERNAL char	ClusterNode[16];	/* DECnet name of the cluster's
					   node conncted with NJE to the
					   outer world			*/
#else
EXTERNAL char	BITNET_QUEUE[80];
EXTERNAL char	COMMAND_MAILBOX[80];
EXTERNAL char	LOCAL_NAME[10];
EXTERNAL char	LOCAL_ALIAS[10];
EXTERNAL char	IP_ADDRESS[80];
EXTERNAL char	TABLE_FILE[80];
EXTERNAL char	LOG_FILE[80];
EXTERNAL char	EBCDICTBL[80];
EXTERNAL char	RSCSACCT_LOG[80];
EXTERNAL char	ADDRESS_FILE[80];
EXTERNAL char	InformUsers[MAX_INFORM][20];
EXTERNAL int	InformUsersCount;
EXTERNAL char	DefaultRoute[16];
EXTERNAL char	DefaultForm[9];
EXTERNAL char	ClusterNode[16];
#endif

#define	VERSION		"3.2."	/* Major.Minor.Edition			*/
#define	SERIAL_NUMBER	"mea"	/* Don't touch it !!!			*/
#define	LICENSED_TO	"  *** From sources of FINFILES origin ***"

#define	DEVLEN		64	/* Device name length (must be at least
				   9 chars long)			*/
#define	LINESIZE	256	/* For various internal usage		*/
#define	SHORTLINE	80	/* Same, but shorter...			*/

/* Line type */
#define	DMF		1	/* DMF line, or DMB in GENBYTE mode	*/
#define	EXOS_TCP	2	/* TCP line using EXOS			*/
#define	MNET_TCP	3	/* MultiNet TcpIp			*/
#define	DECNET		4	/* DECnet line to other RSCS daemon	*/
#define	X_25		5	/* Just thoughts for the future...	*/
#define	ASYNC		6	/* Same...				*/
#define	UNIX_TCP	7	/* Unix standard sockets calls		*/
#define	DMB		8	/* DMB in BiSync mode			*/
#define	DEC__TCP	9	/* DEC's TcpIp package			*/
#define	DSV		10	/* DSV-32 synchronous interface for uVAXes */

/* Line state. Main state (Line.state). After changing, correct the states[]
   array in PROTOCOL.C, before the function Infrom-users-about-lines.	*/

#ifndef USE_ENUM_TYPES
#define	INACTIVE	0	/* Not active - do not try to
				   send/receive				*/
#define	SIGNOFF		1	/* Same as Inactive, but was intially
				   active..				*/
#define	DRAIN		2	/* Line is drained - try to start line	*/
#define	I_SIGNON_SENT	3	/* Initial sign-on sent			*/
#define	ACTIVE		4	/* Line is working, or trying to...	*/
#define	F_SIGNON_SENT	5	/* Final sign-on sent			*/
#define	LISTEN		6	/* Unix TCP: Listen was issued,
				   ACCEPT is needed			*/
#define	RETRYING	7	/* TCP - Will retry reconnect later.	*/
#define	TCP_SYNC	8	/* TCP - accepting a connection.	*/
typedef int LinkStates;
#else
typedef enum {
  INACTIVE = 0, SIGNOFF, DRAIN, I_SIGNON_SENT, ACTIVE, F_SIGNON_SENT,
  LISTEN, RETRYING, TCP_SYNC } LinkStates;
#endif

/* Line state - for each stream (xxStreamState) */
#ifndef USE_ENUM_TYPES
#define	S_INACTIVE	0	/* Inactive but allows new requests	*/
#define	S_REFUSED	1	/* Refused - don't use it		*/
#define	S_WAIT_A_BIT	2	/* In temporary wait state		*/
#define	S_WILL_SEND	3	/* Request to start a transfer will be
				   sent soon				*/
#define	S_NJH_SENT	4	/* Network Job Header was sent		*/
#define	S_NDH_SENT	5	/* DataSet header sent (if needed)	*/
#define	S_SENDING_FILE	6	/* During file sending			*/
#define	S_NJT_SENT	7	/* File ended, sending Network Job Trailer */
#define	S_EOF_SENT	8	/* File ended, EOF sent, waiting for ACK */
#define	S_EOF_FOUND	9	/* EOF found, but not sent yet		*/
#define	S_REQUEST_SENT	10	/* Request to start transfer actually sent */
#define S_RECEIVING_FILE 11	/* During file receiving		*/
typedef short StreamStates;
#else
typedef enum {
	S_INACTIVE = 0, S_REFUSED, S_WAIT_A_BIT, S_WILL_SEND, S_NJH_SENT,
	S_NDH_SENT, S_SENDING_FILE, S_NJT_SENT, S_EOF_SENT, S_EOF_FOUND,
	S_REQUEST_SENT, S_RECEIVING_FILE } StreamStates;
#endif

/* Line flags (Bit masks). When Adding flags correct Restart_channel()
   function in PROTOCOL.C to reset the needed flags.			*/
#define	F_RESET_BCB	0x0001	/* Shall we send the next frame with
				   Reset-BCB?				*/
#define	F_WAIT_A_BIT	0x0002	/* Has the other side sent Wait-a-bit FCS? */
#define	F_SUSPEND_ALLOWED 0x0004	/* Do we allow suspend mode?	*/
#define	F_IN_SUSPEND	0x0008	/* Are we now in a suspend?		*/
#define	F_ACK_NULLS	0x0010	/* Shall we ack with null blocks instead
				   of DLE-ACK?				*/
/* BIT 32 IS FREE FOR USE */
#define	F_SHUT_PENDING	0x0020	/* Shut this line when all files are done. */
#define	F_FAST_OPEN	0x0040	/* For TCP - Other side confirmed
				   OPEN locally				*/
#define	F_SENDING	0x0080	/* DECnet/EXOSx - $QIO called but the final AST
				   not fired yet - we can't write now.	*/
#define	F_DONT_ACK	0x0100	/* DECnet/TCP is sending, so don't try to send
				   more now.				*/
#define	F_XMIT_QUEUE	0x0200	/* This link supports queue of pending
				   xmissions				*/
#define	F_XMIT_MORE	0x0400	/* For TCP links - there is room in TCP buffer
				   for more blocks.			*/
#define	F_XMIT_CAN_WAIT	0x0800	/* In accordance with the previous flag;
				   the current SEND operation can handle
				   the F_XMIT_MORE flag.  If clear, TCP
				   must send now.			*/
#define	F_CALL_ACK	0x1000	/* For UNIX - the main loop should call
				   HANDLE_ACK instead of being called
				   from the TCP send routine (to not go
				   too deep in stack)			*/
#define	F_WAIT_V_A_BIT	0x2000	/* Wait a bit for TCP and DECnet links.
				   We use this flag to not interfere with
				   the normal NJE flag.			*/
#define	F_HALF_DUPLEX	0x4000	/* Run the line in Half duplex mode (DMF and
				   DMB only.				*/
#define	F_AUTO_RESTART	0x8000	/* If the line enters INACTIVE state (DMF and
				   DMB only) then try to restart it after 10
				   minutes).				*/
#define	F_RELIABLE	0x10000	/* This is a reliable link (TCP or DECNET).
				   Means that it supports xmit-queue	*/
#define F_SLOW_INTERLEAVE 0x20000 /* For VMNET links do slow interleave of
				     outgoing (SENDING) streams -- switch
				     at each TCP block, not at each record */

/* Codes for timer routine (What action to do when timer expires) */
#ifndef USE_ENUM_TYPES
#define	T_DMF_CLEAN	1	/* Issue $QIO with IO$_CLEAN function for DMF*/
#define	T_CANCEL	2	/* Issue $CANCEL system service		*/
#define	T_SEND_ACK	3	/* Send an ACK. Used to insert a delay before
				   sending ACK on an idle line.		*/
#define	T_AUTO_RESTART	5	/* Try starting lines which have
				   the AUTO-RESTART flag and are
				   in INACTIVE mode			*/
#define	T_TCP_TIMEOUT	6	/* timeout on TCP sockets, and VMS TCPs	*/
#define	T_POLL		7	/* Unix has to poll sockets once a second */
#define	T_STATS		9	/* Time to collect our statistics	*/
#define	T_ASYNC_TIMEOUT	10	/* Timeout on Asynchronous lines.	*/
#define	T_DECNET_TIMEOUT 11	/* Timeout to keep DECnet pseudo-Ack	*/
#define	T_DMF_RESTART	13	/* Restart a DMF/DMB line		*/
#define T_VMNET_MONITOR	14	/* "VMNET MONITOR"-process		*/
#define T_XMIT_DEQUEUE  15	/* USE_XMIT_QUEUE in use.. 		*/
typedef unsigned short TimerType;
#else
typedef enum {
  T_DMF_CLEAN, T_CANCEL, T_SEND_ACK, T_AUTO_RESTART, T_TCP_TIMEOUT,
  T_POLL, T_STATS, T_ASYNC_TIMEOUT, T_DECNET_TIMEOUT, T_DMF_RESTART,
  T_VMNET_MONITOR, T_XMIT_DEQUEUE } TimerType;
#endif
#define	T_STATS_INTERVAL 3600	/* Compute each T_STATS_INT seconds.	*/
#define	T_AUTO_RESTART_INTERVAL 60 /* Retry granularity is 1 minute.	*/
#define T_VMNET_INTERVAL	60 /* "VMNET MONITOR" interval.         */
#define T_XMIT_INTERVAL		 2 /* Be rather snappy to dequeue.. 	*/

/* General status */
#define	G_INACTIVE	0	/* Not active or not allowed */
#define	G_ACTIVE	1	/* Active or allowed */

/* For the routine to get the link on which to send the message. All are
   negative. Set by Find_line_index(). After adding flags check all the
   places where it is used. */
#define	NO_SUCH_NODE	-1	/* No such node exists			*/
#define	LINK_INACTIVE	-2	/* Node exists, but inactive and no
				   alternate route exists		*/
#define	ROUTE_VIA_LOCAL_NODE -3 /* Connected but not via NJE		*/

/* The command mailbox messages' codes */

#define	CMD_SHUTDOWN_ABRT 1	/* Shutdown the NJE emulator immediately */
#define	CMD_SHUTDOWN	14	/* Shut down after all lines are idle.	*/
#define	CMD_SHOW_LINES	2	/* Show lines status			*/
#define	CMD_QUEUE_FILE	3	/* Add a file to the queue		*/
#define	CMD_SHOW_QUEUE	4	/* Show the files queued		*/
#define	CMD_SEND_MESSAGE 5	/* Send NMR message			*/
#define	CMD_SEND_COMMAND 6	/* Send NMR command			*/
#define	CMD_START_LINE	7	/* Start an INACTIVE or SIGNOFF line	*/
#define	CMD_STOP_LINE	8	/* Stop the line after the current file.*/
#define	CMD_START_STREAM 9	/* Start a REFUSED stream		*/
#define	CMD_FORCE_LINE	11	/* Shut that line immediately.		*/
#define	CMD_FORCE_STREAM 12	/* Shut this stream immediately		*/
#define	CMD_DEBUG_DUMP	15	/* DEBUG - Dump all buffers to logfile	*/
#define	CMD_DEBUG_RESCAN 16	/* DEBUG - Rescan queue diretory
				   (to be used when manually rerouting
				   files to another line)		*/
#define	CMD_LOGLEVEL	17	/* Change log level during run		*/
#define	CMD_CHANGE_ROUTE 18	/* Change the route in database		*/
#define	CMD_GONE_ADD	19	/* Register a user in GONE database by YGONE */
#define	CMD_GONE_DEL	20	/* Remove a user from GONE database by YGONE */
#define	CMD_GONE_ADD_UCP 21	/* Register a user in GONE database by UCP */
#define	CMD_GONE_DEL_UCP 22	/* Remove a user from GONE database by UCP */
#define CMD_MSG_REGISTER 23	/* An MSG-registry command		*/
#define CMD_EXIT_RESCAN 24	/* Do a exit-rescan w/o sending SIGHUP	*/
#define CMD_ROUTE_RESCAN 25	/* Do a route-rescan			*/

typedef enum { CMD_MSG, CMD_CMD } NMRCODES;
/* #define CMD_MSG	0 */	/* This is a message NMR		*/
/* #define CMD_CMD	1 */	/* This is a command NMR		*/

/* For send_data() routine: */
#define	ADD_BCB_CRC	1	/* Add BCB+FCS in the beginning,
				   ETB+CRC at the end			*/
#define	SEND_AS_IS	2	/* Do not add anything			*/

/* The codes that a message can be in */
typedef enum { ASCII, EBCDIC, BINARY } MSGCODES;
/* #define	ASCII		0 */
/* #define	EBCDIC		1 */
/* #define	BINARY		2 */

/* The file's type (Bitmask) */
#define	F_MAIL		0x0	/* Mail message				*/
#define	F_FILE		0x0001	/* File - send it in NETDATA format	*/
#define	F_PUNCH		0x0002	/* File - send it in PUNCH format	*/
#define	F_PRINT		0x0004	/* File - send it in PRINT format	*/
#define	F_ASA		0x0008	/* ASA carriage control			*/
#define F_JOB		0x0010	/* SYSIN job!				*/
#define	F_NOQUIET	0x0080	/* Don't use the QUIET form code	*/

/* For Close_file routine: */
#define	F_INPUT_FILE	0	/* Input file				*/
#define	F_OUTPUT_FILE	1	/* Output file (from the program)	*/

/* For rename_received_file: */
#define	RN_NORMAL	0	/* Normal rename to correct queue	*/
#define	RN_JUNK		1	/* Junk the file			*/
#define	RN_HOLD_ABORT	2	/* Hold it because of sender's abort	*/

/* Offsets inside the received/sent block: */
#define	BCB_OFFSET	2	/* BCB is the 3rd byte			*/
#define	FCS1_OFFSET	3	/* First byte of FCS			*/
#define	FCS2_OFFSET	4	/* Second byte				*/
#define	RCB_OFFSET	5	/* Record control block			*/
#define	SRCB_OFFSET	6	/* SRCB					*/

/* The RCB types: */
#define	SIGNON_RCB	0xF0	/* Signon record			*/
#define	NULL_RCB	0x00	/* End of block RCB			*/
#define	REQUEST_RCB	0x90	/* Request to initiate a stream		*/
#define	PERMIT_RCB	0xA0	/* Permission to initiate a stream	*/
#define	CANCEL_RCB	0xB0	/* Negative permission or receiver cancel */
#define	COMPLETE_RCB	0xC0	/* Stream completed			*/
#define	READY_RCB	0xD0	/* Ready to receive a stream		*/
#define	BCB_ERR_RCB	0xE0	/* Received BCB was in error		*/
#define	NMR_RCB		0x9A	/* Nodal message record			*/
#define	SYSOUT_0	0x99	/* SYSOUT record, stream 0		*/
#define	SYSOUT_1	0xA9	/* SYSOUT record, stream 1		*/
#define	SYSOUT_2	0xB9	/* SYSOUT record, stream 2		*/
#define	SYSOUT_3	0xC9	/* SYSOUT record, stream 3		*/
#define	SYSOUT_4	0xD9	/* SYSOUT record, stream 4		*/
#define	SYSOUT_5	0xE9	/* SYSOUT record, stream 5		*/
#define	SYSOUT_6	0xF9	/* SYSOUT record, stream 6		*/
#define	SYSIN_0 	0x98	/* SYSIN record, stream 0		*/
#define	SYSIN_1		0xA8	/* SYSIN record, stream 1		*/
#define	SYSIN_2 	0xB8	/* SYSIN record, stream 2		*/
#define	SYSIN_3 	0xC8	/* SYSIN record, stream 3		*/
#define	SYSIN_4 	0xD8	/* SYSIN record, stream 4		*/
#define	SYSIN_5 	0xE8	/* SYSIN record, stream 5		*/
#define	SYSIN_6 	0xF8	/* SYSIN record, stream 6		*/
typedef unsigned char RCBs;

/* SRCB types: */
#define	NJH_SRCB	0xC0	/* Job Header				*/
#define	DSH_SRCB	0xE0	/* Data set header			*/
#define	NJT_SRCB	0xD0	/* Job trailer				*/
#define	DSHT_SRCB	0xF0	/* Data set trailer. Not used		*/
#define	CC_NO_SRCB	0x80	/* No carriage control record		*/
#define	CC_MAC_SRCB	0x90	/* Machine carriage control record	*/
#define	CC_ASA_SRCB	0xA0	/* ASA carriage control			*/
#define	CC_CPDS_SRCB	0xB0	/* CPDS carriage control		*/
typedef unsigned char SRCBs;

/* SCB types: */
#define	EOS_SCB		0	/* End of string or end of file		*/
#define	ABORT_SCB	0x40	/* Abort transmission			*/

/* For  send_njh_dsh_record(): */
#define	SEND_NJH	0	/* Send NJH record			*/
#define	SEND_DSH	1	/* Send DSH record			*/
#define	SEND_DSH2	2	/* Send DSH record, fragment 2		*/




/* Commonly used macros. */
#ifdef VMS
#define	htons(xxx)	(((xxx >> 8) & 0xff) | ((xxx & 0xff) << 8))
#define	ntohs(xxx)	(((xxx >> 8) & 0xff) | ((xxx & 0xff) << 8))
#define	ntohl(xxx)	(((xxx >> 24) & 0xff) | ((xxx & 0xff0000) >> 8) | \
			 ((xxx & 0xff00) << 8) | ((xxx & 0xff) << 24))
#define	htonl(xxx)	(((xxx >> 24) & 0xff) | ((xxx & 0xff0000) >> 8) | \
			 ((xxx & 0xff00) << 8) | ((xxx & 0xff) << 24))



/* Commonly used structures definitions */
struct	DESC	{		/* String descriptor */
	short	length;
	short	type;	/* Set it to zero */
	char	*address;
};
#endif

/* Statistics structure (used only in debug mode) */
struct	STATS {
	int	TotalIn, TotalOut,	/* Total number of blocks
					   transmitted			*/
		WaitIn, WaitOut,	/* Number of Wait-A-Bits	*/
		AckIn, AckOut,		/* Number of "Ack-only" blocks	*/
		MessagesIn, MessagesOut, /* Number of interactive
					    messages blocks		*/
		RetriesIn, RetriesOut;	/* Total NAKS			*/
};

/* The queue structure */
struct	QUEUE {
	struct	QUEUE	*next;		/* Next queue item	*/
	int	FileSize;		/* File size in blocks	*/
	int	state;			/* 0: waiting, >0: sending, <0: hold */
	unsigned char	FileName[LINESIZE];	/* File name	*/
	unsigned char	ToAddr[20];		/* Destination node */
	u_int32	hash;			/* For quick comparing.. */
	struct QUEUE *hashnext;
	int	altline;	/* Mark which line is primary route,
				   and tell possible alternate one.
				   Value  -1 == no alternate, queued
				   only on the primary.		*/
	int	primline;	/* We need this too..		*/
#ifdef	UNIX
	struct stat fstats;		/* File information..   */
#endif
};

/* File parameters for the file in process */
struct	FILE_PARAMS {
	long		format,		/* Character set		*/
			type,		/* Mail/File			*/
			FileId,		/* Pre-given ID	-- OID/FID use has  */
			OurFileId,	/* Our local ID -- altered meanings */
			flags,		/* Our internal flags		*/
			RecordsCount,	/* How many records		*/
			FileSize;	/* File size in bytes		*/
	TIMETYPE	XmitStartTime,	/* For speed determination	*/
			RecvStartTime;	/* In and outbound...		*/
	time_t		NJHtime;	/* Arrived file's NJH time	*/
	struct	QUEUE	*FileEntry;	/* Current file's queue ptr	*/
	struct stat	FileStats;	/* standard (UNIX) file params	*/
	char	SpoolFileName[LINESIZE],/* Spool file name		*/
			FileName[20],	/* IBM Name			*/
			FileExt[20],	/* IBM Extension		*/
			From[20],	/* Sender			*/
			To[20],		/* Receiver			*/
			PrtTo[20],	/* SYSIN print routing		*/
			PunTo[20],	/* SYSIN punch routing  	*/
			line[20],	/* On which line to queue	*/
			FormsName[10],	/* RSCS forms name		*/
			DistName[10],	/* RSCS DIST name		*/
			JobName[20],	/* RSCS job name		*/
			JobClass,	/* RSCS class			*/
			tag[137];	/* RSCS tag field		*/
} ;

struct	MESSAGE {			/* For outgoing interactive messages */
	struct MESSAGE	*next;		/* Next in chain		*/
	int		length;		/* Message's length		*/
	char	text[LINESIZE];		/* The message's text		*/
	char	Faddress[20];		/* From address			*/
	char	node[10];		/* Target node			*/
	char	type;			/* CMD_CMD / CMD_MSG		*/
	char	candiscard;		/* non-zero if can discard	*/
};

#ifdef VMS
/* From the EXOS include files: */
struct in_addr {
	union {
		struct { char s_b1,s_b2,s_b3,s_b4; } S_un_b;
		struct { unsigned short s_w1,s_w2; } S_un_w;
		long S_addr;
	} S_un;
};

struct sockaddr_in {
	short		sin_family;	/* family			*/
	unsigned short	sin_port;	/* port number			*/
	struct in_addr	sin_addr;	/* Internet address		*/
	char		sin_zero[8];	/* unused, must be 0		*/
};

struct sockproto {
	short	sp_family;		/* protocol family		*/
	short	sp_protocol;		/* protocol within family	*/
};

struct	SOioctl {
	short	hassa;			/* non-zero if sa specified	*/
	union	{
		struct sockaddr_in sin;	/* socket address (internet style) */
	} sa;
	short	hassp;			/* non-zero if sp specified	*/
	struct	sockproto sp;		/* socket protocol (optional)	*/
	int	type;			/* socket type			*/
	int	options;		/* options			*/
};

/* For MultiNet: */
#ifdef MULTINET
struct sockaddr {
	short		sa_family;	/* family			*/
	unsigned short	sa_port;	/* port number			*/
	unsigned long	sa_addr;	/* Internet address		*/
	char		sa_zero[8];	/* unused, must be 0		*/
};
#endif
#ifdef DEC_TCP
struct sockaddr {
	short	inet_family;
	short	inet_port;
	long	inet_adrs;
	char	blkb[8];
};
#endif
#endif /* VMS */

/* The definition of the lines */
struct	LINE	{
	short	type;		/* Line type (DMF, TCP, etc)		*/
	char	device[DEVLEN];	/* Device name to use for DMF and ASYNC lines,
				   DECnet name to connect to in DECnet links */
	char	HostName[DEVLEN]; /* The other side name		*/
	int	TimerIndex;	/* Index in timer queue for timeout	*/
	int	TotalErrors,	/* Total number of errors accumulated 	*/
		errors;		/* Consecutive errors (for error routine) */
	LinkStates
		state;		/* Main line state			*/

	int	IpPort,		/* The IP port number to use		*/
		InBCB,		/* Incoming BCB count			*/
		OutBCB,		/* Outgoing BCB count			*/
		CurrentStream,	/* The stream we are sending now	*/
		ActiveStreams,	/* Bitmap of streams we are sending now	*/
		FreeStreams,	/* Number of inactive streams we can use */
		flags;		/* Some status flags regarding this line. */
	int	MaxStreams;	/* Maximum streams active on this line	*/
	int	LastActivatedStream;
#define MAX_RETRIES 10		/* Lets say, 9 different values..	*/
	int	RetryPeriods[MAX_RETRIES];	/* Can do longer waits	*/
	int	RetryPeriod;	/* Active Retry period value.		*/
	int	RetryIndex;	/* in between failures.. 5, 10, 30, ..min */
#ifdef UNIX
	struct	sockaddr_in	/* For TcpIp communication		*/
		SocketName;	/* The socket's name and address	*/
	int	socket;		/* The socket FD			*/
	int	socketpending;	/* Flag is the open is just starting..
				   After all, we can do it somewhat
				   asynchronoysly -- try anyway.	*/

	FILE	*InFds[MAX_STREAMS];	/* The FD for each stream (Sending) */
	long	InFilePos[MAX_STREAMS];
	FILE	*OutFds[MAX_STREAMS];	/* The FD for each stream (Recv) */
	long	OutFilePos[MAX_STREAMS];
#endif /* UNIX */
#ifdef VMS
	short	channel,	/* IO channel				*/
		iosb[4],	/* IOSB for that channel		*/
		Siosb[4];	/* Another IOSB for full-duplex lines	*/

# ifdef EXOS
	struct SOioctl LocalSocket,	/* Local socket structure for EXOS */
		RemoteSocket;	/* The other side			*/
# endif
# ifdef MULTINET
	struct	sockaddr MNsocket;	/* Remote socket for MultiNet	*/
# endif
# ifdef DEC_TCP
	struct	sockaddr LocalSocket, RemoteSocket;
# endif
	struct FAB InFabs[MAX_STREAMS];	 /* The FAB for each stream (Sending)*/
	struct FAB OutFabs[MAX_STREAMS]; /* The FAB for each stream (Recv) */
	struct RAB InRabs[MAX_STREAMS];		/* The RAB for each stream */
	struct RAB OutRabs[MAX_STREAMS];
#endif /* VMS */

	StreamStates
		InStreamState[MAX_STREAMS],	/* This stream state	*/
		OutStreamState[MAX_STREAMS];	/* This stream state	*/
	struct	FILE_PARAMS
		InFileParams[MAX_STREAMS],	/* Params of file in process */
		OutFileParams[MAX_STREAMS];

	short	TimeOut,	/* Timeout value			*/
		PMaxXmitSize,	/* Proposed maximum xmission size	*/
		MaxXmitSize;	/* Agreed maximum xmission size		*/
	struct	QUEUE		/* The files queued for this line:	*/
		*QueueStart, *QueueEnd;	/* Both sides of the queue	*/
	int	QueuedFiles,	/* Number of files queued for this link
				   at this moment (held+sending+waiting)*/
		QueuedFilesWaiting; /* Non-hold, non-sending files	*/
	struct	MESSAGE		/* Interactive messages waiting for this line*/
		*MessageQstart, *MessageQend;

	TIMETYPE  InAge, XmitAge; /* Timestamps of last read/write */
	int	XmitSize;	/* Size of contents of this buffer	*/
	void	*WritePending;	/* Flag that we have a write pending	*/
	int	WriteSize;	/* When non-block writing, pre-written cnt */
	int	RecvSize;	/* Size of receive buffer (for TCP buffer
				   completion				*/
	unsigned long	TcpState, /* Used during receipt to TCP stream	*/
		TcpXmitSize;	/* Up to what size to block TCP xmissions */

	/* For lines that support transmit queue (XMIT_QUEUE flag is on).
	   These are normally only reliable links (TCP and DECnet)	*/
#ifdef	USE_XMIT_QUEUE
	char	*XmitQueue[MAX_XMIT_QUEUE];	/* Pointers to buffers	*/
	int	XmitQueueSize[MAX_XMIT_QUEUE];	/* Size of each entry	*/
	int	FirstXmitEntry, LastXmitEntry;	/* The queue bounds	*/
#endif

	struct	STATS	stats;		/* Line's statistics		*/
	struct	STATS	sumstats;	/* Line's cumulative statistics	*/
	long	WrFiles, WrBytes;	/* Cumulative for monitoring .. */
	long	RdFiles, RdBytes;
	unsigned char InBuffer[MAX_BUF_SIZE],	/* Buffer received	*/
		      XmitBuffer[MAX_BUF_SIZE];	/* Last buffer sent	*/

	/* The received NJH, DSH, and NJT for analysis at reception stream
	   closing time.  These buffers get defragmented headers, and are
	   analyzed for appropriate contents at convenient moment..	*/
#define JOBHEADERSIZE 216	/* 204 -- allocate multiple of 8..	*/
#define DATASETHEADERSIZE 304	/* 296 with RSCS TAGs			*/
#define	JOBTRAILERSIZE 56	/* 48					*/
	unsigned char	SavedJobHeader[MAX_STREAMS][JOBHEADERSIZE],
			SavedDatasetHeader[MAX_STREAMS][DATASETHEADERSIZE],
			SavedJobTrailer[MAX_STREAMS][JOBTRAILERSIZE];
	int		SizeSavedJobHeader[MAX_STREAMS],
			SizeSavedDatasetHeader[MAX_STREAMS],
			SizeSavedJobTrailer[MAX_STREAMS];
	/* Actually we do nothing with NJT's -- except note their presence.
	   MVS systems can (AND DO!) send multiple DSH:s within a file!    */
};


/* Some usefull constants: */
/* If we are in MAIN, define the constants. If not, define them as external,
   to solve compilation problems on SUN.
*/
#ifdef MAIN
unsigned char EightNulls [10] = "\0\0\0\0\0\0\0\0";
unsigned char EightSpaces[10] = "\100\100\100\100\100\100\100\100"; /* 8 EBCDIC spaces */
INTERNAL unsigned char E_BITnet_name[10];	/* Our BITnet name in Ebcdic */
INTERNAL int	E_BITnet_name_length;		/* It's length */
#else
extern unsigned char EightNulls[10];
extern unsigned char EightSpaces[10];
EXTERNAL unsigned char E_BITnet_name[10];
EXTERNAL int	E_BITnet_name_length;
#endif


/*
 | The block headers used by VMnet version 2
 */
#define	TTB_SIZE	8
struct	TTB {			/* Data block header */
	unsigned char	F,	/* Flags */
			U;	/* Unused */
	u_int16		LN;	/* Length */
	u_int32		UnUsed;	/* Unused */
	};

#define	TTR_SIZE	4
struct	TTR {			/* Record header */
	unsigned char	F,	/* Flags */
			U;	/* Unused */
	u_int16		LN;	/* Length */
	};

#define	VMctl_SIZE	33
struct	VMctl {			/* Control record of VMnet */
	unsigned char	type[8],	/* Request type */
			Rhost[8];	/* Sender name */
	u_int32		Rip;		/* Sender IP address */
	unsigned char	Ohost[8];	/* Receiver name */
	u_int32		Oip;		/* Receiver IP address */
	unsigned char	R;		/* Error Reason code */
				/* Some processors pad this to 36.. */
	};


struct ROUTE_DATA {
	char	DestNode[9];
	char	DestLink[9];
	char	Fmt;
};

#ifdef	MAIN
INTERNAL struct	LINE	IoLines[ABSMAX_LINES];
#else
EXTERNAL struct	LINE	IoLines[ABSMAX_LINES];
#endif
EXTERNAL char		*version_time;
