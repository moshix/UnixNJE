/* ---------------------------------------------------------------- *
 |    UNIX_MSGS.C	-- A module for FUNET-NJE		    |
 |								    |
 |    Traps, and processes messages				    |
 |								    |
 * ---------------------------------------------------------------- */

#include "consts.h"
#include "prototypes.h"
#include "unix_msgs.h"

extern char *ReadNoncommentLine __(( FILE **fil, char *buf, const int bufsiz,
				     int *linenump ));
extern void *ReallocTable __(( void *oldtbl, const int eltsize, const int newelts ));

char *CmdHelpFile = NULL;

#define ACT_DISCARD	0	/*  "/dev/null"			*/
#define ACT_RUN		1	/* run arbitary program		*/
#define ACT_PIPE	2	/* Pipe via UDP datagram	*/
#define ACT_BRCAST	3	/* the default..		*/
#define ACT_BUILTIN	4	/* Builtin COMMANDS		*/

struct MSG_EXIT {
	char	cmdmsg;	/* 'C', or 'M' */
	char	touser[9], tonode[9];
	char	fruser[9], frnode[9];
	int	action;
	char	*cmdpattern;	/* strdup()ed string */
	char	*arg;		/* strdup()ed string, or PIPE data.. */
};

/* Exits from config file parser */
static struct MSG_EXIT **Msg_Exits = NULL;
static int Msg_Exits_cnt = 0;
static int Msg_Exits_spc = 0;

/* Exits from the users/network.. */
static struct MSG_EXIT **Reg_Exits = NULL;
static int Reg_Exits_cnt = 0;
/* static int Reg_Exits_spc = 0; */

static struct MSG_EXIT *
exit_alloc(p_exits,p_cnt,p_spc)
struct MSG_EXIT ***p_exits;
int *p_cnt, *p_spc;
{
	/* p_exits points to the exit array variable.. */
	struct MSG_EXIT *mex = (void*) malloc(sizeof(struct MSG_EXIT));
	struct MSG_EXIT **mxp = *p_exits;

	if (mex == NULL) return NULL;

	if (*p_cnt >= *p_spc) {
	  *p_spc += 8;
	  if (mxp == NULL)
	    mxp = (void*) malloc(*p_spc * (sizeof(struct MSG_EXIT *)));
	  else
	    mxp = (void*) realloc(mxp, *p_spc * (sizeof(struct MSG_EXIT *)));
	}
	if (mxp == NULL) return NULL;

	*p_exits = mxp;

	mxp[*p_cnt] = mex;
	mex->arg = NULL;
	mex->cmdpattern = NULL;
	*p_cnt += 1;
	return mex;
}

