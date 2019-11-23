/* YGONE.C	V1.3
 |
 | It has two parts which are different in functionality:
 | YGONE/CONTINUE: Will trap all messages sent to the terminal into a file.
 | This is a general mechanism and the NJE emulator is not concerned.
 | The messages are appended to SYS$LOGIN:YGONE_MESSAGES.TXT, and the
 | terminal is left logged in but unuseable for any other thing, until
 | this program is aborted.  When aborting the terminal is set to
 | Broadcast, disregarding its previous setup.
 |
 | YGONE - Inform the NJE emulator that you want to be added to the Gone list.
 | YGONE/DISABLE - Remove yourself from the gone list.
 |
 | V1.1 - 25/3/90 - Add support for Unix.
 | V1.2 - 28/3/90 - Account for long lines, thus not overflowing one screen
 |	  when long lines are displayed.
 | V1.3 - 15/10/90 - When called with /DISABLE call Mail_check() to inform
 |        about new mails.
 | V2.0 - 1/10/93 - Ported to FUNET version  K000165@ALIJKU11, and [mea]
 */
#include "consts.h"
#include "prototypes.h"
#include "clientutils.h"

/* Hold the terminal characteristics in order to change and retore them: */
struct	terminal_char {	/* Terminal characteristics */
		unsigned char	class, type;
		unsigned short	PageWidth;
		long	BasicChars;
		long	ExtChars;
	} TerChar;
short	TerminalMailboxChan, TerminalChan;
int	MessagesCount = 0;	/* So we can inform user how many new messages */

char LOCAL_NAME   [10];
char BITNET_QUEUE [80];
char LOG_FILE     [80] = "-";	/* STDERR as the default */

int LogLevel = 1;
FILE *LogFd =NULL;

#ifdef	UNIX
#define MESSAGE_FILE ".ygone.messages"
#else /* must be VMS.. */
#define MESSAGE_FILE "YGONE_MESSAGES.TXT"
#endif


/* The file descriptor to write messages on */
FILE	*MessageFd;
/*********************** NJE-GONE section *****************************/
/*
 | Get the user's login directory, create a command line which contains it and
 | the username and starts with the command code CMD_GONE_ADD, and then send
 | it to the NJE emulator.
 */
void add_gone()
{
	char	*LoginDirectory;
	char	UserName[128], CommandLine[128];
	struct	passwd	*UserEntry, *getpwnam();

	mcuserid(UserName);
	if ((UserEntry = getpwnam(UserName)) == NULL) {
	  perror("Getpwname"); exit(1);
	}
	LoginDirectory = UserEntry->pw_dir;

	*CommandLine = CMD_GONE_ADD;
	mcuserid(&CommandLine[1]);	/* The username */
	strcat(CommandLine, " ");	/* A space to separate them */
	strcat(CommandLine, LoginDirectory);	/* And the login directory */
	strcat(CommandLine, "/");
	/* And send it to emulator */
	send_cmd_msg(CommandLine, strlen(&CommandLine[1]) + 1, 0); /* Must be ONLINE! */

}

/*
 | Inform the NJE emulator to remove the user from the gone list.
 */
void remove_gone()
{
	char	CommandLine[128];

	*CommandLine = CMD_GONE_DEL;
	mcuserid(&CommandLine[1]);	/* The username */
	/* And send it to emulator */
	send_cmd_msg(CommandLine, strlen(&CommandLine[1]) + 1, 0); /* Must be ONLINE! */
}


void logout_process()
{
	printf("Logout now!!!!\n");
}

/*
 | Display the file on the screen, one screen at a time. Delete it after done.
 */
void display_file()
{
	int	counter;
	char	line[256];
	char	FileName[128], UserName[128], *LoginDirectory;
	struct	passwd	*UserEntry, *getpwnam();
	FILE	*fd;

	mcuserid(UserName);
	if ((UserEntry = getpwnam(UserName)) == NULL) {
	  perror("Getpwname"); exit(1);
	}
	LoginDirectory = UserEntry->pw_dir;

	sprintf(FileName, "%s/%s", LoginDirectory, MESSAGE_FILE);

	if((fd = fopen(FileName, "r")) == NULL)
		return;		/* New new messages or user not subscribed */
	counter = 0;

	if (fgets(line, sizeof line, fd) == NULL) { /* Empty file */

	  printf("No new messages have arrived!\n");

	} else {

	  printf("New messages have arrived:\n");
	  printf("-------------------------\n");

	  for(;;) {
	    printf("%s", line);
	    counter += (strlen(line) / 80); /* Add for lines longer than 80 */
	    if(counter++ > 20) {
	      printf("<CR> to continue, Anything else to abort: ");
	      fgets(line, sizeof line, stdin);
	      if((*line != '\n') && (*line != '\0')) break;
	      counter = 0;
	      printf("\033[2J\033[H"); /* Clear screen */
	    }
	    if(fgets(line, sizeof line, fd) == NULL) break; /* End of file */
	  }

	}

	fclose(fd);
	unlink(FileName);
}

void
usage()
{
  printf("\
YGONE: Inform NJE transporter about your intention to disappear\n\
       Options:   <none>       mark up yourself as gone..\n\
                  -d           disable to gone -- come back\n");
  printf("\
       Messages arrived while you were gone will be logged into your\n\
       login directory file: %s, and displayed when you\n\
       return and issue command:  ygone -d\n",MESSAGE_FILE);
  exit(1);
}

/*
 | If there are no parameters - inform NJE and logout. If there is a parameter
 | named /CONTINUE - only trap terminal messages and do not logout; this is
 | general and have no special connection to the NJE emulator.
 */
void main(cc, vv)
char	**vv;
int	cc;
{
	read_configuration();
	if(cc == 1) {	/* No parameters */
	  add_gone();	/* Inform emulator about it. */
	  printf("Your messages will be recorded while you are away.\n");
	  logout_process();	/* Send him home */
	  exit(0);
	}
	if (strcmp(vv[1], "-d") == 0) {	/* YGONE/DISABLE */
	  remove_gone();
	  display_file();
	  exit(0);
	}
	usage();
}
