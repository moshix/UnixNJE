/*	FUNET-NJE		Client utilities
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 26-Sep-1993
 */

#include "consts.h"
#include "prototypes.h"
#include "clientutils.h"

#include <sysexits.h>
#include <ctype.h>

/* read_configuration() -- reads configuration info */

#ifndef DEF_SPOOL_DIR_RULE
# ifdef	UNIX
#  define DEF_SPOOL_DIR_RULE "/usr/spool/bitspool/"
# else
#  define DEF_SPOOL_DIR_RULE ???
# endif
#endif

#ifndef DEF_POSTMAST_SPOOL_DIR
# ifdef UNIX
#  define DEF_POSTMAST_SPOOL_DIR "/usr/spool/bitspool/POSTMAST"
# else
#  define DEF_POSTMAST_SPOOL_DIR ???
# endif
#endif

char COMMAND_MAILBOX[80] = "";
char DefaultSpoolDir[256]        = DEF_SPOOL_DIR_RULE;
char DefaultPOSTMASTdir[256]     = DEF_POSTMAST_SPOOL_DIR;
char DefaultEBCDICTBL[256]       = "";
char DefaultForm[9]		 = "STANDARD";
int LuserUidLevel = 1;	/* Only root can fake sender... */
char USER_EXITS[256]	= "";	/* File of user exits		*/


int
read_configuration()
{
	FILE	*fd;
	char	line[LINESIZE], KeyWord[80], param1[80],
		param2[80], param3[80];

	if ((fd = fopen(CONFIG_FILE, "r")) == NULL) {
	  fprintf(stderr,"Can't open configuration file named '%s'\n",
		  CONFIG_FILE);
		return 0;
	}

	while (fgets(line, sizeof line, fd) != NULL) {
	  if (*line == '*') continue; /* Comment */
	  if (*line == '#') continue; /* Comment */
	  if (*line == '\n') continue; /* Blank line */
	  if (*line == 0) continue; /* Blank line */

	  *KeyWord = 0; *param1 = 0; *param2 = 0; *param3 = 0;
	  sscanf(line, "%s %s %s %s", KeyWord, param1, param2, param3);

	  if (strcasecmp(KeyWord, "USEREXITS") == 0) {
	    strcpy(USER_EXITS, param1);
	  } else if (strcasecmp(KeyWord, "QUEUE") == 0) {
	    strcpy(BITNET_QUEUE, param1);
	  } else if (strcasecmp(KeyWord, "CMDMAILBOX") == 0) {
	    strcpy(COMMAND_MAILBOX, param1);
	    strcat(COMMAND_MAILBOX, " ");
	    strcat(COMMAND_MAILBOX, param2);
	    if (param3[0] != 0) {
	      strcat(COMMAND_MAILBOX, " ");
	      strcat(COMMAND_MAILBOX, param3);
	    }
	  } else if (strcasecmp(KeyWord, "NAME") == 0) {
	    strcpy(LOCAL_NAME, param1);
	  } else if (strcasecmp(KeyWord, "EBCDICTBL")== 0) {
	    strcpy(DefaultEBCDICTBL, param1);
	  } else if (strcasecmp(KeyWord, "DEFFORM") == 0) {
	    strncpy(DefaultForm, param1, sizeof(DefaultForm)-1);
	    DefaultForm[sizeof(DefaultForm)-1] = 0;
	  } else if (strcasecmp(KeyWord, "LUSERUIDLEVEL")== 0) {
	    sscanf(param1,"%d",&LuserUidLevel);
	    if (LuserUidLevel < 0)
	      LuserUidLevel = 0;
	  }
	}
	fclose(fd);
	if (*USER_EXITS)
	  if (read_exit_config(USER_EXITS) < 0) {
	    fprintf(stderr,"Can't read user-exits file `%s'\n",USER_EXITS);
	    return 0; /* Not ok. */
	  }

	return 1;	/* All ok */
}