int
read_message_config( path )  /* Config reader... */
     const char *path;
{
	FILE *cfg = fopen(path,"r");
	char line[512];
	char key[30];
	char arg[300];
	char frnode[9], fruser[9], tonode[9], touser[9];
	int linenro = 0;
	int cmdmsg = 0;
	int i;
	struct MSG_EXIT *mex;

	if (Msg_Exits != NULL) {
	  for (i = 0; i < Msg_Exits_cnt; ++i) {
	    if (Msg_Exits[i]->arg)
	      free(Msg_Exits[i]->arg);
	    if (Msg_Exits[i]->cmdpattern)
	      free(Msg_Exits[i]->cmdpattern);
	    free(Msg_Exits[i]);
	  }
	  free(Msg_Exits);
	  Msg_Exits = NULL;
	  Msg_Exits_cnt = 0;
	  Msg_Exits_spc = 0;
	}

	if (!cfg) {
	  logger(1,"UNIX_MSGS: read_message_config(%s) didn't open the config file!\n",path);
	  return 1;
	}

	while (1) {
	  
	  if (ReadNoncommentLine( &cfg,line,sizeof line,&linenro ) == NULL)
	    break;

	  if ((i = sscanf(line,"%29s %s",key,arg)) < 1) break;

	  if (strcasecmp("CmdHelpFile:",key)==0) {
	    if (i < 2) continue;
	    if (CmdHelpFile != NULL) free(CmdHelpFile);
	    CmdHelpFile = strdup(arg);
	  } else {
	    char *tokseps = " \t";
	    char *s = strtok(line,tokseps);
	    if (s != NULL) {
	      strncpy(touser,s,9);
	      despace(touser,8);
	    }
	    if (s && (s = strtok(NULL,tokseps))) {
	      strncpy(tonode,s,9);
	      despace(tonode,8);
	    }
	    if (s && (s = strtok(NULL,tokseps))) {
	      strncpy(fruser,s,9);
	      despace(fruser,8);
	    }
	    if (s && (s = strtok(NULL,tokseps))) {
	      strncpy(frnode,s,9);
	      despace(frnode,8);
	    }
	    cmdmsg = 0;
	    if (s && (s = strtok(NULL,tokseps))) {
	      if (*s == 'C' || *s == 'c')
		cmdmsg = 1;
	      else if (*s == 'M' || *s == 'm')
		cmdmsg = -1;
	    }
	    if (cmdmsg == 0) {
bad_entry:
	      logger(2,"UNIX_MSGS: read_message_config(%s) on line %d a bad entry.\n",path,linenro);
	      continue;
	    }
	    if (cmdmsg == 1 /* CMD */) {

	      /* CMD checkup.. */
	      int action = -1;
	      char *cmd;
	      char *p = strtok(NULL,"\001\n"); /* Whole remaining line.. */
	      while (p && *p && (*p == ' ' || *p == '\t')) ++p;
	      if (!p || *p != '"')
		goto bad_entry;

	      /* Get the match pattern w/o its quotes */
	      cmd = strtok(p+1,"\"");
	      if (!cmd || *cmd == 0)
		goto bad_entry;

	      s = strtok(NULL,tokseps);
	      if (!s || !*s)
		goto bad_entry;

	      p = strtok(NULL,"\001\n"); /* Whole remaining line.. */
	      while (p && *p && (*p == ' ' || *p == '\t')) ++p;

	      /* cmd == command pattern
		 s   == action verb
		 p   == action argument string */
	      if (strcasecmp(s,"BUILTIN")==0)
		action = ACT_BUILTIN;
	      else if (strcasecmp(s,"RUN")==0)
		action = ACT_RUN;
	      else
		goto bad_entry;

	      mex = exit_alloc(&Msg_Exits,&Msg_Exits_cnt,&Msg_Exits_spc);
	      if (mex == NULL) {
		logger(1,"UNIX_MSGS: read_message_config() OUT OF MEMORY!  AARGH!\n");
		break;
	      }
	      mex->cmdmsg = 'C';
	      strcpy(mex->touser,touser);
	      strcpy(mex->tonode,tonode);
	      strcpy(mex->fruser,fruser);
	      strcpy(mex->frnode,frnode);
	      mex->action = action;
	      mex->cmdpattern = strdup(cmd);
	      if ((action == ACT_RUN || action == ACT_BUILTIN) && p)
		mex->arg = strdup(p);

	      /* logger(1,
		 "UNIX_MSGS: Exit entry: %s@%s <- %s@%s CMD cmd='%s' %s arg=%s\n",
		 touser,tonode,fruser,frnode,cmd,s,mex->arg); */

	    } else {
	      /* MSG checkup.. */
	      int action = -1;
	      s = strtok(NULL,tokseps);
	      if (!s || !*s)
		goto bad_entry;

	      if (strcasecmp(s,"BRCAST")==0)
		action = ACT_BRCAST;
	      else if (strcasecmp(s,"RUN")==0)
		action = ACT_RUN;
	      else if (strcasecmp(s,"DISCARD")==0)
		action = ACT_DISCARD;
	      else
		goto bad_entry;


	      mex = exit_alloc(&Msg_Exits,&Msg_Exits_cnt,&Msg_Exits_spc);
	      if (mex == NULL) {
		logger(1,"UNIX_MSGS: read_message_config() OUT OF MEMORY!  AARGH!\n");
		break;
	      }
	      mex->cmdmsg = 'M';
	      strcpy(mex->touser,touser);
	      strcpy(mex->tonode,tonode);
	      strcpy(mex->fruser,fruser);
	      strcpy(mex->frnode,frnode);
	      mex->action = action;
	      if (action == ACT_RUN) {
		char *p = strtok(NULL,"\001\n"); /* Whole remaining line.. */
		while (p && *p && (*p == ' ' || *p == '\t')) ++p;

		if (p)
		  mex->arg = strdup(p);
	      }
	      /* logger(1,
		 "UNIX_MSGS: Exit entry: %s@%s <- %s@%s MSG %s arg=%s\n",
		 touser,tonode,fruser,frnode,s,mex->arg); */
	    }
	  }
	}

      
	if (cfg)
	  fclose(cfg);

	logger(1,"UNIX_MSGS: New exit table (%s) scanned in.\n",path);

	return 0;
}

