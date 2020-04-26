/* MAIN.C   V-2.8-mea
 | Copyright (c) 1988,1989,1990,1991 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use oR misuse of this software.
 |
 | The main module for the RSCS. Initialyze the things, queues the read
 | requests and sleeps...
 | The initial state of a line is ACTIVE. When the receive QIO is queued for
 | an active line, its status is changed to DRAIN to show the real state.
 |  The IoLines database is defined here. All the other modules point to it
 | (defined as external).
 |
 | This program can be run in debug mode. For doing so, first compile it when
 | the DEBUG compilation variable is defined. Then, define it as foreign DCL
 | symbol, and run it with one parameter (1 - 4) which the logging level.
 | The default logging level is 1.
 |
 | NOTE: The buffer size defined in CONSTS.H must be larger in 20 characters
 |       than the maximum buffer size used for a line. This is because there
 |       is an overhead of less than 20 characters for each block.
 |
 | Currently, the program supports only single stream, and doesn't support
 | splitted records nor SYSIN files.
 |
 | The value of the variable MustShutDown defines whether we have to shut down
 | or not. Zero = keep running. 1 = Shutdown immediately. -1 = shutdown when
 | all lines are signed-off.
 |
 | Sections: MAIN - The main section.
 |           INIT-LINE-DB - Read the permanent database into memory.
 |           LOGGER - The logging routines.
 |           INIT - Initialyze some commonly used variables.
 */
#include <stdio.h>
FILE	*AddressFd;	/* Temporary - for DMF routine address */

#define	MAIN		/* So the global variables will be defined here */
#include "consts.h"	/* Our RSCS constants */
#include "headers.h"
#include "prototypes.h"
#include <sysexits.h>	/* BSD UNIXoid thing */

static void  init_lines_data_base __(( void ));
static void  init_variables __(( void ));
static void  handle_sigsegv();
#ifdef SIGBUS /* Not everybody has strict alignments -- RISC processors
		 usually do, and raise a SIGBUS at such conditions. */
static void  handle_sigbus();
#endif

INTERNAL int	LogLevel;	/* Referenced in READ_CONFIG.C also	*/
INTERNAL int	MustShutDown;	/* For UNIX - When we have to exit
				   the main loop			*/
FILE	*LogFd = NULL;

/* For the routine that initialize static variables: */
EXTERNAL struct	JOB_HEADER	NetworkJobHeader;
EXTERNAL struct	DATASET_HEADER	NetworkDatasetHeader;

/*====================== MAIN =====================================*/
/*
 | The main engine. Initialize all things, fire read requests, and then hiber.
 | All the work will be done in AST level.
 */
