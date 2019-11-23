/* UNIX-FILES  -- Module for FUNET-NJE by  <mea@nic.funet.fi>

    Procedure  inform_filearrival()  handles unix escapes for
    non-BSMTP mail transports.

*/

#include "consts.h"
#include "prototypes.h"


typedef enum FileExitActions {
	Act_KEEP,
	Act_DISCARD,
	Act_RUN,
	Act_NOTIFY
} FileExitActions;

typedef enum SpoolTypes {
	Type_ANY,
	Type_PUNCH,
	Type_PRINT,
	Type_SYSIN
} SpoolTypes;

static char *ExitActionStrs[] = {
	"KEEP",
	"DISCARD",
	"RUN",
	"NOTIFY"
};

struct FILE_EXIT_CONFIG {
	int	UsageCnt;  /* Some statistics... */
	/* First what to match... */
	char	touser[9];
	char	tonode[9];
	char	fname[9];
	char	ftype[9];
	char	fruser[9];
	char	frnode[9];
	char	class;
	char	dist[9];
	SpoolTypes spooltype;
	/* what to do ... */
	FileExitActions	Action;
	char	SpoolDir[256];
	char	ArgTemplate[256];
	};

#ifndef	DEF_MAILIFY_PROGRAM /* SITE_CONSTS.H can override */
# ifdef	UNIX
#  define DEF_MAILIFY_PROGRAM "/usr/lib/huji/mailify"
# else /* for VMS ? */
#  define DEF_MAILIFY_PROGRAM ???
# endif
#endif

#ifndef	DEF_SYSIN_PROGRAM /* SITE_CONSTS.H can override */
# ifdef	UNIX
#  define DEF_SYSIN_PROGRAM "/usr/lib/huji/sysin"
# else /* for VMS ? */
#  define DEF_SYSIN_PROGRAM ???
# endif
#endif

#ifndef	DEF_TRANSFER_PROGRAM /* SITE_CONSTS.H can override */
# ifdef	UNIX
#  define DEF_TRANSFER_PROGRAM "/usr/lib/huji/transfer"
# else /* for VMS ? */
#  define DEF_TRANSFER_PROGRAM ???
# endif
#endif

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


static int	Check_File_Exit_Validity __(( const struct FILE_EXIT_CONFIG *ExitPtr, const int InParser ));
#if 0
static char    *FileDispositionExit __(( struct FILE_PARAMS *fp ));
#endif
static struct FILE_EXIT_CONFIG *AnalyzeFileExitAction __(( struct FILE_EXIT_CONFIG *ex, struct FILE_PARAMS *fp, int *ExplicitePseudoUser, int *RealUser, int *RealUID, char *ToUser, char *HomeDir ));


struct FILE_EXIT_CONFIG *FileExitTable = NULL;
struct FILE_EXIT_CONFIG *TransitExitTable = NULL;
static struct FILE_EXIT_CONFIG DefaultFileExit = {
	0,"*","*","*","*","*","*",'*',"*",Type_ANY,Act_KEEP,".",""
};
static char DefaultSpoolDir[256]        = DEF_SPOOL_DIR_RULE;
static char DefaultPOSTMASTdir[256]     = DEF_POSTMAST_SPOOL_DIR;

struct passwd *Getpwnam( namestr )
 const char *namestr;
{
  char lowername[9];

  strncpy(lowername,namestr,8);
  lowername[8] = 0;
  lowerstr(lowername);
  return getpwnam(lowername);
}



static int
Check_File_Exit_Validity( ExitPtr,InParser )
     const struct FILE_EXIT_CONFIG *ExitPtr;
     const int InParser;
{
	struct passwd *upassp = NULL;

	if (ExitPtr->touser[0] != 0) return -1;
	if (InParser && (ExitPtr->touser[0] != '*'))
	  upassp = Getpwnam(ExitPtr->touser);
	/* XXX: Check here validity of directories, expand ~/ -notation
	        (if InParser != 0), validate programs, validate directories.
		If InParser != 0, create noexistent spool DIRECTORIES,
		give them uid/gid from this particular user, mod=0700,
		If not real user, ownership to root. */

	return 0; /* Exit is valid! */
}

static int tracethis = 0;