static const char *
find_tok_start(string,number)
const char *string;
const int number;
{
	int i = 1;

	/* Skip over the possible preceeding space */
	while (*string == ' ' || *string == '\t')
	  ++string;

	/* Skip preceeding tokens */
	while (i < number && *string != 0) {
	  /* Skip over the token */
	  while (*string != 0 && *string != ' ' && *string != '\t')
	    ++string;
	  /* Skip over its trailing space */
	  while (*string == ' ' || *string == '\t')
	    ++string;
	  /* and count it */
	  ++i;
	}
	return string;
}

static int
cmd_builtin(Exit,touser,tonode,fruser,frnode,text)
const struct MSG_EXIT *Exit;
const char *touser, *tonode, *fruser, *frnode;
char *text;
{
	char line[800];
	char Faddress[20],Taddress[20];
	sprintf(Faddress,"%s@%s",fruser,frnode);
	sprintf(Taddress,"%s@%s",touser,tonode);

	if (strcasecmp(Exit->arg,"HELP")==0) {
	  strcpy(text,"HELP");
	  return msg_helper(Taddress,Faddress);

	} else if (strncasecmp(Exit->arg,"ERROR",5)==0) {
	  char *s = Exit->arg;
	  while (*s != 0 && *s != ' ' && *s != '\t') ++s;
	  while (*s == ' ' || *s == '\t') ++s;
	  if (*s != 0)
	    strcpy(line,s);
	  else
	    sprintf(line,"Unknown command `%s', try HELP",text);
	  send_nmr(Taddress,Faddress,line,strlen(line),ASCII,CMD_MSG);
	  return 1;

	} else if (strcasecmp(Exit->arg,"HARDCODED")==0) {
	  /* Fall back to the hard-coded stuff */
	  return 0;

	} else if (strncasecmp(Exit->arg,"ALIAS",5)==0) {
	  /* Command aliasing */
	  char *a = Exit->arg;
	  char *l = line;
	  /* Skip the 'ALIAS'+next spaces */
	  while (*a != 0 && *a != ' ' && *a != '\t') ++a;
	  while (*a == ' ' || *a == '\t') ++a;
	  while (*a != 0) {
	    if (*a == '$') {
	      /* Ok '$' is a special prefix, it can have numbers after it,
		 and  '$$' means 'put a $ in there. */
	      int toknum = 0;
	      const char *s;
	      ++a;
	      if (*a == '$') {
		*l++ = *a++;
		continue;
	      }
	      while (*a >= '0' && *a <= '9')
		toknum = toknum * 10 + (*a++ - '0');
	      /* Ok, 'toknum' is a numeric value */
	      s = find_tok_start(text,toknum);
	      while (*s != 0 && *s != ' ' && *s != '\t')
		*l++ = *s++;
	      if (*a == '+') {
		/* Copy everything since that token.. */
		while (*s != 0)
		  *l++ = *s++;
		++a;
	      }
	    } else {
	      *l++ = *a++;
	    }
	  }
	  *l = 0;
	  strncpy(text,line,131);
	  text[132] = 0;
	  return 0; /* Return it for the hard-coded stuff to handle.. */

	} else {
logger(1,"UNIX_MSGS: cmd_builtin(`%s') -- unknown builtin on command `%s'!\n",
       Exit->arg,text);
	  sprintf(line,"Unknown builtin `%s' on command `%s', report BUG",
		  Exit->arg,text);
	  send_nmr(Taddress,Faddress,line,strlen(line),ASCII,CMD_MSG);
	  return 1;
	}
}


