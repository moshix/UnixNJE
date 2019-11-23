/*----------------------------------------------------------------------*
 |		QRDR		Query user incoming queue		|
 |	For FUNET-NJE by Matti Aarnio  <mea@nic.funet.fi>		|
 |									|
 |  Options:								|
 |	-u user		(default is invoker)				|
 |	-n		(give return code != 0 if pending files)	|
 |      -l		(list original filenames in front if other data,|
 |                       and file arrival date (UNIX ctime) to spool.)  |
 |	dirpath		(point to directory to search)			|
 |	filepath	(point to file to search)			|
 |									|
 |  Default:	Show short listing of files in my (invoker) rdr queue	|
 |									|
 |									|
 *----------------------------------------------------------------------*/

#include "consts.h" /* DIR & DIRENTTYPE definitions.. */
#include "prototypes.h"
#include <ctype.h>
#include "clientutils.h"

char LOCAL_NAME   [10];
char BITNET_QUEUE [80];
char LOG_FILE     [80] = "-";

int LogLevel = 2;
FILE *LogFd = NULL;

extern char DefaultSpoolDir[256];
extern char DefaultPOSTMASTdir[256];

extern	int	dump_header __(( char const *path, char const *dirpath, const time_t mtime ));
extern	void	usage __(( void ));

char	WhoAmI[10] = "";
char	WhoAmIl[10];	/* Lowercase copy.. */
char	StudyDir[1024];
char	delayprint[1024];
char	*pname = "<QRDR>";

int	BeQuiet = 0;	/* -n gives only non-zero RC if files in queue */
int	BeQuiet2 = 0;	/* -N gives non-zero RC if files in queue, and
			   prints `You have (no) bitnet files in queue' */
int	FilesFound = 0;
int	LongList = 0;	/* -l prints also filenames in front of other data */
int	Debug = 0;

struct namedate {
	char	*name;
	time_t	mtime;
};

static int cmpnamedate(p1,p2)
const struct namedate *p1, *p2;
{
  return (p1->mtime) - (p2->mtime);
}

static int
study_dir(dirpath)
char *dirpath;
{
	DIR	*dirfile;
	DIRENTTYPE *dirp;
	struct stat fstat;
	char curdir[MAXPATHLEN];
	struct namedate *namedates = NULL;
	int entryspace = 0;
	int entrycount = 0;
	int rc;

#if	defined(USG)
	getcwd(curdir,sizeof(curdir));
#else
	getwd(curdir);
#endif
	dirfile = opendir(dirpath);
	if (dirfile == NULL) {
	  if (!BeQuiet && !BeQuiet2) {
	    if (*delayprint)
	      puts(delayprint);
	    *delayprint = 0;
	    printf("QRDR: Can't open directory `%s'\n",dirpath);
	  }
	  return 0;
	}
	rc = 0;
	chdir(dirpath);
	for (dirp = readdir(dirfile);
	     dirp != NULL;
	     dirp = readdir(dirfile)) {
	  if (stat(dirp->d_name,&fstat) != 0)
	    continue;
	  /* printf("QRDR: dirfile: `%s'\n",dirp->d_name); */
	  /* if (strcmp(dirp->d_name,".spoolid")== 0)
	     continue; */ /* old, obsolete.. */
	  if ((fstat.st_mode & S_IFMT) == S_IFREG) {
	    if (entryspace == 0) {
	      entryspace  = 8;
	      namedates = (struct namedate*)malloc(sizeof(struct namedate) *
						   entryspace);
	    } else if (entrycount >= entryspace) {
	      entryspace += 8;
	      namedates = (struct namedate*)realloc(namedates,
						    sizeof(struct namedate) *
						    entryspace);
	    }
	    namedates[entrycount].name = strdup(dirp->d_name);
	    namedates[entrycount].mtime = fstat.st_mtime;
	    ++entrycount;
	  }
	}
	if (entrycount > 0) {
	  int i;
	  qsort(namedates,entrycount,sizeof(struct namedate),cmpnamedate);
	  for (i = 0; i < entrycount; ++i) {
	    rc |= dump_header(namedates[i].name, dirpath, namedates[i].mtime);
	    free(namedates[i].name);
	  }
	  free(namedates);
	}
	closedir(dirfile);
	chdir(curdir);
	if (BeQuiet2 & FilesFound) {
	  printf("You have BITNET files waiting! Use  qrdr -l  to check them!\n");
	  exit(1);	/* Signal founding files... */
	}
	if (BeQuiet & FilesFound)
	  exit(1);	/* Signal founding files... */
	return rc;
}