int
main(cc, vv)
char	*vv[];
int cc;
{
	char	line[512];		/* Long buffer for OPCOM messages */
	long	LOAD_DMF();		/* The loading routine of DMF framing
					   routine */

	/* Init the command mailbox, the timer process.
	 */
	LogFd = NULL; MustShutDown = 0;	/* =0 - Run. NonZero = Shut down */
	InformUsersCount = 0;
	LogLevel = 1;
	if (cc > 1) {	/* Read the log level as the first parameter */
		switch (**++vv) {
		  case '2': LogLevel = 2; break;
		  case '3': LogLevel = 3; break;
		  case '4': LogLevel = 4; break;
		  case '5': LogLevel = 5; break;
		  case '6': LogLevel = 6; break;
		  default: break;
		}
	}

#ifdef UNIX
	if (getuid() != 0) { /* NOT started as ROOT ! */
	  fprintf(stderr,"FUNET-NJE MUST BE STARTED WITH ROOT PRIVILEDGES!  Aborting!\n");
	  exit(EX_USAGE);
	}

# ifdef PID_FILE
	{
	  FILE *PidFil = fopen(PID_FILE,"r");
	  int pid = 0;
	  if (PidFil != NULL) {
	    fscanf(PidFil,"%d",&pid);
	    fclose(PidFil);
	  }
	  if (pid > 2  && kill(pid,0) == 0) {
	    /* There exists a process! */
	    fprintf(stderr,"*** THERE ALREADY EXISTS A (FUNET-NJE) PROCESS WITH REGISTERED PID!\n*** WON'T START UNTIL PID %d AS REGISTERED IN %s IS ELIMINATED\n",
		    pid,PID_FILE);
	    exit(EX_SOFTWARE);
	  }
	}
# endif

	/* Read our configuration from the configuration file */
	init_lines_data_base();		/* Clear it.
					   Next routine will fill it */
	if (read_configuration() == 0)
	  exit(EX_DATAERR);
	read_ebcasc_table(EBCDICTBL);

	rscsacct_open(RSCSACCT_LOG);

	/* We got all configuration, so we can now
	   spit error messages into our logfile.

	   This is the time to detach... */

	if (cc < 3)	/* Detach only if not other parameter after loglevel */
	  detach();
#  ifdef PID_FILE
	{
	  FILE *PidFil = fopen(PID_FILE,"w");
	  if( PidFil != NULL ) {
	    chmod(PID_FILE,0644);
	    fprintf(PidFil,"%d\n",(int)getpid());
	    fclose(PidFil);
	  }
	}
#  endif
#endif

	if (open_route_file() == 0) { /* Open the routing table */
	  send_opcom("FUNET-NJE aborting. Problems opening the route table!");
	  exit(1);
	}

#ifdef VMS
	init_crc_table();		/* For DECnet links */
#endif
	init_headers();			/* Init byte-swapped values in NJE
					   headers */
	init_variables();		/* Init some commonly used variables */
	init_command_mailbox();		/* To get operator's command and file
					   queueing */
	init_timer();			/* Our own timer mechanism.
					   Will tick each second	*/
#ifdef VMS
	if(*ClusterNode != '\0')	/* We run in cluster mode */
	  init_cluster_listener();
#endif

	/* Initialize the lines */
#ifdef VMS
	/*  This must be the last one, since it allocates non-paged pool.
	    Check whether we have a DMF at all. */
	for (i = 0; i < MAX_LINES; i++) {
	  if ((IoLines[i].state == ACTIVE) &&
	      (IoLines[i].type == DMF)) {
	    /*			DMF_routine_address = LOAD_DMF();	*/
	    /*************************** Temp *******************************/
	    if ((AddressFd = fopen(ADDRESS_FILE, "r")) == NULL) {
	      logger(1, "Can't open DMF address file.\n");
	      exit(1);
	    }
	    if ((fscanf(AddressFd, "%x", &DMF_routine_address)) != 1) {
	      logger(1, "Can't Fscanf address.\n"); exit(1);
	    }
	    fclose(AddressFd);
	    logger(1, "DMF routine address: %x\n", DMF_routine_address);
	    /****************************************************************/
	    break;		/* Loaded only once */
	  }
	}
#endif

	/* If we got up to here, almost all initialization was ok;
	   we must call it before initializing the lines, as from
	   there we start a mess, and this SEND_OPCOM seems to collide
	   with AST routines when calling Logger().			*/

	sprintf(line,
		"HUyNJE, Version-%s(%s)/%s started on node %s, logfile=%s\r\n",
		VERSION, SERIAL_NUMBER, version_time, LOCAL_NAME, LOG_FILE);

	strcat(line, "  Copyright (1988,1989,1990) - The Hebrew University of Jerusalem\r\n");
	strcat(line, "  Parts Copyright (1990,1991,1993) - Finnish University and Research Network\r\n      FUNET\r\n");
	strcat(line, LICENSED_TO);

	send_opcom(line);		/* Inform operator */

	init_communication_lines();	/* Setup the lines (open channels, etc)
					   and fire reads when needed. */


	/* Queue the auto-restart function */
	queue_timer(T_AUTO_RESTART_INTERVAL, -1, T_AUTO_RESTART);

	/* Start the statistics timer */
	queue_timer(T_STATS_INTERVAL, -1, T_STATS);

	/* Start the automatic link (VMNET) monitor */
	queue_timer(T_VMNET_INTERVAL, -1, T_VMNET_MONITOR);

#ifdef UNIX
#ifdef	BSD_SIGCHLDS
# ifdef	SIGCHLD
	signal(SIGCHLD,handle_childs);
# else
	signal(SIGCLD,handle_childs);
# endif
#else
	signal(SIGCLD,SIG_IGN);
#endif

	/* Do   kill -1 `cat /etc/huji.pid`	to raise its attention... */
	signal(SIGHUP,handle_sighup);	/* [mea] */
	/* Do   kill -USR1 `cat /etc/huji.pid`  to dump statistics counters */
	signal(SIGUSR1,handle_sigusr1);	/* [mea] */

	/* Do   kill `cat /etc/huji.pid`	to kill me -- SIGTERM	*/
	signal(SIGTERM,handle_sigterm);	/* [mea] */

	/* PIPE closed signals.. */
	signal(SIGPIPE,SIG_IGN);

#ifdef	SIGPOLL
	signal(SIGPOLL,SIG_IGN);
#endif

	/* ALARM -- programmed timeout */
	signal(SIGALRM,handle_sigalrm);

#if 0
	signal(SIGSEGV,handle_sigsegv);
#ifdef SIGBUS
       	signal(SIGBUS,handle_sigbus);
#endif
#endif

/* XX: Lets do things with link-activation time  'debug_rescan_queue()'
       calls, instead of this routine.. (Gerald Hanush)		*/
	init_files_queue();	/*	 Init the in-memory file's queue */

	while (MustShutDown <= 0) {
	  poll_sockets();
	  timer_ast();
	}
#endif
#ifdef VMS

	init_files_queue();		/* Init the in-memory file's queue */

	while (MustShutDown <= 0) {
	  sys$hiber(); 		/* Sleep. All work will be done in AST mode */
	  if (MustShutDown <= 0)
	    logger(1, "MAIN, False wakeup\n");
	}
#endif

/* If we got here - Shutdown the daemon (Close permanently opened files). */
	close_command_mailbox();
	close_route_file();

	logger(0,"=======================\n");
	logger(0,"Shuting down, counters:\n");
	logger(0,"=======================\n");
	log_line_stats();

	send_opcom("FUNET-NJE, normal shutdown");

#ifdef	UNIX
	unlink(PID_FILE); /* Successfull shutdown will kill the PID_FILE */
	if (strchr(COMMAND_MAILBOX,'/'))
	  unlink(COMMAND_MAILBOX);
#endif

	exit(0);
}