static void
msg_pipeout(Exit,touser,tonode,fruser,frnode,text)
const struct MSG_EXIT *Exit;
const char *touser, *tonode, *fruser, *frnode, *text;
{
	/* XXX: MSG_PIPEOUT() TO BE IMPLEMENTED! */
}

static void
msgcmd_run(Exit,touser,tonode,fruser,frnode,text)
const struct MSG_EXIT *Exit;
const char *touser, *tonode, *fruser, *frnode, *text;
{
	pid_t pid;
	char *args, *p;
	char runprog[250];
	char Argv[2048];
	char *Argvp[200];
	static char *Envp[3] = {"IFS= \t\n", NULL, NULL};
	int i;

	logger(2,
	       "UNIX_MSGS: msgcmd_run('%s@%s' <- '%s@%s', msg='%s', arg='%s')\n",
	       touser,tonode,fruser,frnode,text,Exit->arg);

	p = Exit->arg;
	args = p;
	while (*args != 0 && *args != ' ' && *args != '\t') ++args;
	strncpy(runprog,p,args-p);
	runprog[args-p] = 0;
	p = runprog;
	while (*args != 0 && (*args == ' ' || *args == '\t')) ++args;

	if ((pid = fork()) < 0) {
	  logger(1,
		 "UNIX_FILES: ###### fork() failed!  errno=%d (%s)\n",
		 errno,PRINT_ERRNO);
	  return;
	}
	if (pid) {
	  /* Parent - main thread.. */
	  /* We do nothing here! */
	  logger(2,"Forking... pid=%d, prog='%s'\n",pid,p);
	  /* XXX: Actually we should put child monitoring data somewhere... */
	} else {
	  /*
	     Programs are executed with following ARGV:
	     0  =  ProgPath
	     1  =  FilePath
	     [2..=  args]
	     */
	  strcpy( Argv,p );
	  Argvp[0] = p = Argv;
	  p = p + strlen(p); *++p = 0;

	  i = 1;
	  while (*args != 0) {
	    while (*args != 0 &&
		   (*args == ' ' || *args == '\t')) ++args;
	    Argvp[i++] = p;
	    while (*args != 0 && *args != ' ' && *args != '\t') {
	      if (*args == '$') {
		/*  Macroexpansion in here, a'la: $FRUSER@$FRNODE  */
		char macname[20];
		char *q = macname;
		const char *cs = macname;
		int k = 9;
		int doing_args = 0;
		char matchch = 0;
		if (*++args == '{') {
		  matchch = '}';
		  ++args;
		} else if (*args == '(') {
		  matchch = ')';
		  ++args;
		} else if (*args == '$') {
		  ++args;
		  strcpy(macname,"$");
		  matchch = '$';
		}
		if (matchch != '$')
		  while (*args && k > 0 &&
			 ((matchch != 0 && *args != matchch) ||
			  (matchch == 0 &&
			   ((unsigned)*args >= (unsigned)'A' &&
			    (unsigned)*args <= (unsigned)'Z')))) {
		    *q++ = *args++;
		    --k;
		  }
		*q = 0;
		/*logger(2,"UNIX_MSGS: Matching macro var: `%s'\n",
		  macname);*/

		q = "";
		/* Ok, now we have the magic token: uppercase string */
		if (*macname == '$') {
		  sprintf(macname,"%d",(int)getpid());
		  cs = macname;
		} else if (strcmp(macname,"TOUSER")==0) {
		  cs = touser;
		} else if (strcmp(macname,"TONODE")==0) {
		  cs = tonode;
		} else if (strcmp(macname,"FRUSER")==0) {
		  cs = fruser;
		} else if (strcmp(macname,"FRNODE")==0) {
		  cs = frnode;
		} else if (strcmp(macname,"TEXT")==0) {
		  cs = text;
		} else if (strcmp(macname,"ARGS")==0) {
		  cs = text;
		  doing_args = 1;
		} /* Else what ?? */
		/* Copy the string in */
		if (!doing_args) {
		  /*logger(2,"UNIX_MSGS: Copying in string `%s'\n",cs);*/
		  while (*cs)
		    *p++ = *cs++;
		} else { /* We do $ARGS here.. */
		  /* Pre-clean spaces/tabs from front */
		  while (*cs == ' ' || *cs == '\t') ++cs;
		  while (*cs) {
		    /* While not space/tab, copy string */
		    while (*cs != 0 && *cs != ' ' && *cs != '\t')
		      *p++ = *cs++;
		    *p++ = 0; /* And mark its end */
		    /* Skip trailing blanks */
		    while (*cs == ' ' || *cs == '\t') ++cs;
		    /* Yet more to follow, start a new ARGV[] item */
		    if (*cs != 0)
		      Argvp[i++] = p;
		  }
		}
	      } else		/* else not started a $-macro */
		*p++ = *args++;
	    } /* end - while processing a token within ArgTemplate */
	    *p++ = 0;
	    /*logger(2,"UNIX_MSGS: Expanded argv[%d] to be \"%s\"\n",
	      i-1,Argvp[i-1]); */
	  }
	  Argvp[i] = NULL;
	  {
	    int dtbls = GETDTABLESIZE(NULL);
	    for (pid=0; pid < dtbls; ++pid) close(pid);
	  }
	  execve(Argvp[0],Argvp,Envp);
	  /* Right...  IF execve() returns -> Things are BAD! */
	  _exit(-errno);	/* Handles are closed, we do NOT flush here.*/
	}
}