int
main(argc,argv)
     int argc;
     char *argv[];
{
	struct passwd *passwdent;
	char *p;
	int c;
	char HomeDir[256];
	struct stat fstat;
	int UseDefault = 0;
	extern char *optarg;
	extern int optind;

	pname = *argv;

	*WhoAmI = 0;
	*StudyDir = 0;

	read_configuration();

	/* Process options here! */

	while ((c = getopt(argc, argv, "u:DnNl?")) != -1) {
	  switch (c) {
	    case 'u':
		strncpy(WhoAmI,optarg,8);
		break;
	    case 'D':
		Debug = 1;
		break;
	    case 'N':
		BeQuiet2 = 1;
		BeQuiet = 1;
		break;
	    case 'n':
		BeQuiet = 1;
		break;
	    case 'l':
		LongList = 1;
		break;
	    case '?':
	        usage();
		break;
	    default:
	        usage();
		break;
	  }
	}

	argv += optind;
	argc -= optind;

	if (*WhoAmI == 0) {
	  WhoAmI[8] = 0;
	  p = getenv("LOGNAME");  /* BSD/SunOS/... alike */
	  if (!p) {
	    p = getenv("USER");	/* SysV */
	    if (!p) {
	      passwdent = getpwuid(getuid());
	      if (passwdent)
		strncpy(WhoAmI,passwdent->pw_name,8);
	      else {
		fprintf(stderr,"QRDR: Can't figure out who I am.\n");
		exit(2);
	      }
	    } else
	      strncpy(WhoAmI,p,8);
	  } else
	    strncpy(WhoAmI,p,8);
	}
	strcpy(WhoAmIl,WhoAmI);
	upperstr(WhoAmI);
	    
	if (*StudyDir == 0) {
	  strcpy(StudyDir,DefaultSpoolDir);
	  if ((passwdent = getpwnam( WhoAmIl )) != NULL) {
	    strcpy(HomeDir,passwdent->pw_dir);
	  } else {
	    if (!(BeQuiet || LongList))
	    logger(1,"QRDR: Can't figure home dir of `%s' -- PseudoUser ?\n",
		   WhoAmI);
	    strcpy(HomeDir,"/");
	  }
	  ExpandHomeDir(DefaultSpoolDir,HomeDir,WhoAmI,StudyDir);
	  UseDefault = 1;
	}
	
	*delayprint = 0;
	if (!(BeQuiet || LongList))
	  sprintf(delayprint,"QRDR: Spool dir: `%s'",StudyDir);

	if (!*argv)
	  study_dir(StudyDir);
	else
	  while (*argv) {
	    if (stat(*argv,&fstat) != 0) {
	      if (!UseDefault) { /* Default: Don't worry.. */
		if (*delayprint)
		  puts(delayprint);
		*delayprint = 0;
		printf("QRDR: `%s' is nonexistent!  Can't stat() it.\n",*argv);
	      }
	      ++argv;
	      continue;			/* Bad name ? Skip it */
	    }
	    if ((fstat.st_mode & S_IFMT) == S_IFREG)
	      dump_header(*argv,"",fstat.st_mtime);
	    else if ((fstat.st_mode & S_IFMT) == S_IFDIR)
	      study_dir(*argv);
	    else {
	      if (*delayprint)
		puts(delayprint);
	      *delayprint = 0;
	      printf("QRDR: `%s' Not regular file, or directory!\n",*argv);
	      ++argv;
	      continue;
	    }
	    ++argv;
	  }
	if (BeQuiet2) {
	  if (FilesFound == 0)
	    printf("QRDR: You have no BITNET files waiting.\n");
	  else
	    printf("QRDR: Found %d BITNET file%s waiting for you.\n",
		   FilesFound,(FilesFound==1) ? "":"s" );
	  return (FilesFound != 0);
	}

	if (BeQuiet)
	  return (FilesFound != 0);	/* Signal founding files... */

	if (FilesFound > 0 && !LongList) {
	  if (*delayprint)
	    puts(delayprint);
	  *delayprint = 0;
	  printf("Found %d file%s.\n",FilesFound,(FilesFound==1) ? "":"s" );
	} else if (!LongList) {
	  if (*delayprint)
	    puts(delayprint);
	  *delayprint = 0;
	  printf("No files found.\n");
	}
	return 0;
}

void
Type2Str(type,str)
     const int type;
     char *str;
{
	if (type & F_PRINT)
	  strcpy(str,"PRT");
	else if (type & F_JOB)
	  strcpy(str,"JOB");
	else
	  strcpy(str,"PUN");
}

static	unsigned char INMR01[7] = {0xE0,0xC9,0xD5,0xD4,0xD9,0xF0,0xF1};
static	unsigned char CORNELLCARD[8] =
	{ 0x7A,0xC3,0xC1,0xD9,0xC4,0x40,0x40,0x40 };
static  unsigned char VMSDUMP[12] =
	{ 0xFF,0x0D,0x00,'V','M','S','D','U','M','P',' ','V' };

