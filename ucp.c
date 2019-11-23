/* UCP.C (formerly CP.C)     V1.8-mea
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
 | Control program for the RSCS daemon. It assigns a channel to its mailbox
 | and send a command message to it.

 | Altered outlook, more features, and options..
 | Copyright (c) 1991, 1993, 1994,  Finnish University and Research Network,
 | FUNET.
 
 */

#include "prototypes.h"
#include "clientutils.h"

#define LINESIZE 256

#define	Exit	1
#define	Show	2
#define	Shut	3
#define	Start	4
#define	Help	5
#define	Queue	6
#define	Stop	7
#define	Force	8
#define	Debug	9
#define	Loglevel 10
#define	Route	11
#define	Gone	12
#define	Ungone	13
#define Rescan	14

struct Commands {
	char	*command;
	int	code;
	} commands[] = {
	  { "EXIT", Exit },
	  { "SHOW", Show },
	  { "SHUT", Shut },
	  { "START", Start },
	  { "HELP", Help },
	  { "QUEUE", Queue },
	  { "STOP", Stop },
	  { "FORCE", Force },
	  { "DEBUG", Debug },
	  { "LOGLEVEL", Loglevel },
	  { "ROUTE", Route },
	  { "GONE", Gone },
	  { "UNGONE", Ungone },
	  { "RESCAN", Rescan },
	  { "", 0 }
	};

extern void process_cmd();
extern void show_help();
extern int parse_command();
extern void command();


char LOCAL_NAME   [10];
char BITNET_QUEUE [80];
char LOG_FILE     [80] = "-";

int LogLevel = 1;
FILE *LogFd = NULL;

int
main(cc, vv)
int	cc;
char	**vv;
{
	char	line[LINESIZE];
	int	i;

	read_configuration();

	if (cc == 1) {
	  while (!feof(stdin) && !ferror(stdin)) {
	    printf("NJE-CP> ");
	    if (fgets(line, sizeof line, stdin) == NULL)
	      exit(0);
	    process_cmd(line);
	  }
	} else {
	  *line = '\0';
	  for (i = 1; i < cc; i++) {
	    strcat(line, vv[i]);
	    strcat(line, " ");
	  }
	  process_cmd(line);
	}
	exit(0);
}

void
process_cmd(line)
char	*line;
{
	char	*p, KeyWord[80], param1[80], param2[80], param3[80];
	char	TempStr[160];
	int	i, NumParams, cmd;

	if ((p = strchr(line, '\n')) != NULL) *p = '\0';
	if (*line == '\0') return;	/* Null command */
	*param1 = 0;
	*param2 = 0;
	*param3 = 0;
	NumParams = sscanf(line, "%s %s %s %s", KeyWord, param1,
		param2, param3);
	switch ((cmd = parse_command(KeyWord))) {
	  case Help:
	      show_help();
	      break;
	  case Exit:
	      exit(0);	/* Exit program */
	  case Show:
	      if (NumParams != 2) {
		printf("Illegal SHOW command\n");
		break;
	      }
	      if (strncasecmp(param1, "LINE", 4) == 0)
		command(CMD_SHOW_LINES);
	      else
		if (strncasecmp(param1, "QUEUE", 5) == 0)
		  command(CMD_SHOW_QUEUE);
		else
		  printf("Illegal SHOW command\n");
	      break;
	  case Start:
	      if (NumParams != 3) {
		printf("Illegal START command\n");
		break;
	      }
	      sscanf(param2, "%d", &i);
	      if (strncasecmp(param1, "LINE", 4) == 0)
		command(CMD_START_LINE, i);
	      else
		printf("Illegal START command\n");
	      break;
	  case Force:
	  case Stop:
	      if (NumParams != 3) {
		printf("Illegal STOP/FORCE command\n");
		break;
	      }
	      if (strncasecmp(param1, "LINE", 4) != 0) {
		printf("Illegal STOP command\n");
		break;
	      }
	      if (sscanf(param2, "%d", &i) != 1) {
		printf("Illegal line number\n");
		break;
	      }
	      if (cmd == Force)
		command(CMD_FORCE_LINE, i);
	      else
		command(CMD_STOP_LINE, i);
	      break;
	  case Gone:
	  case Ungone:
	      if (cmd == Gone) {
		if (NumParams != 3) {
		  printf("Illegal GONE command\n");
		  break;
		}
		sprintf(line, "%s %s", param1, param2);
		command(CMD_GONE_ADD_UCP, 0, line);
	      } else {
		if (NumParams != 2) {
		  printf("Illegal GONE command\n");
		  break;
		}
		command(CMD_GONE_DEL_UCP, 0, param1);
	      }
	      break;
	  case Queue:
	      if (NumParams < 2) {
		printf("No filename given\n");
		break;
	      }
	      if ((NumParams == 4) &&
		 (strncasecmp(param2, "SIZE", 4) == 0))
		sscanf(param3, "%d", &i);
	      else
		i = 0;
	      command(CMD_QUEUE_FILE, 0, param1, i);
	      break;
	  case Shut:
	      if (NumParams == 1)	/* Shutdown normally */
		command(CMD_SHUTDOWN);
	      else
		if (strncasecmp(param1, "ABORT", 5) == 0)
		  command(CMD_SHUTDOWN_ABRT);
		else
		  printf("Illegal SHUT command\n");
	      break;
	  case Debug:
	      if (NumParams != 2 && NumParams != 3) {
		printf("DEBUG must get a parameter\n");
		break;
	      }
	      if (strncasecmp(param1, "DUMP", 4) == 0)
		command(CMD_DEBUG_DUMP);
	      else
		if (strncasecmp(param1, "RESCAN", 6) == 0)
		  command(CMD_DEBUG_RESCAN,0,param2,0);
		else
		  printf("Illegal DEBUG command\n");
	      break;
	  case Rescan:
	      if (NumParams < 2) {
		printf("  RESCAN QUEUE, or RESCAN EXITS ?\n");
		break;
	      }
	      if (strcasecmp(param1,"QUEUE")==0) {
		if (*param2 == 0) *param2 = '+';
		command(CMD_DEBUG_RESCAN,0,param2,0);
	      } else if (strcasecmp(param1,"EXITS")==0) {
		command(CMD_EXIT_RESCAN,0,0,0);
	      } else if (strcasecmp(param1,"ROUTE")==0) {
		command(CMD_ROUTE_RESCAN,0,0,0);
	      } else {
		printf("  RESCAN EXITS, RESCAN QUEUE, or RESCAN ROUTE ?\n");
	      }
	      break;
	  case Loglevel:
	      if (NumParams != 2) {
		printf("No log level given\n");
		break;
	      }
	      sscanf(param1, "%d", &i);
	      command(CMD_LOGLEVEL, i);
	      break;
	  case Route:
	      if (NumParams != 4) {
		printf("Illegal format: Try: ROUTE xx TO yy\n");
		break;
	      }
	      if (strncasecmp(param2, "TO", 2) != 0) {
		printf("Illegal format: Try: ROUTE xx TO yy\n");
		break;
	      }
	      sprintf(TempStr, "%s %s", param1, param3);
	      command(CMD_CHANGE_ROUTE, 0, TempStr, 0);
	      break;
	  default:
	      printf("Illegal command '%s'. Type HELP\n", line);
	      break;
	}
}