/* Do command matching -- by following rules:
   "**" -- zero to any number of args (for fallback)
   "Q*UERY *" -- Match words Q QU QUE QUER QUERY for Q*UERY, and any token for
                 the star. If there is 3rd argument, don't match at all..
   "TOKEN**" -- Match any token starting with "TOKEN" -string.
 */
int
cmd_match(cmdstr,MsgTxt)
const char *cmdstr, *MsgTxt;
{
	const char *cmd = cmdstr;
	const char *txt = MsgTxt;
	char key1[20], key2[20];
	char *s1, *s2;
	int key1len, key2len;
	int matched = 0;

	while (*cmd == ' ' || *cmd == '\t') ++cmd;
	while (*txt == ' ' || *txt == '\t') ++txt;

	while (1) {
	  /* Pick respective tokens, at least  CMD  has a token in here */
	  key1len = 0;
	  while (*cmd != 0 && *cmd != ' ' && *cmd != '\t') {
	    if (key1len < (sizeof(key1)-1))
	      key1[key1len++] = *cmd;
	    ++cmd;
	  }
	  key1[key1len] = 0;
	  while (*cmd == ' ' || *cmd == '\t') ++cmd;

	  key2len = 0;
	  while (*txt != 0 && *txt != ' ' && *txt != '\t') {
	    if (key2len < (sizeof(key2)-1))
	      key2[key2len++] = *txt;
	    ++txt;
	  }
	  key2[key2len] = 0;
	  while (*txt == ' ' || *txt == '\t') ++txt;

	  if (key1len == 0 && key2len > 0) return 0; /* No match */
	  if (key1len == 0 && key2len == 0)
	    /* Ok, both finished. Quit */
	    return matched;
	  
	  if (strcmp("**",key1)==0)
	    /* Match that always! */
	    return 1;

	  if (strcmp("*",key1)==0 && key2len != 0) {
	    /* Single word, matched.. */
	    matched = 1;
	    continue;
	  }
	  /* Ok, now the pattern contains a token, and a blank
	     string won't match it.. */
	  if (key2len == 0) return 0;

	  upperstr(key1);
	  s1 = key1;
	  upperstr(key2);
	  s2 = key2;
	  while (*s1) {
	    if (*s1 == '*') {
	      ++s1;
	      /* Match the trailing strings */
	      if (*s1 == '*') {
		/* Pattern "START**" means: anything matches which starts
		   with "START".  */
		s2 = "";
		break;
	      }
	      while (*s1) {
		if (*s2 == 0) break; /* NMR ended, if non-wild part matched,
					all fine.. */

		if (*s1 != *s2) return 0; /* No match.. */
		++s1; ++s2;
	      }
	      if (*s2 != 0) return 0; /* NMR longer than pattern! No match */
	      break;
	    }
	    /* While we have not met wild-card star in the string */
	    if (*s2 == 0 || *s1 != *s2) return 0;
	    matched = 1;
	    ++s1;
	    ++s2;
	  }
	  if (*s2) return 0; /* NMR longer than pattern! No match */
	}
	/* NOT REACHED */
}

