/* SEND.C	V1.3-mea
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
 | Send a command or message to the NJE emulator to send it over to the
 | correct line.
 | The call to this module is either by SEND/COMMAND or SEND/MESSAGE. If the
 | user gives the message's text, then we send it and exit. If not, we
 | inquire it interactively.
 | Since the command given is parsed by the shell as arguments, we have to
 | collect them back as one text line.
 | One line usage: send /command node-name command
 |            or:  send /message User@Node message...
 | Please note the space between SEND and the qualifier.
 |
 */

#include "prototypes.h"
#include "clientutils.h"

#define LINESIZE 512
#define MAXTEXTSIZE 103 /* Should be able to use at least 103 chars.. */
#define CMD_MSG 0
#define CMD_CMD 1

void
usage(explanation)
char *explanation;
{
	if (!explanation) {
	  fprintf(stderr,"Usage: send [-u fromuser|-s] [-c{ommand}] [@]node [command string]\n");
	  fprintf(stderr,"       send [-u fromuser|-s] [-m{essage}] user@node [message text]\n");
	} else {
	  fprintf(stderr,"SEND: %s\n",explanation);
	}
	exit(9);
}


char LOCAL_NAME   [10];
char BITNET_QUEUE [80];
char LOG_FILE     [80] = "-";	/* STDERR as the default */
int silent_wordwrap = 0;

int LogLevel = 1;
FILE *LogFd = NULL;

extern int send_nje();

int
main(cc, vv)
char	**vv;
int	cc;
{
	char	text[LINESIZE];
	char	address[32];
	char	from[32];
	char	*p;
	int	type = 0,	/* Message or command */
		mode = 0;	/* Interactive or not */
	struct stat stdstats;
	int	noprompt = 0;


	setbuf (stdout,NULL);	/* No buffering on stdout,
				   no need to flush... */
	fstat(fileno(stdin),&stdstats); /* This MUST succeed! */
	if (!S_ISCHR(stdstats.st_mode))
	  noprompt = 1;

	read_configuration();
	read_etable();
	*from = 0;

	/* Get the command line and command parameters.*/
	if (cc < 2) {	/* No /Mode or no address.. - tell user and exit */
	  usage(NULL);
	  exit(1);
	}

/* Get the switch */
	++vv;		/* Point to the qualifier */
	if (strcasecmp(*vv,"-u") == 0) {
	  ++vv;
	  cc -= 2;
	  if (*vv) {
	    strncpy(from,*vv,sizeof(from)-1);
	    from[sizeof(from)-1] = 0;
	    if ((p = strchr(from,'@')) != NULL) {
	      if ((p - from) > 8 || strlen(p+1) > 8) {
	    fromaddrerror:
		usage("Defined sender address (-u) components may not exceed 8 chars in size");
	      }
	    } else {
	      if (strlen(from) > 8)
		goto fromaddrerror;
	    }
	  } else {
	    usage("-u option requires one parameter!");
	  }
	  ++vv;
	}
	if (strcasecmp(*vv,"-s") == 0) {
	  ++vv;
	  --cc;
	  sprintf(from,"@%s",LOCAL_NAME);
	  if (getuid() >= LuserUidLevel) {
	    type = -1;
	    usage("-s option requires priviledges!");
	  }
	  type = CMD_MSG;
	} else if ((strcasecmp(*vv, "/message") == 0) ||
		   (strcasecmp(*vv, "-message") == 0) ||
		   (strcasecmp(*vv, "-m") == 0)) {
	  type = CMD_MSG;
	  --cc;
	  ++vv;
	} else if((strcasecmp(*vv, "/command") == 0) || 
		  (strcasecmp(*vv, "-command") == 0) ||
		  (strcasecmp(*vv, "-c") == 0)) {
	  type = CMD_CMD;
	  --cc;
	  ++vv;
	} else if (**vv == '/' || **vv == '-') {
	  usage("Valid qualifiers are `-c{ommand}' and `-m{essage}' only");
	} else
	  type = -1;	/* No qualified defined.. */

/* Get the address (if exists) */
	if (cc < 1) {	/* No parameters - prompt for address and enter interactive mode */
	  while (1) {
	    if (type == CMD_CMD)
	      printf("_Host_Address: ");
	    else if (type == CMD_MSG)
	      printf("_User@Host_Address: ");
	    else
	      printf("_Address: ");
	    if (fgets(address, sizeof address, stdin)== NULL) /* EOF */
	      exit(0);
	    address[sizeof(address)-1] = 0; /* Make sure it fills only so much.. */
	    if ((p = strchr(address, '\n')) != NULL) *p = '\0';
	    mode = 1;		/* Interactive mode */
	    if (*address == 0) continue;	/* Blank address.. */
	    if (*address == '@') {
	      strcpy(address,address+1);
	      if (strlen(address) > 8) {
	    addresserror:
		usage("target address may not have components longer than 8 chars");
	      }
	      type = CMD_CMD;
	      break;
	    } else if ((p = strchr(address,'@')) != NULL) {
	      if ((p - address) > 8 || strlen(p+1) > 8)
		goto addresserror;
	      type = CMD_MSG;
	      break;
	    } else if (type == -1) { /* Not defined! */
	      if (strlen(vv[-1]) > 8)
		goto addresserror;
	      type = CMD_MSG;
	      break;
	    }
	    break;
	  }
	} else {		/* Get the available parameters */

	  if (!*vv)
	    usage("Mandatory parameter (target address) is missing!");

	  strncpy(address, *vv, sizeof(address)); /* We have at least
						     the address there */
	  address[sizeof(address)-1] = 0;	  /* Make sure it fills
						     only so much.. */
	  ++vv;
	  --cc;

	  if (*address == '@') {
	    strcpy(address,address+1);
	    if (strlen(address) > 8)
	      goto addresserror;
	    type = CMD_CMD;
	  } else if ((p = strchr(address,'@')) != NULL) {
	    if ((p - address) > 8 || strlen(p+1) > 8)
	      goto addresserror;
	    type = CMD_MSG;
	  } else if (type == -1) {
	    if (strlen(vv[-1]) > 8)
	      goto addresserror;
	    sprintf(address,"%.8s@%.8s",vv[-1],LOCAL_NAME);
	    type = CMD_MSG;
	  }

	  if (cc <= 1) {	/* Nothing more than it		*/
	    mode = 1;		/* Interactive mode		*/
	  } else {		/* Reconstruct the parameters as the text */
	    char *tout = text;
	    int  charspace = sizeof(text)-2;
	    *text = '\0';
	    while (cc > 0 && *vv) {
	      char *s = *vv;
	      while (*s && charspace > 0) {
		*tout++ = *s++;
		--charspace;
	      }
	      if (vv[1] != NULL && charspace > 0)  /* More to come ? */
		*tout++ = ' ';
	      *tout = 0;
	      ++vv; --cc;
	    }
	  }
	}

	/* Create the sender's address */
	if (getuid() >= LuserUidLevel && *from ) {
	  fprintf(stderr,"SEND: -u option not allowed, insufficient priviledges.\nForcing uname to your login id.   Your uid = %d\n",(int)getuid());
	  *from = 0;
	}
	if (*from == 0)
	  mcuserid(from);

	if (strchr(address,'.') != NULL)
	  usage("target address may not contain '.' chars in it!");
	if (strchr(from,'.') != NULL)
	  usage("source address may not contain '.' chars in it!");

	if (mode == 0) {		/* Batch mode */
	  send_nje(type, from, address, text);	/* Send it to the daemon */
	  exit(0);
	}

/* We have to read it interactively. Loop until blank line */
	if (!noprompt)
	  fprintf(stderr,
		  "Hit your message/command. End with EOF (usually Ctrl-D)\n");
	while (!feof(stdin) && !ferror(stdin)) {	/* [mea] */
	  if (!noprompt)
	    printf("%s: ", address);
	  *text = 0;
	  if (fgets(text, sizeof(text)-1, stdin) == NULL)
	    break;		/* EOF */
	  text[sizeof(text)-1] = 0;
	  if ((p = (char*)strchr(text, '\n')) != NULL) *p = '\0';
	  send_nje(type, from, address, text);
	}
	exit(0);
}