/*
 | Called after a line has signedoff. Checks whether this is a result of a
 | request to shutdown after all lines has signed off (MustShutDown = -1).
 | If so, scan all lines. If none is active we can shut: change MustShutDown
 | to 1 (Imediate shut) and wakeup the main routine to do the real shutdown.
 */
void
can_shut_down()
{
	register int	i;

	if (MustShutDown == 0) return;	/* No need to shut down */

	for (i = 0; i < MAX_LINES; i++)
	  if (IoLines[i].state == ACTIVE)	/* Some line is still active */
	    return;				/* Can't shutdown yet */

	/* All lines are closed. Signal it */
	MustShutDown = 1;	/* Immediate shutdown */
#ifdef VMS
	sys$wake(0,0);		/* Wakeup the main routine */
#endif
}

/*======================== INIT-LINES-DB ===============================*/
/*
 | Clear the line database. Init the initial values.
 */
static void
init_lines_data_base()
{
	int	i, j;

	for (i = 0; i < ABSMAX_LINES; i++) {

	  IoLines[i].flags		= 0;
	  IoLines[i].socket		= -1;
	  IoLines[i].socketpending	= -1;
	  IoLines[i].QueueStart		= NULL;
	  IoLines[i].QueueEnd		= NULL;
	  IoLines[i].MessageQstart	= NULL;
	  IoLines[i].MessageQend	= NULL;
	  IoLines[i].TotalErrors	= 0;
	  IoLines[i].errors		= 0;
	  IoLines[i].state		= INACTIVE;	/* They are all dead */
	  IoLines[i].InBCB		= 0;
	  IoLines[i].OutBCB		= 0;
	  IoLines[i].TimeOut		= 3;	/* 3 seconds		*/
	  IoLines[i].HostName[0]	= 0;
	  IoLines[i].MaxXmitSize	= 0;
	  IoLines[i].PMaxXmitSize	= MAX_BUF_SIZE;
	  IoLines[i].XmitSize		= 0;
	  IoLines[i].QueuedFiles	= 0;	/* No files queued	*/
	  IoLines[i].QueuedFilesWaiting	= 0;	/* No files queued	*/
#ifdef	USE_XMIT_QUEUE
	  IoLines[i].FirstXmitEntry	= 0;
	  IoLines[i].LastXmitEntry	= 0;
#endif
	  IoLines[i].ActiveStreams	= 0;
	  IoLines[i].CurrentStream	= 0;
	  IoLines[i].FreeStreams	= 1;	/* Default of 1 stream	*/
	  for (j = 0; j < MAX_STREAMS; ++j) {
	    IoLines[i].InStreamState[j]		= S_INACTIVE;
	    IoLines[i].OutStreamState[j]	= S_INACTIVE;
	    IoLines[i].SizeSavedJobHeader[j]	= 0;
	    IoLines[i].SizeSavedDatasetHeader[j] = 0;
	    IoLines[i].SizeSavedJobTrailer[j]	= 0;
	  }
	  IoLines[i].TcpState		= 0;
	  IoLines[i].TcpXmitSize	= 0;
	  IoLines[i].stats.TotalIn	= 0;
	  IoLines[i].stats.TotalOut	= 0;
	  IoLines[i].stats.WaitIn	= 0;
	  IoLines[i].stats.WaitOut	= 0;
	  IoLines[i].stats.AckIn	= 0;
	  IoLines[i].stats.AckOut	= 0;
	  IoLines[i].stats.RetriesIn	= 0;
	  IoLines[i].stats.RetriesOut	= 0;
	  IoLines[i].stats.MessagesIn	= 0;
	  IoLines[i].stats.MessagesOut	= 0;
	  IoLines[i].sumstats.TotalIn	= 0;
	  IoLines[i].sumstats.TotalOut	= 0;
	  IoLines[i].sumstats.WaitIn	= 0;
	  IoLines[i].sumstats.WaitOut	= 0;
	  IoLines[i].sumstats.AckIn	= 0;
	  IoLines[i].sumstats.AckOut	= 0;
	  IoLines[i].sumstats.RetriesIn	= 0;
	  IoLines[i].sumstats.RetriesOut  = 0;
	  IoLines[i].sumstats.MessagesIn  = 0;
	  IoLines[i].sumstats.MessagesOut = 0;
	  IoLines[i].WrFiles = 0;
	  IoLines[i].WrBytes = 0;
	  IoLines[i].RdFiles = 0;
	  IoLines[i].RdBytes = 0;
	}
}