/* Process exits, return 0, if didn't match, or othervice must
   use either BRCAST, or builtin commands..   Return 1, if
   the thing got handled already.  */
static int
msg_exit_proc(Exits,ExitCnt,ToName,ToNode,FromName,FromNode,cmdmsg,MsgTxt)
const struct MSG_EXIT **Exits;
const int ExitCnt;
const char *ToName, *ToNode, *FromName, *FromNode;
const NMRCODES cmdmsg;
char *MsgTxt;
{
	int i;


	--Exits;
	for (i = 0; i < ExitCnt; ++i) {
	  ++Exits;

	  /* Look up for possible matches..  Elimination works :-)  */

	  if (cmdmsg == CMD_CMD && (*Exits)->cmdmsg != 'C') continue;
	  if (cmdmsg == CMD_MSG && (*Exits)->cmdmsg != 'M') continue;

	  if (cmdmsg == CMD_MSG &&
	      (*Exits)->touser[0] != '*' &&
	      strcmp((*Exits)->touser,ToName)!=0) continue;
	  if ((*Exits)->tonode[0] != '*' &&
	      strcmp((*Exits)->tonode,ToNode)!=0) continue;
	  if ((*Exits)->fruser[0] != '.' &&
	      (*Exits)->fruser[0] != '*' &&
	      strcmp((*Exits)->fruser,FromName)!=0) continue;
	  if ((*Exits)->fruser[0] == '.' && *FromName != 0) continue;
	  if ((*Exits)->frnode[0] != '*' &&
	      strcmp((*Exits)->frnode,FromNode)!=0) continue;

	  /* Ok, match so far, now do something about them.. */
	  if (cmdmsg == CMD_CMD) {

	    int matched = cmd_match((*Exits)->cmdpattern,MsgTxt);
	    if (!matched) continue; /* Ok, pick next */

	    /* Matched ???  Ok, lets do something about them! */
	    if ((*Exits)->action == ACT_DISCARD) {
	      return 1;
	    }
	    if ((*Exits)->action == ACT_RUN) {
	      msgcmd_run(*Exits,ToName,ToNode,FromName,FromNode,MsgTxt);
	      return 1;
	    }
	    if ((*Exits)->action == ACT_BUILTIN)
	      return cmd_builtin(*Exits,ToName,ToNode,FromName,FromNode,MsgTxt);

	    logger(1,"UNIX_MSGS: *** exit of a CMD got action code %d ??\n",
		   (*Exits)->action);
	    return 0;

	  } else {    /*     We have a  CMD_MSG  for processing:    */

	    if ((*Exits)->action == ACT_DISCARD) {
	      /*logger(2,"UNIX_MSGS: DISCARD msg from %s@%s to %s@%s, txt='%s'\n",
		FromName,FromNode,ToName,ToNode,MsgTxt); */
	      return 1; /* yep, that was its 'processing' :-) */
	    }
	    if ((*Exits)->action == ACT_BRCAST)
	      return 0; /* Let the caller to handle it in the
			   old-fashioned way.. */
	    if ((*Exits)->action == ACT_PIPE) {
	      msg_pipeout(*Exits,ToName,ToNode,FromName,FromNode,MsgTxt);
	      return 1;
	    }
	    if ((*Exits)->action == ACT_RUN) {
	      msgcmd_run(*Exits,ToName,ToNode,FromName,FromNode,MsgTxt);
	      return 1;
	    }
	    logger(1,"UNIX_MSGS: *** exit of a MSG got action code %d ??\n",
		   (*Exits)->action);
	    return 0;
	  }
	}

	return 0;
}