char *
ExpandHomeDir( PathOrDir,HomeDir,ToUser,path )
     const char *PathOrDir,*HomeDir,*ToUser;
     char *path;
{
	struct stat fstats;
	int plen;
	struct passwd *passwds;
	
	char *s;
	char path2[512],path3[512];

	if (strcmp(PathOrDir,"default")==0 ||
	    strcmp(PathOrDir,".")==0       || *PathOrDir == 0) {
	  PathOrDir = DefaultSpoolDir;
	}

	if (*PathOrDir == '~') {
	  if (PathOrDir[1] != '/') {
	    /* Uh.. ~/ would have been easy... */
	    strcpy( path2,PathOrDir+1 );
	    if ((s = strchr(path2,'/'))) *s = 0;
	    if ((passwds = Getpwnam(path2))) {
	      strcpy( path,passwds->pw_dir );
	      *s = '/';
	      strcat( path,s );
	    } else {
	      return NULL;
	      /* Uhh... Wasn't real user... */
	    }
	  } else { /* ~/... */
	    strcpy( path3,PathOrDir ); /* Now that our 'home' isn't blank... */
	    strcpy( path,HomeDir );
	    strcat( path,path3+1 );
	  }
	} else
	  if (path != PathOrDir)
	    strcpy( path,PathOrDir ); /* Jeeks, they may be different ones */

	plen = strlen(path);
	s = path + plen - 1;

	/* A '/' terminating dir path ? */
	if ((plen > 0) && (*s != '/') ) {
	  if (stat(path,&fstats) == 0) {	/* Ok, file does exist */
	    if ((fstats.st_mode & S_IFMT) == S_IFDIR) { /* Ok, directory */
	      *++s = '/';
	      *s = 0;
	      plen += 1; /* Then fall to create unique fileid. */
	    } else { /* Not directory, but exists */
	      return path;
	    }
	  } else {	/* Ok, file does not exist */
	    if (errno != ENOENT) {
	      /* XXX: stating failed, and not because nonexistent file.
		      This calls for stronger measures, or nice fault ?
		      Invent something ! */
	      return NULL;
	    }
	    /* This wasn't terminated with '/'.
	       Thus create this directory ?? */
	again_dir1:  /* following 5 lines from gtar program */
	    if (mkdir(path, 0770) != 0) {
	      if (make_dirs(path))
		goto again_dir1;
	      logger(1,"Could not make directory %s\n",path);
	      /* XXX: Bad failure! */
	      return NULL;
	    }
	    /* Directory created, lets fall to unique file creation */
	  }
	} else {  /* A '/' -terminated path.  Make sure dir is ok. */
	  *s = 0;
	  if (stat(path,&fstats) != 0) {	/* Ok, dir does not exist */
	again_dir2:  /* following 5 lines from gtar program */
	    if ( mkdir(path, 0770) != 0) {
	      if (make_dirs(path))
		goto again_dir2;
	      logger(1,"Could not make directory %s\n",path);
	      /* XXX: Bad failure! */
	      return NULL;
	    }
	  } /* Here it MUST be directory... */
	  *s = '/';
	  strcat( path,ToUser );
	  strcat( path,"/" );
	}
	return path;
}


/* ----------------------------------------------------------------
   char *RenameToUniqueFileid( fp,PathOrDir,ToUser,HomeDir,MaxFiles )

    When given path to FILE (existing normal/special(/dev/null), not dir)
    this renames it with given rules and return pointer to new name.

    When given directory, this return opened WRITE handle to unique
    filename in it - it is gotten by opening file with some temp name,
    then renaming it so that its INODE number is used as id.

    If rename(2) operation fails on renaming, temporary name is left as is.

   ---------------------------------------------------------------- */

char *
RenameToUniqueFileid( fp, PathOrDir, ToUser, HomeDir, MaxFiles, RealUID )
     const char *PathOrDir, *ToUser, *HomeDir;
     struct FILE_PARAMS *fp;
     const int MaxFiles, RealUID;
{
	struct stat fstat1;
	
	char path[512],path2[512], *p;
	unsigned char *s;
	int	SpoolID = 0;
	int	i;

	if (! ExpandHomeDir( PathOrDir,HomeDir,ToUser, path )) {
	  if (tracethis) logger(2,"UNIX_FILES: RenameToUniqueFileid() call to ExpandHomeDir() gave NULL\n");
	  return RenameToUniqueFileid(fp,DefaultPOSTMASTdir,"POSTMAST","",
				      100000, -1 );
	  /* Uhh... Wasn't real user... */
	}

	logger(3,"UNIX_FILES: RenameToUniqueFileid() call to ExpandHomeDir() gave '%s'\n",path);

	fp->OurFileId = 0; /* Nothing defined yet */
	
	strcpy( path2,path );
	if (stat(path,&fstat1) != 0) /* Hmm... Needs to make some dirs.. */
	  make_dirs( path );
	if (stat(path,&fstat1) != 0) { /* Awk... Nothing ok! */
	  logger(1,"UNIX_FILES: RenameToUniqueFileid() failed to created target directory! '%s'\n",path);
	  /* strcpy( fp->SpoolFileName,path ); */
	} else { /* All ok ! */

	  int FirstID, renameok = 0;

	  p = path2+strlen(path2);

	  if (RealUID > 0 && fstat1.st_uid != RealUID) {
	    /* If the directory ownership is not right, chown.. */
	    chown(path,RealUID,-1);
	    fstat1.st_uid = RealUID;
	  }

	  if (p[-1] != '/')
	    *p++ = '/';

	  SpoolID = get_user_fileid(ToUser);
	  FirstID = SpoolID;
	  do {
	    sprintf(p,"%04d",SpoolID);
	    if (link(fp->SpoolFileName,path2) == 0) {
	      renameok = 1;
	      fp->OurFileId = SpoolID;
	      break;
	    }
	    /* Not linked to the new name, now sequence.. */
	    SpoolID = (SpoolID % 9900) + 1;
	  } while (SpoolID != FirstID);
	  if (SpoolID != FirstID)
	    set_user_fileid(ToUser,SpoolID);

	  /* If the file was not changed to SpoolID, much is lost.. */
	  if (!renameok) {
	    sprintf(p,"A%08lxA",(long unsigned)time(NULL));

	    s = (unsigned char *)(strlen(path2)+path2-1);

	    for (i = 0; i < 200;++i) {
	      if (link(fp->SpoolFileName,path2) == 0) break;
	      /* Not linked to the new name, now sequence.. */
	      *s += 1;
	      if ((*s >'Z') && (*s <'a')) *s = 'a'; /* Some 60-odd different
						       ones... */
	      continue;
	    }
	  }
	  if (unlink(fp->SpoolFileName) == 0) {
	    strcpy( fp->SpoolFileName,path2 );
	    /* Fine, done */
	  }
	}
	logger(3,"UNIX_FILES: RenameToUniqueFileid(): new file name: '%s'\n",
	       fp->SpoolFileName);
	return fp->SpoolFileName;
}