int
dump_header(path,dirpath,mtime)
     char const *path;
     char const *dirpath;
     const time_t mtime;
{
	int	rc;
	char	FileName[20], FileExt[20], From[20], To[20], Class;
	char	Form[20];
	char	TypeStr[20];
	char	Tag[140];
	char	*ContentsType;
	int	Type, FileId, Format;
	FILE	*fd;
	static	int	once = 1;
	int	FilePosition;
	unsigned char uline[1000];
	char	const *SpoolID = strrchr(path,'/');

	if (SpoolID) ++SpoolID;
	else	       SpoolID = path; 

	if (!(fd = fopen(path,"r"))) return -1;

	rc = parse_header(fd, From, To, FileName, FileExt, &Type,
			  &Class, Form, &Format, Tag, &FileId);

	if (Debug) printf("parse_header(%s) rc=%d\n",path,rc);

	FilePosition = ftell(fd);	/* Save the beginning of real message*/

	/* Lets see if it is NETDATA... */
	while (1) {
	  if (Uread(uline,sizeof uline,fd) < 1) break;
	  if (*uline == 0x80) break; /* PUNCH.. */
	  if (*uline == 0x90) break; /* PRINT */
	  if (*uline == 0xA0) break; /* PASA  */
	  if (*uline == 0xB0) break; /* PCPDS */
	}

	fseek(fd, FilePosition, 0);  /* Get back to beginning of real text */
	ContentsType = "*FAULT*";
	if (*uline == 0x80) {	/* virtual card record */
	  if (memcmp(uline+3,INMR01,7)==0) {
	    ContentsType = "NETDATA";
	  } else if (memcmp(uline+2,VMSDUMP,12)==0) {
	    ContentsType = "VMSDUMP";
	  } else if (memcmp(uline+2,CORNELLCARD,8)==0) {
	    ContentsType = "CARD";
	  } else
	    ContentsType = "PUNCH";
	} else if (*uline == 0x90) { /* PRINTs.. */
	  ContentsType = "PRINT";
	} else if (*uline == 0xA0) { /* PRINTs.. */
	  ContentsType = "PASA";
	} else if (*uline == 0xB0) { /* PRINTs.. */
	  ContentsType = "PCPDS";
	}

	fclose(fd);

	if (rc != 0) {
	  if (*delayprint)
	    puts(delayprint);
	  *delayprint = 0;
	  printf("File: `%s' is NOT FUNET-NJE Spool file!\n",path);
	  return 1;
	}

	FilesFound++;

	Type2Str(Type,TypeStr);

	if (BeQuiet)
	  return 0;

	if (SpoolID) {
	  if (!(SpoolID[0] >= '0' && SpoolID[0] <= '9' &&
		SpoolID[1] >= '0' && SpoolID[1] <= '9' &&
		SpoolID[2] >= '0' && SpoolID[2] <= '9' &&
		SpoolID[3] >= '0' && SpoolID[3] <= '9' &&
		SpoolID[4] == 0))
	    SpoolID = "??";
	}

	/* Print header line, if no long-list.. */
	if (once && !LongList) {
	  if (*delayprint)
	    puts(delayprint);
	  *delayprint = 0;
	  printf("%-17s %-17s %-8s %-8s %4s  %-8s %s\n",
		 "From:","To:","FName:","FExt:","Type","Form:",
		 "SpoolID");
	  once = 0;
	}
	if (*delayprint)
	  puts(delayprint);
	*delayprint = 0;
	if (LongList) {
	  int len = strlen(dirpath);
	  char last = len ? dirpath[len-1] : 0;
	  char cdatestr[20];
	  struct tm *tp = gmtime(&mtime);
	  sprintf(cdatestr,"%04d%02d%02d%02d%02d%02d",
		  tp->tm_year+1900,tp->tm_mon+1,tp->tm_mday,
		  tp->tm_hour,tp->tm_min,tp->tm_sec);
	  printf("%s%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%c\t%s\t%s\t%s\n",
		 dirpath, (last=='/') ? "":"/", path,
		 ContentsType,
		 From,To,FileName,FileExt,TypeStr,Class,Form,SpoolID,
		 cdatestr);
	} else
	  printf("%-17s %-17s %-8.8s %-8.8s %-3s %c %-8.8s %s\n",
		 From,To,FileName,FileExt,TypeStr,Class,Form,SpoolID);

	return 0;
}

volatile void
bug_check(str)
     char const *str;
{
	logger(1,"QRDR: BUG_CHECK: %s",str);
	exit(100);
}

void
usage()
{
	fprintf(stderr,"\
QRDR:  Query FUNET-NJE readers and/or files in it.\n\
       -u user (default: invoker)\n\
       -n      (give !=0 rc if files in queue dir.)\n\
       dirpath (Check pointed directory)\n\
       filepath (study pointed file)\n");
	fflush(stderr);
}