int
send_nje(type, from, address, text)
int	type;
char	*from, *address, *text;
{
	char	line[140], *p;
	int	size;
	int	textlen, addrlen;

	/* Remove all controls */
	for (p = text; *p != '\0'; p++)
	  if (((unsigned char)*p) < ' ')
	    *p = ' ';

	if (type == CMD_CMD) {
	  /* Uppercase the message's text */
	  upperstr(text);
	  *line = CMD_SEND_COMMAND;
	}
	else
	  *line = CMD_SEND_MESSAGE;

	if (type == CMD_CMD)	/* Add the @ to address */
	  sprintf(&line[2], "%s @%s ", from, address);
	else			/* Address already has @ */
	  sprintf(&line[2], "%s %s ", from, address);

	addrlen = strlen(line+2);

	/* Upper-case the addresses.
	   If this is a command - uppercase it also */
	upperstr(line+2);
	if (type == CMD_CMD)
	  upperstr(text);

	textlen = strlen(text);

	line[1] = addrlen + 2;		/* Where the text begins */
	if (textlen <= MAXTEXTSIZE) {
	  strcpy(line+2+addrlen, text);

	  size = 3+addrlen+textlen;	/* 3 for the command code,
					   text start offset,
					   and string end NULL */

	  return send_cmd_msg(line, size, 0); /* Must be ONLINE! */

	} else {
	  /* Ok, so the text length is more than the allowed maximum
	     (103 chars), do wraps et.al. */
	  char *t = text;

	  /* XX: Complain about long input ?? */

	  while (textlen > 0) {
	    int partlength = textlen > MAXTEXTSIZE  ? MAXTEXTSIZE : textlen;
	    int rc;

	    strncpy(line+2+addrlen, t, partlength);
	    size = 3+addrlen+partlength;
	    line[size-1] = 0; /* Add terminating NULL */

	    rc = send_cmd_msg(line, size, 0); /* Must be ONLINE! */
	    if (rc) return rc;  /* Return immediately,
				   if sending was not ok.. */

	    t += partlength;
	    textlen -= partlength;
	  }
	  return 0;
	}
}