#if 0

static char *
FileDispositionExit( fp )
     struct FILE_PARAMS *fp;
{
	struct FILE_EXIT_CONFIG *ex;
	char *s;
	static char FileDispos[256] = "";
	char ToUser[20];
	char HomeDir[256];
	int ExplicitePseudoUser, RealUser, RealUID;

	ex = AnalyzeFileExitAction(FileExitTable,fp,
				   &ExplicitePseudoUser,&RealUser,&RealUID,
				   ToUser,HomeDir);
	if (ex == NULL) {
	  return CreateUniqueFileid( fp,DefaultPOSTMASTdir,"POSTMAST","" );
	  /* Uh..  FAULT immediately.. Awk! */
	}
	switch (ex->Action) {
	  case Act_DISCARD:
	      return "/dev/null";	/* UNIX NULL device */
	  default:
	      /* XXX: Check feasibility of given SPOOL, if file create
		      there isn't possible with associated uids (real
		      ones, if available), recurse via POSTMAST.	*/
	      strcpy( FileDispos,ex->SpoolDir );
	      if (strcmp(FileDispos,"default")==0 ||
		  strcmp(FileDispos,".")==0       || *FileDispos == 0) {
		strcpy( FileDispos,DefaultSpoolDir );
	      }

	      if (RealUser || ExplicitePseudoUser) {
		return CreateUniqueFileid( fp,FileDispos,ToUser,HomeDir );
	      }
	      break;
	}
	return CreateUniqueFileid( fp,DefaultPOSTMASTdir,"POSTMAST","" ); /* ?? */
}

#endif

static struct FILE_EXIT_CONFIG *
AnalyzeFileExitAction( ex,fp,ExplicitePseudoUser,RealUser,RealUID,ToUser,HomeDir )
     struct FILE_EXIT_CONFIG *ex;
     struct FILE_PARAMS *fp;
     int *ExplicitePseudoUser, *RealUser, *RealUID;
     char *HomeDir, *ToUser;
{
	char FromUser[20],FromNode[20],ToNode[20];
	char *s;
	struct passwd *passwds;

	strncpy(ToUser,fp->To,8); ToUser[8] = 0;
	if ((s = strchr(ToUser,'@')) != NULL) *s = 0;
	*ToNode = 0;
	if ((s = strchr(fp->To,'@')) != NULL) {
	  strcpy(ToNode,s+1);
	}
	*ExplicitePseudoUser = 0;	/* No knowledge yet */
	*RealUser   = 0;
	*RealUID    = -1;	/* Great Mr. Nobody... */
	*HomeDir = 0;
	if ((passwds = Getpwnam(ToUser)) != NULL) {
	  *RealUser = 1;
	  strcpy(HomeDir,passwds->pw_dir);
	  *RealUID = passwds->pw_uid;
	}
	if (ex == NULL) {
	  logger(1,"UNIX_FILES:  FileExitTable not configured!\n");
	  return &DefaultFileExit;	/* Didn't find specials, use default */
	}

	strcpy(FromUser,fp->From); *FromNode = 0;
	if ((s = strchr(FromUser,'@')) != NULL) {
	  strcpy(FromNode,s+1);
	  *s = 0;
	}

	--ex;
	while (*((++ex)->touser) != 0) {

	  if ((*(ex->touser) == '.' && *ToUser != 0) ||
	      ((*(ex->touser) != '.') && /* non-blank field */
	       (*(ex->touser) != '*') && /* non wild field */
	       (*(ex->touser) != '+') && /* via name/node remap database */
	       (strcasecmp(ex->touser,ToUser) != 0))) continue;
	  if ((*(ex->tonode) != '*') &&
	      (strcasecmp(ex->tonode,ToNode) != 0)) continue;
	  if ((*(ex->fruser) != '*') &&
	      (strcasecmp(ex->fruser,FromUser) != 0)) continue;
	  if ((*(ex->frnode) != '*') &&
	      (strcasecmp(ex->frnode,FromNode) != 0)) continue;
	  if ((*(ex->fname) != '*') &&
	      (strcasecmp(ex->fname,fp->FileName) != 0)) continue;
	  if ((*(ex->ftype) != '*') &&
	      (strcasecmp(ex->ftype,fp->FileExt) != 0)) continue;
	  if (((ex->class) != '*') &&
	      (ex->class != fp->JobClass)) continue;
	  if ((*(ex->dist) != '*') &&
	      (strcasecmp(ex->dist,fp->DistName) != 0)) continue;
	  if ((ex->spooltype == Type_SYSIN) && /* No SYSIN job */
	      ((fp->type & F_JOB) != F_JOB)) continue;
	  if ((ex->spooltype == Type_PRINT) &&
	      ((fp->type & F_PRINT) != F_PRINT)) continue;
	  if ((ex->spooltype == Type_PUNCH) &&
	      ((fp->type & F_PUNCH) != F_PUNCH)) continue;
	      

	  /* Found actions! */
	  if (*(ex->touser) != '*') /* Non-wild user -> pseudoUID */
	    *ExplicitePseudoUser = 1;

	  ex->UsageCnt += 1;	/* count the usage */
	  return ex;
	}
	return &DefaultFileExit;  /* Didn't find specials, use default */
}