/* ----------------------------------------------------------------

    READ_EXIT_CONFIG( path )

    Routine to read configuration file which is given with  'char *path'
    argument.
    (Note: MUST HAVE  POSTMAST  user !)

    Format is:

    # comment lines
					(Also completely whites are comment)
    # Default for MAILIFY program.
    # Short hand for ARBIT-PROGRAM /path/for/program
 
    Default-Mailify:	/path/for/mailify/program
    Default-Transfer:	/path/for/transfer/program

    # Rule for spool disposition path, defines also system default
    # user spool directory for those who don't have specially set
    # something else.
    # Possibilities:	~/bitspool  -- real user's only! (~ == home)
    #			/usr/path/path/ -- append 'touser' as subdir,
    #					    it is spool dir.
    #					   Valid for Exits and real users.
    #			/usr/path/path  -- explicite directory.
    #			/dev/null	-- explicite file (special case).
    #	When directory isn't present, it is built from upmost present level
    #	down to users bottommost level hmm...
    #   Question about ownership of directory/files...
    #     Real users:  real protections, programs start with setuid() user.
    #	  Exit users:  POSTMAST  (exits start as root anyway.)
    #	  Exited reals: real protections, programs start with setuid() user.

    Spool-Dir:	~/bitspool or /path/path/
    Postmast-Dir: /path/path/path

    # Now list of things to match and then what to do
    # To do keywords:	FORGET-IT	to /dev/null.
    #			LEAVE-INTO-SPOOL just so.  Into default or given spool.
    #			ARBIT-PROGRAM	starts arbitary program with arbitary
    #					arguments telling about file location
    #					and its properties.
    #					If fails, well..
    #			MAILIFY		ARBIT-PROGRAM with default program.
    #			NOTIFY-BY-MSG	send NJE message to someone.
    #			TRANSFER-TO	send over the net to somebody else.
    # Exit table begin keyword:

    Exit-Table:

    # Args:
    # touser8 tonode8 fname8   ftype8  pun? fruser8  frnode8  dist8    sysin?   action   SpoolDir  ExtraArgument
    MAILER   LOCAL    *        *        PUN *        *        *        *        MAILIFY  /usr/spool/bitmail/
    NOBODY   LOCAL    *        *        *   *        *        *        *        FORGET-IT
    FOOBAT   LOCAL    *        *        *   *        *        *        *        TRANSFER-TO default touser@whatnode
    *        *        *        *        *   *        *        *        *        LEAVE-INTO-SPOOL default

    ...
    <eof>

 Programs are executed with following ARGV:
  0  =  ProgPath
  1  =  FilePath

   ---------------------------------------------------------------- */

static char *ReadNoncommentLine( fil,buf,bufsiz )
     FILE **fil;
     char *buf;
     const int bufsiz;
{
	char *s;

	if (*fil == NULL) return NULL;
	while (!feof(*fil) && !ferror(*fil)) {
		if (fgets(buf,bufsiz,*fil) == NULL) break;
		if ((s = strrchr(buf,'\n')) != NULL) *s = 0;
		s = buf;
		while ((*s != 0) && ((*s == ' ') || (*s == '\t'))) ++s;
		if (*s == 0) continue; /* White space */
		if (*s == '#') continue; /* Comment line */
		return s;
	}
	fclose(*fil); *fil = NULL;
	return NULL;
}

int
read_exit_config( path )
     const char *path;
{
	char	line[250]; /* Just something long... */
	char	key[60]; /* Should be enough */
	char	arg[250]; /* hmm... */
	FILE	*cfg = fopen(path,"r");
	int	scans, rc;

	DefaultSpoolDir       [sizeof(DefaultSpoolDir)       -1] = 0;
	DefaultPOSTMASTdir    [sizeof(DefaultPOSTMASTdir)    -1] = 0;

	if (cfg == NULL) {
	  /* Config reading failed, we leave defaults or old setups - whatever.. */
	  return -1;
	}

	rc = 0;
	while (1) {

	  if (ReadNoncommentLine( &cfg,line,sizeof line ) == NULL) break;
	  /* Didn't get line for keywords.. */
	  if ( (scans=sscanf(line,"%s %s",key,arg)) < 1) break;
	  if (strcasecmp("EXIT-TABLE:",key)==0)
	    break;
	  if (scans < 2) {
	    fprintf(stderr,"LIBREADCFG: Exit configuration keywords expect arguments: Key: `%s'\n",key);
	    rc = -1;
	    continue;
	  }
	  if (strcasecmp("SPOOL-DIR:",key)==0)
	    strncpy(DefaultSpoolDir,arg,
		    sizeof(DefaultSpoolDir)-1);
	  else if (strcasecmp("POSTMAST-DIR:",key)==0)
	    strncpy(DefaultPOSTMASTdir,arg,
		    sizeof(DefaultPOSTMASTdir)-1);
	  else {
	    fprintf(stderr,
		    "LIBREADCFG: Unrecognized file-exit keyword: `%s'\n",key);
	    rc = -1;
	    break;
	  }
	}

	/* Actual exits aren't interesting for us - clients.. */
	if(cfg)
	  fclose(cfg);
	return rc;
}