void
message_exit(ToName,ToNode,FromName,FromNode,cmdmsg,MsgTxt)
const char *ToName, *ToNode, *FromName, *FromNode;
const NMRCODES cmdmsg;
char *MsgTxt;
{
	char	MsgBuf[20+142];
	int 	i;

	/* Mailer, et.al. receives quite a lot of messages,
	   but are never logged-in... */

	if (Reg_Exits != NULL &&
	    msg_exit_proc(Reg_Exits,Reg_Exits_cnt,
			  ToName,ToNode,FromName,FromNode,cmdmsg,MsgTxt))
	  return;
	if (Msg_Exits != NULL &&
	    msg_exit_proc(Msg_Exits,Msg_Exits_cnt,
			  ToName,ToNode,FromName,FromNode,cmdmsg,MsgTxt))
	  return;

	if (cmdmsg == CMD_CMD) {
	  handle_local_command(FromNode,FromName,MsgTxt);
	  return;
	}

	if (*FromName != 0)
	  sprintf(MsgBuf,"\r\007%s(%s): %s\r\n",FromName,FromNode,MsgTxt);
	else
	  sprintf(MsgBuf,"\r\007%s: %s\r\n",FromNode,MsgTxt);

	if ((i = send_user(ToName, MsgBuf)) <= 0) {

	  /* If there is a username we can answer.
	     If not - the source was machine.
	     Do not answer if the message text starts with *,
	     since these are usually GONE messages */

	  if ((*FromName != '\0') && (*MsgTxt != '*')) {
	    char DestinationNode[20],OriginNode[10],line[256];

	    if (i == 0)	/* Not logged in */
	      sprintf(line, "* %s not logged in", ToName);
	    else	/* -1 = Not logged-in, but got the Gone */
	      sprintf(line, "*yGONE: %s not logged-in, but your message has been recorded", ToName);

	    /* Create the sender and receiver */
	    sprintf(DestinationNode, "%s@%s", FromName, FromNode);
	    sprintf(OriginNode, "@%s", LOCAL_NAME);
	    send_nmr(OriginNode, DestinationNode, line,
		     strlen(line), ASCII, CMD_MSG);
	  }
	}
}



void
msg_register(ptr,size)
const void *ptr;
const int size;
{
	/* XXX: msg_register() TO BE IMPLEMENTED! */
}


int
msg_helper(FrAddr,ToAddr)
const char *FrAddr, *ToAddr;
{
	FILE *helpf;
	char line[200];
	char *s;

	if (CmdHelpFile == NULL) return 0; /* Do the builtin job.. */

	helpf = fopen(CmdHelpFile,"r");
	if (!helpf) return 0; /* Do the builtin job.. */

	while (!feof(helpf) && !ferror(helpf)) {
	  *line = 0;
	  if (fgets(line,sizeof line,helpf) == NULL) break;
	  if ((s = strchr(line,'\n')) != NULL) *s = 0;
	  send_nmr(FrAddr, ToAddr, line, strlen(line), ASCII, CMD_MSG);
	}
	fclose(helpf);

	return 1;
}