/* ----------------------------------------------------------------
 *  Basic entrypoint.
 *
 * When recipient is  REAL USER,  rename for unique name is attempted,
 * and once it is done (even somehow),  his UID is set for that file.
 * GID isn't set to anything especial, and we can't say what it is.
 * Files protection is 0600, thus only UID matters.
 *
 */

static void	_inform_filearrival __(( const char *path, struct FILE_PARAMS *FileParams, char *OutLine, const int transitflag ));

void
inform_filearrival( path, FileParams, OutputLine )
     const char *path;
     struct FILE_PARAMS *FileParams;
     char *OutputLine;
{
	_inform_filearrival (path, FileParams, OutputLine, 0);
}

void
transit_trap( path, FileParams, OutputLine )
     const char *path;
     struct FILE_PARAMS *FileParams;
     char *OutputLine;
{
	_inform_filearrival (path, FileParams, OutputLine, 1);
}

static void
_inform_filearrival( path, FileParams, OutputLine, transitflag )
     const char *path;
     struct FILE_PARAMS *FileParams;
     char *OutputLine;
     const int transitflag;
{
	struct FILE_EXIT_CONFIG *Action;
	int ExplicitePseudoUser,RealUser = 0,RealUID = -1;
	char ToUser[20];
	char HomeDir[256];
	char FilePath[512];
	char From[40];
	char TimeString[30];
	char *Argvp[20];
	char Argv[2048];
	char runprog[250];
	char *p, *args = "";
	int pid, i;
	int MaxFiles = 9900;	/* Have this to be a configurable parameter! */
	static char *Envp[3] = {"IFS= \t\n",NULL,NULL};

	logger(2,"UNIX_FILES: inform_filearrival('%s',From:'%s',To:'%s',Fn:'%s',Ft:'%s',Fc:'%c')\n",
	       path,FileParams->From,FileParams->To,FileParams->FileName,
	       FileParams->FileExt,FileParams->JobClass);

	if (!transitflag)
	  /* Normal exits.. */
	  Action = AnalyzeFileExitAction(FileExitTable,FileParams,
					 &ExplicitePseudoUser,
					 &RealUser,&RealUID,
					 ToUser,HomeDir);
	else
	  /* Files in transit */
	  Action = AnalyzeFileExitAction(TransitExitTable,FileParams,
					 &ExplicitePseudoUser,
					 &RealUser,&RealUID,
					 ToUser,HomeDir);

	if (Action == NULL) {
	  if (transitflag) return; /* No nonsense, if default routing is ok */
	  sprintf(OutputLine,"Got your file, no processings configured to do...BUG HERE! from=%s, to=%s, RealUser=%d, ToUser=%s",
		  FileParams->From,FileParams->To,RealUser,ToUser);
	  logger(1,"UNIX_FILES: %s\n",OutputLine);
	  return;
	}

	logger(2,"UNIX_FILES: Arrived file action: %s\n",
	       ExitActionStrs[Action->Action]);

	*TimeString = 0;
	if (FileParams->NJHtime != 0) {
	  struct tm *tm_var = localtime(&FileParams->NJHtime);
	  strftime(TimeString,sizeof(TimeString)," %D %T %Z",tm_var);
	}

	p = strchr(FileParams->From,'@');
	if (!p) strcpy(From,FileParams->From);
	else {
	  *p = 0;
	  sprintf(From,"%s(%s)",p+1,FileParams->From);
	  *p = '@';
	}

	if (Action->Action == Act_DISCARD) {
	  unlink( FileParams->SpoolFileName ); /* Delete it */
	  /* Here: It worked! */
	  if (*ToUser == 0) strcpy(ToUser,"SYSTEM");
	  sprintf(OutputLine,"FILE (%04ld) to %s deleted -- origin %s%s",
		  FileParams->FileId,ToUser,From,TimeString );

	  logger(2,"UNIX_FILES: inform_filearrival(): %s\n",OutputLine);
	  return;
	}

	sprintf(OutputLine,"FILE (%04ld) spooled to %s -- origin %s%s",
		FileParams->FileId,ToUser,From,TimeString );

	/* Ok, all prepared to go.. */

	strcpy( FilePath,Action->SpoolDir );
	if ((strcmp(FilePath,"default")==0) ||
	    (strcmp(FilePath,".")==0) ||
	    (*FilePath == 0))
	  strcpy( FilePath, DefaultSpoolDir );

	/* If not a real user, and not even an explicite pseudo-ID, drop
	   it to the Postmaster! */
	if (!RealUser && !ExplicitePseudoUser) {
	  strcpy( FilePath, DefaultPOSTMASTdir );
	  if (*ToUser == 0) strcpy(ToUser,"(null)");
	  sprintf(OutputLine,
		  "FILE (%04ld) to %s spooled to POSTMAST -- origin %s%s",
		  FileParams->FileId,ToUser,From,TimeString );
	  strcpy( ToUser, "POSTMAST" );
	}
	if (ExplicitePseudoUser && *ToUser == 0) {
	  sprintf(OutputLine,
		  "FILE (%04ld) spooled to SYSTEM -- origin %s%s",
		  FileParams->FileId,From,TimeString );
	  strcpy(ToUser,"*NJE*");
	}

	if (!RenameToUniqueFileid( FileParams,FilePath,ToUser,HomeDir,MaxFiles,RealUID )) {
	  /* Failed on expansion... */
	  sprintf(OutputLine,"Got troubles on receiving file from=%s, to=%s, given to POSTMASTer",
		  FileParams->From,FileParams->To);
	  logger(1,"UNIX_FILES: %s\n",OutputLine);
	  return;
	}

	/* Lets make sure it has quite restrictive protection.. */
	chmod( FileParams->SpoolFileName,0600 );
	if ( RealUser ) {
	  /* User is real, let's set his file with his UID .. */
	  chown(FileParams->SpoolFileName,RealUID,-1);
	  strcpy(Argv,FileParams->SpoolFileName);
	  p = strrchr(Argv,'/');
	  if (p) { /* Can't fail actually! -- 'famous last words..' */
	    struct stat stats;
	    *p = 0;
	    stat(Argv,&stats);
	    if (stats.st_uid != RealUID && RealUID > 0) {
	      chown(Argv,RealUID,-1); /* Make sure user owns this directory
					 and thus can access files  */
	      chmod(Argv,(stats.st_mode & 0755));
	    }
	  }
	} else if ( ExplicitePseudoUser ) {
	  /* If it is a pseudo-user, alter file owner to
	     be the same as spool directory owner.	 */
	  struct stat stats;
	  stats.st_uid = -1;
	  p = strrchr(Argv,'/');
	  if (p) {
	    *p = 0;
	    stats.st_uid = 0; /* Init it to be root, if nothing else */
	    stat(Argv,&stats);
	    *p = '/';
	    chown(Argv,stats.st_uid,stats.st_gid);
	  }
	  logger(2,"UNIX_FILES: pseudo-user, directory UID=%d\n",stats.st_uid);
	}

	switch (Action->Action) {
	  /* Act_DISCARD is handled before.. */

	  case Act_KEEP:
	      if (RealUser || ExplicitePseudoUser) {
		logger(2,"UNIX_FILES: inform_filearrival(): %s\n",OutputLine);
		/* Inform local user also... */
		if (!RealUser) return; /* ...if there is one. */
		strcat(ToUser,"@");
		strcat(ToUser,LOCAL_NAME);
		sprintf(From,"@%s",LOCAL_NAME);
		logger(3,"UNIX_FILES: Send_nmr(%s,%s,\"%s\",ASCII,CMD_MSG)\n",
		       From,ToUser,OutputLine);
		send_nmr(From,ToUser,
			 OutputLine,strlen(OutputLine),
			 ASCII,CMD_MSG);
	      } else {
		logger(2,"UNIX_FILES: #### inform_filearrival() -- Not real user, nor pseudo user! ??\n");
	      }
	      return;

	  case Act_RUN:
	      p = Action->ArgTemplate;
	      args = p;
	      while (*args != 0 && *args != ' ' && *args != '\t') ++args;
	      strncpy(runprog,p,args-p);
	      runprog[args-p] = 0;
	      p = runprog;
	      while (*args != 0 && (*args == ' ' || *args == '\t')) ++args;

	      logger(2,"UNIX_FILES: RUN: %s %s [%s]\n",
		       p,FileParams->SpoolFileName,args);
		
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
		/* XXX: Actually we should put child monitoring data
		   somewhere... */
	      } else {

		/* Actual location of file is found from:
		   FileParams->SpoolFileName */
		/*
		   Programs are executed with following ARGV:
		   0  =  ProgPath
		   1  =  FilePath
		   [2..=  args]
		   */
		strcpy( Argv,p );
		Argvp[0] = p = Argv;
		p = p + strlen(p); *++p = 0;
		Argvp[1] = p;
		strcpy( p,FileParams->SpoolFileName );
		p = p + strlen(p); *++p = 0;
		i = 2;		/* HAN: Argvp[1] points to file path ... */
		if (*args != 0) {
		  int had_spool = 0;
		  i = 1;	/* If we have args, we may have the $SPOOL
				   also.. */
		  p = Argvp[1]; /* pull back.. */
		  while (*args != 0) {
		    while (*args != 0 &&
			   (*args == ' ' || *args == '\t')) ++args;
		    Argvp[i++] = p;
		    while (*args != 0 && *args != ' ' && *args != '\t') {
		      if (*args == '$') {
			/*  Macroexpansion in here, a'la: $TOUSER@$TONODE  */
			char macname[20];
			char *q = macname;
			int k = 9;
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
			  goto mac_expand;
			}
			while (*args && k > 0 &&
			       ((matchch != 0 && *args != matchch) ||
				(matchch == 0 &&
				 ((unsigned)*args >= (unsigned)'A' &&
				  (unsigned)*args <= (unsigned)'Z')))) {
			  *q++ = *args++;
			  --k;
			}
			*q = 0;
			/*logger(2,"UNIX_FILES: Matching macro var: `%s'\n",
			  macname);*/
		    mac_expand:

			q = "";
			/* Ok, now we have the magic token: uppercase string */
			if (*macname == '$') {
			  sprintf(macname,"%d",(int)getpid());
			  q = macname;
			} else if (strcmp(macname,"SPOOL")==0) {
			  q = FileParams->SpoolFileName;
			  had_spool = 1;
			} else if (strcmp(macname,"TOUSER")==0) {
			  strcpy(macname,FileParams->To);
			  if ((q = strchr(macname,'@')) != NULL) *q = 0;
			  q = macname;
			} else if (strcmp(macname,"TONODE")==0) {
			  strcpy(macname,FileParams->To);
			  if ((q = strchr(macname,'@')) != NULL) *q = 0;
			  else q = macname-1;
			  ++q;
			} else if (strcmp(macname,"FNAME")==0) {
			  q = FileParams->FileName;
			} else if (strcmp(macname,"FTYPE")==0) {
			  q = FileParams->FileExt;
			} else if (strcmp(macname,"CLASS")==0) {
			  q = macname;
			  macname[0] = FileParams->JobClass;
			  macname[1] = 0;
			} else if (strcmp(macname,"FID")==0) {
			  sprintf(macname,"%04ld",FileParams->FileId);
			  q = macname;
			} else if (strcmp(macname,"FRUSER")==0) {
			  strcpy(macname,FileParams->From);
			  if ((q = strchr(macname,'@')) != NULL) *q = 0;
			  q = macname;
			} else if (strcmp(macname,"FRNODE")==0) {
			  strcpy(macname,FileParams->From);
			  if ((q = strchr(macname,'@')) != NULL) *q = 0;
			  else q = macname-1;
			  ++q;
			} else if (strcmp(macname,"TAG")==0) {
			  q = FileParams->tag;
			} else if (strcmp(macname,"SPTYPE")==0) {
			  if (FileParams->type & F_JOB)
			    q = "SYSIN";
			  else if (FileParams->type & F_PRINT)
			    q = "PRINT";
			  else
			    q = "PUNCH";
			} else if (strcmp(macname,"DIST")==0) {
			  q = FileParams->DistName;
			} /* Else what ?? */
			/* Copy the string in */
			/*logger(2,"UNIX_FILES: Copying in string `%s'\n",q);*/
			while (*q)
			  *p++ = *q++;
		      } else	/* else not started a $-macro */
			*p++ = *args++;
		    } /* end - while processing a token within ArgTemplate */
		    *p++ = 0;
		    logger(2,"UNIX_FILES: Expanded argv[%d] to be \"%s\"\n",
			   i-1,Argvp[i-1]);
		  } /* end - while had ArgTemplate string */
		  if (!had_spool) {
		    logger(1,"UNIX_FILES: No $SPOOL in the argument pattern: `%s'\n",
			   Action->ArgTemplate);
		  }
		}
		Argvp[i] = NULL;
		{
		  int dtbls = GETDTABLESIZE(NULL);
		  for (pid=0; pid < dtbls; ++pid) close(pid);
		}
		execve(Argvp[0],Argvp,Envp);
		/* Right...  IF execve() returns -> Things are BAD! */
		_exit(-errno); /* Handles are closed, we do NOT flush here.*/
	      }
	      if (RealUser || ExplicitePseudoUser) {
		logger(2,"UNIX_FILES: inform_filearrival(): %s\n",
		       OutputLine);
	      } else {
		logger(1,"UNIX_FILES: #### inform_filearrival() -- Not real user, nor pseudo user! ??\n");
	      }
	      return;

	  case Act_NOTIFY:
	      /* Send message to given user@node */
	      if (RealUser || ExplicitePseudoUser) {
		logger(2,"UNIX_FILES: inform_filearrival(): %s\n",
		       OutputLine);
		/* Send message to given user@node */
		/* Action->Path -- This points to message recipient */
		sprintf(From,"@%s",LOCAL_NAME);
		send_nmr(From, Action->ArgTemplate,
			 OutputLine, strlen(OutputLine),
			 ASCII, CMD_MSG );
	      } else {
		logger(1,"UNIX_FILES: #### inform_filearrival() -- Not real user, nor pseudo user! ??\n");
	      }
	      return;

	  default:
	      break;
	}
	
}