void
show_help()
{
	printf("   HELP  - Show this message\n");
	printf("   SHOW LINE/QUEUE - Show lines or queue status\n");
	printf("   START LINE n - Start a closed line\n");
	printf(" * START STREAM n LINE m - Start specific stream in active line\n");
	printf("   SHUT [ABORT] - Shutdown or abort the whole program\n");
	printf("   STOP LINE - Stop a line\n");
	printf("   FORCE LINE - Stop a line immediately\n");
	printf(" * STOP STREAM n LINE m - Stop a single stream in active line\n");
	printf(" * FORCE STREAM n LINE m - Stop immediately\n");
	printf("   QUEUE file-name [SIZE size] - To queue a file to send\n");
	printf("   RESCAN EXITS - Rescan file and message exits.\n");
	printf("   RESCAN QUEUE - Rescan queue and requeue files.\n");
	printf("   RESCAN ROUTE - Reopen route database.\n");
	printf("   DEBUG DUMP - Dump all lines buffers to logfile\n");
	printf(" %% DEBUG RESCAN - Rescan queue and requeue files.\n");
	printf("   LOGLEVEL n - Set the loglevel to N\n");
	printf("   ROUTE xxx TO yyy - Change the routing table.\n");
	printf("        Route to  OFF  will delete the route entry.\n");
	printf("   GONE username LoginDirectory - Add username to gone list\n");
	printf("   UNGONE username - Remove a user from the Gone list.\n");
	printf("* - Not yet implemented,  %% - obsoleted\n");
}


/*
 | Send the command. The command is in CMD, a numeric value (if needed) is in
 | VALUE, and a string value (if needed) is in STRING.
 */
void
command(cmd, value, string, param)
int	cmd, value, param;
char	*string;
{
#ifdef VMS
	struct	DESC	MailBoxDesc;
#endif
	char	line[LINESIZE];
	long	size;

	*line = cmd;
	switch (cmd) {
	case CMD_QUEUE_FILE :
			/* File size: */
		line[1] = (unsigned char)((param & 0xff00) >> 8);
		line[2] = (unsigned char)(param & 0xff);
		strcpy(&line[3], string);
		size = strlen(&line[3]) + 3;
		break;
	case CMD_CHANGE_ROUTE:
		strcpy(&line[1], string);
		size = strlen(&line[1]) + 1;
		break;
	case CMD_START_LINE:
	case CMD_STOP_LINE:
	case CMD_FORCE_LINE:
	case CMD_LOGLEVEL:
		line[1] = (value & 0xff); line[2] = '\0';
		size = 2;
		break;
	case CMD_GONE_ADD_UCP:
	case CMD_GONE_DEL_UCP:
		strcpy(&line[1], string);
		size = strlen(string) + 1;
		break;
	case CMD_DEBUG_RESCAN:
		mcuserid(&line[1]);	/* Add the username of sender */
		line[11] = *string;
		size = 13;
		break;
	default:
		mcuserid(&line[1]);	/* Add the username of sender */
		size = strlen(&line[1]) + 1;	/* 1 for the command code */
		break;
	}

	send_cmd_msg(line, size, 0); /* Must be ONLINE! */
}



int
parse_command(line)
char	*line;
{
	int	i;

	for (i = 0; ; i++) {		/* Look for the command */
	  if (commands[i].code == 0) /* End of valid commands */
	    return 0;		/* Signal error */
	  if (strncasecmp(commands[i].command, line,
			  strlen(line)) == 0)
	    return commands[i].code;
	}
}