/* ========================== BUG_CHECK() ============================ */

/*
 | Log the string, and then abort.
 */
volatile void
bug_check(string)
const char	*string;
{
	int i;
	char line[80];
	char From[20];


	logger(1, "Bug check: %s\n", string);
	strcpy(line,"FUNET-NJE: Aborting due to bug-check.");
	send_opcom(line);
	sprintf(From,"@%s", LOCAL_NAME);

	for (i = 0; i < InformUsersCount; ++i)
	  send_nmr(From, InformUsers[i], line, strlen(line), ASCII, CMD_MSG);

	close_command_mailbox();
	close_route_file();

#ifdef	UNIX
	unlink(PID_FILE); /* Successfull shutdown will kill the PID_FILE */
	if (strchr(COMMAND_MAILBOX,'/'))
	  unlink(COMMAND_MAILBOX);
#endif
	abort();
}


/*=========================== INIT ==============================*/
/*
 | Init some commonly used variables.
 */
static void
init_variables()
{

	/* Create our name in EBCDIC */
	E_BITnet_name_length = strlen(LOCAL_NAME);
	ASCII_TO_EBCDIC(LOCAL_NAME, E_BITnet_name, E_BITnet_name_length);
	PAD_BLANKS(E_BITnet_name, E_BITnet_name_length, 8);
	E_BITnet_name_length = 8;

	/* Init static job header fields */
	memcpy(NetworkJobHeader.NJHGACCT, EightSpaces, 8); /* Blank ACCT */
	memcpy(NetworkJobHeader.NJHGPASS, EightSpaces, 8);
	memcpy(NetworkJobHeader.NJHGNPAS, EightSpaces, 8);
	memcpy(NetworkJobHeader.NJHGXEQU, EightSpaces, 8);
	memcpy(NetworkJobHeader.NJHGFORM, EightSpaces, 8);
	PAD_BLANKS(NetworkJobHeader.NJHGPRGN, 0, 20);
	memcpy(NetworkJobHeader.NJHGROOM, EightSpaces, 8);
	memcpy(NetworkJobHeader.NJHGDEPT, EightSpaces, 8);
	memcpy(NetworkJobHeader.NJHGBLDG, EightSpaces, 8);
	/* Same for dataset header */
	memcpy(NetworkDatasetHeader.NDH.NDHGDD, EightSpaces, 8);
	memcpy(NetworkDatasetHeader.NDH.NDHGFORM, EightSpaces, 8);
	memcpy(NetworkDatasetHeader.NDH.NDHGFCB, EightSpaces, 8);
	memcpy(NetworkDatasetHeader.NDH.NDHGUCS, EightSpaces, 8);
	memcpy(NetworkDatasetHeader.NDH.NDHGXWTR, EightSpaces, 8);
	memcpy(NetworkDatasetHeader.NDH.NDHGPMDE, EightSpaces, 8);
	memcpy(NetworkDatasetHeader.RSCS.NDHVDIST, EightSpaces, 8);
	PAD_BLANKS(NetworkDatasetHeader.RSCS.NDHVTAGR, 0, 136);
}


/* ---------------------------------------------------------------- */
/*   SIGSEGV, and SIGBUS are LOGGED with these.. */

static void
handle_sigsegv(n)
int n;
{
	signal(SIGSEGV,SIG_DFL);
	logger(1,"Got SIGSEGV!\n");
	abort();
}

#ifdef SIGBUS
static void
handle_sigbus(n)
int n;
{
	signal(SIGBUS,SIG_DFL);
	logger(1,"Got SIGBUS!\n");
	abort();
}
#endif