/* ----------------------------------------------------------------

    READ_EXIT_CONFIG( path )

    Routine to read configuration file which is given with  'char *path'
    argument.
    (Note: MUST HAVE  POSTMAST  user !)

    Format is:

    # comment lines
					(Also completely whites are comment)

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
    # To do keywords:	DISCARD	to /dev/null.
    #			KEEP	just so.  Into default or given spool.
    #			RUN	starts arbitary program with arbitary
    #				arguments telling about file location
    #				and its properties.
    #				If fails, well..
    #			NOTIFY	send NJE message to someone.
    # Exit table begin keyword:

    Exit-Table:

    # Args:
    # touser8 tonode8 fname8   ftype8  pun? class   fruser8  frnode8  dist8  SpoolDir            action  ExtraArgument
    MAILER   LOCAL    *        *        PUN *        *        *        *     /usr/spool/bitmail/ RUN /usr/local/lib/huji/mailify $SPOOL
    NOBODY   LOCAL    *        *        *   *        *        *        *     default             DISCARD
    FOOBAT   LOCAL    *        *        *   *        *        *        *     default             RUN /usr/local/lib/huji/transfer touser@whatnode $SPOOL
    *        *        *        *        *   *        *        *        *     default             KEEP

    ...
    <eof>

 Programs are executed with following ARGV:
  0  =  ProgPath
 [1.. = args]

   ---------------------------------------------------------------- */

char *
ReadNoncommentLine( fil,buf,bufsiz,linenump )
     FILE **fil;
     char *buf;
     const int bufsiz;
     int *linenump;
{
	char *s;

	if (*fil == NULL) return NULL;
	while (!feof(*fil) && !ferror(*fil)) {
	  if (fgets(buf,bufsiz,*fil) == NULL) break;
	  *linenump += 1;
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

/* struct FILE_EXIT_CONFIG * */
void *
ReallocTable( oldtbl, eltsize, newsize )
void *oldtbl;
int eltsize, newsize; /* recs, not bytes */
{
	void *tmp = oldtbl;
	char *msg1="##### UNIX_FILES, malloc() for exit config table failed!";
	char *msg2="##### UNIX_FILES, realloc() for exit config table failed!";

	if (tmp == NULL) {
	  tmp = (void *) malloc ((newsize + 1) * eltsize);
	  if (tmp == NULL) {
	    logger (1,"%s\n",msg1);
	    send_opcom(msg1);
	    return NULL;
	  }
	} else {
	  /* Original pointer isn't NULL, realloc in question.. */
	  tmp = (void *) realloc ( oldtbl,(newsize+1)*eltsize);
	  if (tmp == NULL) {
	    logger (1,"%s\n",msg2);
	    send_opcom(msg2);
	    return NULL;
	  }
	}
	return tmp;
}

int
read_exit_config( path )
     const char *path;
{
	int entries = 0, tablesize = 0;
	struct FILE_EXIT_CONFIG *NewExitTable = NULL, *New;
	/* struct FILE_EXIT_CONFIG **CurrentTable = &FileExitTable */
	char line[500], *linep; /* Just something long... */
	char key[30];
	char spooltype[8];
	char arg[500]; /* hmm... */
	int scans, i, linenum = 0;
	FILE *cfg = fopen(path,"r");

	DefaultSpoolDir       [sizeof(DefaultSpoolDir)       -1] = 0;
	DefaultPOSTMASTdir    [sizeof(DefaultPOSTMASTdir)    -1] = 0;

	if (cfg == NULL) {
	  /* Config reading failed, we leave defaults or old setups - whatever.. */
	  logger(1,"UNIX_FILES: file open of cfg-file failed...\n");
	  return -1;
	}
	

	while (1) {

	  if (ReadNoncommentLine( &cfg,line,sizeof line,&linenum ) == NULL)
	    break;
	  /* Didn't get line for keywords.. */
	  if ((scans = sscanf(line,"%29s %s",key,arg)) < 1) break;
	  if (strcasecmp("SPOOL-DIR:",key)==0) {
	    if (scans < 2) continue;
	    else strncpy(DefaultSpoolDir,arg,
			 sizeof(DefaultSpoolDir)-1);
	  } else if (strcasecmp("POSTMAST-DIR:",key)==0) {
	    if (scans < 2) continue;
	    else strncpy(DefaultPOSTMASTdir,arg,
			 sizeof(DefaultPOSTMASTdir)-1);
	  } else if (strcasecmp("EXIT-TABLE:",key)==0) {
	    break;
	  } else {
	    sprintf(line,"UNIX_FILES, ###### While scanning exit table got bad key '%s'",key);
	    logger(1,"%s\n",line);
	    send_opcom(line);
	    fclose(cfg); cfg = NULL;
	    break;
	  }
	}

	/* Parse actual exits ! */

	while (!feof(cfg) && !ferror(cfg)) {
	  if (ReadNoncommentLine( &cfg,line,sizeof line,&linenum ) == NULL)
	    break;
	  if (entries == tablesize) {
	    if ((NewExitTable = ReallocTable( NewExitTable,
					     sizeof (struct FILE_EXIT_CONFIG),
					     tablesize+10 ))!=NULL) {
	      tablesize += 10;
	      NewExitTable[tablesize].touser[0] = 0; /* Mark the last of
							the array... */
	    } else {
	      fclose(cfg);
	      logger(1,"UNIX_FILES: table ReAlloc failed, exiting...\n");
	      return -3; /* Table malloc failing */
	    }
	  }
	  New = &NewExitTable[entries];
/* # touser8 tonode8 fname8   ftype8  pun? class fruser8  frnode8  dist8   action   SpoolDir  ExtraArguments */
	  scans = 0;
	  linep = line;
	  for (i = 0; i < 11; ++i) {
	    /* scans = sscanf(line,"%s %s %s %s %s %c %s %s %s %s %s", */
				/*  01 02 03 04 05 06 07 08 09 10 11   */
	    static int maxlens[] = { sizeof(New->touser), sizeof(New->tonode),
				       sizeof(New->fname), sizeof(New->ftype),
				       sizeof(spooltype), 2,
				       sizeof(New->fruser),
				       sizeof(New->frnode),
				       sizeof(New->dist),
				       sizeof(New->SpoolDir),
				       sizeof(key) };
	    static char *vars[11];
	    int  maxlen = maxlens[i];
	    char *p;

	    if (i == 0) {
	      vars[0] = New->touser;
	      vars[1] = New->tonode;
	      vars[2] = New->fname;
	      vars[3] = New->ftype;
	      vars[4] = spooltype;
	      vars[5] = &New->class;
	      vars[6] = New->fruser;
	      vars[7] = New->frnode;
	      vars[8] = New->dist;
	      vars[9] = New->SpoolDir;
	      vars[10] = key;
	    }
	    p = vars[i];

	    while (*linep == ' ' || *linep == '\t') ++linep;

	    /* Count how many string tokens we have */
	    if (*linep != '\0') scans = i+1;

	    while (*linep != '\0' && *linep != ' ' && *linep != '\t')
	      if (--maxlen > 0) /* Store while there is space */
		*p++ = *linep++;
	      else
		++linep;
	    *p = '\0';
	  }
	  while (*linep == ' ' || *linep == '\t') ++linep;
	  strncpy(New->ArgTemplate, linep, sizeof(New->ArgTemplate));
	  New->ArgTemplate[sizeof(New->ArgTemplate)-1] = 0;
	  if (*linep != '\0') ++scans;

	  if (scans < 11) {
	    logger (1,"UNIX_FILES, scanning exit table got bad exit line. Too few datas (%d, min 11) on line:%d\n",scans,linenum);
	    send_opcom("UNIX_FILES, scanning exit table got bad exit line. See log for details.");
	    New->touser[0] = 0;
	    continue;
	  }

	  if (strcasecmp(spooltype,"SYSIN")==0)
	    New->spooltype = Type_SYSIN;
	  else if (strcasecmp(spooltype,"PRINT")==0)
	    New->spooltype = Type_PRINT;
	  else if (strcasecmp(spooltype,"PRT")==0)
	    New->spooltype = Type_PRINT;
	  else if (strcasecmp(spooltype,"PUNCH")==0)
	    New->spooltype = Type_PUNCH;
	  else if (strcasecmp(spooltype,"PUN")==0)
	    New->spooltype = Type_PUNCH;
	  else if (*spooltype == '*')
	    New->spooltype = Type_ANY;
	  else {
	    logger (1,"UNIX_FILES: scanning file exit table, and got bad spooltype spec: `%s' on input line %d\n",spooltype,linenum);
	    continue;
	  }
	  
	  if (strcasecmp("DISCARD",key)==0)
	    New->Action = Act_DISCARD;
	  else if (strcasecmp("KEEP",key)==0)
	    New->Action = Act_KEEP;
	  else if (strcasecmp("RUN",key)==0)
	    New->Action = Act_RUN;
	  else if (strcasecmp("NOTIFY",key)==0)
	    New->Action = Act_NOTIFY;
	  else
	    New->Action = 99;
	  switch (New->Action) {
	    case Act_DISCARD:
		break;  /* No NEED for extra arguments */
	    case Act_RUN:
		if (scans < 12) {
		  logger (1,"UNIX_FILES, scanning exit table got bad exit line. Too few datas (%d, min 12) on line: %d\n",scans,linenum);
		  send_opcom("UNIX_FILES, scanning exit table got bad exit line. See log for details.");
		  New->touser[0] = 0;
		  continue;
		}
	    case Act_KEEP:
		/* If not valid, forget this episode. */
		if (scans < 11) {
		  logger (1,"UNIX_FILES, scanning exit table got bad exit line. Too few datas: (%d, min 11) on line: %d\n",scans,linenum);
		  send_opcom("UNIX_FILES, scanning exit table got bad exit line. See log for details.");
		  New->touser[0] = 0;
		  continue;
		}
		if (Check_File_Exit_Validity(New,1)==0) {
		  logger (1,"UNIX_FILES, scanning exit table got bad exit line. Validity failed on line: %d\n",linenum);
		  send_opcom("UNIX_FILES, scanning exit table got bad exit line. See log for details.");
		  New->touser[0] = 0;
		  continue;
		}
		break;
		/* Great! Propably it is valid! */
	    default:
		logger (1,"UNIX_FILES, scanning exit table got bad exit line. Got bad exit type. Key: '%s', on line: %d\n",key,linenum);
		send_opcom("UNIX_FILES, scanning exit table got bad exit line. See log for details.");
		New->touser[0] = 0;
		continue;
	  } /* switch() */

	  /* XXX: Convert case insensitive fields to Lowercase ? UPPERCASE ? */

	  /* We accept this entry, validity test accepted it anyway. */
	  ++entries;
	  
	}
	NewExitTable[entries].touser[0] = 0; /* Mark last... */

	if (NewExitTable != NULL && NewExitTable->touser[0] != 0) {
	  New = FileExitTable;
	  FileExitTable = NewExitTable;
	  free (New);
	}
	if (cfg)
	  fclose(cfg);
	logger(1,"UNIX_FILES: new exit table (%s) scanned in.\n", path);
	return 0;
}
