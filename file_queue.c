/* FILE_QUEUE.C   V2.0-mea
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
 | Handle the in-memory file queue. Queue and de-queue files.
 | Currently the host assignment to line (and thus the files queue) is fixed,
 | during the program run. However, the files queueing is dynamic.
 |   When the files are queued from the mailer, they are ordered in ascending
 | size order.
 */

#include "consts.h"
#include "prototypes.h"


#ifdef	HAS_LSTAT	/* lstat(2), or only stat(2)  ?? */
# define  STATFUNC lstat
#else
# define  STATFUNC stat
#endif

static void submit_file __((const int fd, const char *FileName, int FileSize));

int	file_queuer_pipe = -1;
int	file_queuer_cnt  =  0;

#define	HASH_SIZE 503
static struct QUEUE *queue_hashes[HASH_SIZE];
static int hash_inited = 0;

static void
__hash_init()
{
	int i;

	if (hash_inited) return;

	for (i = 0; i < HASH_SIZE; ++i)
	  queue_hashes[i] = NULL;
	hash_inited = 1;
}

static void
hash_insert(Entry)
struct QUEUE *Entry;
{
	int i;

	if (!hash_inited) __hash_init();

	i = Entry->hash % HASH_SIZE;

	Entry->hashnext = queue_hashes[i];
	queue_hashes[i] = Entry;
}

static void
hash_delete(Entry)
struct QUEUE *Entry;
{
	int i, match = 0;
	struct QUEUE *hp1, *hp2;

	if (!hash_inited) __hash_init();

	i = Entry->hash % HASH_SIZE;
	if (queue_hashes[i] == NULL)
	  return;

	hp1 = NULL; /* Predecessor */
	hp2 = queue_hashes[i];
	while (hp2 != NULL) {
	  while (hp2 != NULL && hp2->hash != Entry->hash) {
	    hp1 = hp2; /* predecessor */
	    hp2 = hp2->hashnext;
	  }
	  if (hp2 == NULL) return; /* Nothing to do, quit */
	  if (hp2->hash == Entry->hash) { /* Hash-match,
					     check real match: */
#ifdef	UNIX
	    if (hp2->fstats.st_dev  == Entry->fstats.st_dev &&
		hp2->fstats.st_ino  == Entry->fstats.st_ino &&
		hp2->fstats.st_size == Entry->fstats.st_size)
	      match = 1;
#else
	    match = (strcmp(hp2->FileName,Entry->FileName) == 0);
#endif
	    if (match) break;
	  }
	  hp1 = hp2; /* Predecessor */
	  hp2 = hp2->hashnext;
	}
	if (!match)
	  return; /* No match.. */

	if (hp1 == NULL) /* Match at first */
	  queue_hashes[i] = hp2->hashnext;
	else
	  hp1->hashnext = hp2->hashnext;

	Entry->hashnext = NULL; /* Superfluous, this entry will
				   be deleted next */
}

static struct QUEUE *
hash_find(Entry)
struct QUEUE *Entry;
{
	int i;
	struct QUEUE *hp2;

	if (!hash_inited) __hash_init();

	i = Entry->hash % HASH_SIZE;
	if (queue_hashes[i] == NULL)
	  return NULL;

	hp2 = queue_hashes[i];
	while (hp2 != NULL) {
	  while (hp2 != NULL && hp2->hash != Entry->hash) {
	    hp2 = hp2->hashnext;
	  }
	  if (hp2 == NULL) return NULL; /* Nothing to do, quit */
	  if (hp2->hash == Entry->hash) { /* Hash-match,
					     check real match: */
#ifdef	UNIX
	    if (hp2->fstats.st_dev  == Entry->fstats.st_dev &&
		hp2->fstats.st_ino  == Entry->fstats.st_ino &&
		hp2->fstats.st_size == Entry->fstats.st_size)
	      return hp2;
#else
	    if (strcmp(hp2->FileName,Entry->FileName) == 0)
	      return hp2;
#endif
	  }
	  hp2 = hp2->hashnext;
	}
	return NULL; /* No match.. */
}



/*
 | Zero the queue pointers. Then go over the queue and look for files queued
 | to us. Each file found is queued according to the routing table and
 | alternate routes.
 | When working on a Unix system, we have a problem of telling the Find-File
 | routine whether this is the first call or not (on VMS the value of Context
 | tells it). Thus, on Unix, if FileMask is none-zero, this is the first call.
 | If it points to a Null byte, then this is not the first call, and the
 | find-file routine already has the parameters in its internal variables.
 */
int
init_files_queue()
{
	int	i = -1, pid;
	long	FileSize;
	int	sync_mode = 0;
	int	pipes[2];
	int	filecnt = 0;
	char	FileMask[LINESIZE], FileName[LINESIZE];
#ifdef VMS
	long	context;	/* Context for find file */
#else
	DIR	*context;
#endif

#ifdef	UNIX
	pipes[0]  = -1;
	pid = -1;

	sync_mode = (pipe(pipes) < 0);
	if (!sync_mode) {

	  extern FILE *LogFd;

	  if (file_queuer_pipe >= 0)
	    close(file_queuer_pipe);
#if 0
	  return -1; /* ALREADY RUNNING! */
#endif

	  file_queuer_pipe = pipes[0];
	  file_queuer_cnt = 0;

	  if ((pid = fork()) > 0) {
	    close(pipes[1]);
	    return 0;	/* Parent.. */
	  }

	  /* Child -- detach all except the submit-pipe */
	  if (LogFd == NULL) {
	    for (i = getdtablesize(); i >= 0; --i)
	      if (i != pipes[1])
		close(i);
	  } else {
	    for (i = getdtablesize(); i >= 0; --i)
	      if (i != pipes[1] && i != fileno(LogFd))
		close(i);
	  }
	}
	logger(1,"FILE_QUEUE: %s mode queue initializer running, pid=%d\n",
	       !sync_mode ? "Child":"Sync",getpid());

	if (!sync_mode && pid < 0)	/* Must do in the usual way */
	  sync_mode = 1;
	/* When fork()'s return code is 0, we are a child process,
	   which then goes on and scans all the queue directories,
	   and sends submissions to the main program.  A LOT faster
	   start that way, if the queues are large for some reason.. */
	if (!sync_mode && pid == 0) { /* Child must die on PIPE lossage.. */
	  signal(SIGPIPE,SIG_DFL);
#ifdef	SIGPOLL
	  signal(SIGPOLL,SIG_DFL);
#endif
	  /* No point in closing all unnecessary file descriptors.
	     the final  _exit()  will take care of them. */
	}
#endif


	for (i = 0; i < MAX_LINES; i++) {
	  if (IoLines[i].HostName[0] == 0) continue;

	  /* Scan the queue for files to be sent */
#ifdef VMS
	  sprintf(FileMask, "%sASC_*.*;*", BITNET_QUEUE);
	  context = 0;
#else
	  sprintf(FileMask, "%s/%s/*", BITNET_QUEUE, IoLines[i].HostName);
	  make_dirs(FileMask); /* Make sure the dir exists.. */
#endif

	  while (find_file(FileMask, FileName, &context) == 1) {
	    FileSize = get_file_size(FileName);	/* Call OS specific routine */
	    /* Although we know the exact line name we call Queue_File();
	       this is usefull when this routine is called via DEBUG RESCAN
	       command and will queue the files to the alternate route,
	       if found.						*/
/* logger(1,"queue file: '%s' (%d blk)\n",FileName,FileSize); */
	    if (sync_mode)
	      queue_file(FileName, FileSize, NULL, NULL);
	    else
	      submit_file(pipes[1], FileName, FileSize);
	    ++filecnt;
#ifdef UNIX
	    *FileMask = '\0';	/* So we know this is not the first call */
#endif
	  }

	}
#ifdef UNIX
	sprintf(FileMask, "%s/*",BITNET_QUEUE);
	make_dirs( FileMask );

	while (find_file(FileMask, FileName, &context) == 1) {
	  FileSize = get_file_size(FileName);
	  /* Call OS specific routine */
	  if (sync_mode)
	    queue_file(FileName, FileSize, NULL, NULL);
	  else
	    submit_file(pipes[1],FileName, FileSize);
	  ++filecnt;
	  *FileMask = '\0';	/* So we know this is not the first call */
	} /* while( find_file() ) */
#endif
	if (!sync_mode) {
	  logger(1,"FILE_QUEUE: Child-mode file queuer done. Submitted %d files.\n",filecnt);
	  _exit(0);
	} else
	  logger(1,"FILE_QUEUE: Sync-mode file queuer done. Submitted %d files.\n",filecnt);
	return 1;
}


/* ================ submit_file()  ================ */

static void
submit_file(fd, FileName, FileSize)
const int fd;
const char	*FileName;
int	FileSize;
{
	unsigned char	line[LINESIZE];
	long	size;

	FileSize /= 512; /* receiver multiplies it again.. */

	/* Send file size */
	line[1] = (unsigned char)((FileSize & 0xff00) >> 8);
	line[2] = (unsigned char)(FileSize & 0xff);
	strcpy(&line[3], FileName);
	size = strlen(&line[3]) + 4; /* include terminating NULL.. */
	line[0] = size-1;

	writen(fd,line,size);
}


/* ================ queue_file() ================ */
/*
 | Given a file name, find its line number and queue it. If the line number is
 | not defined, ignore this file.
 | We try getting an alternate route if possible. If the status returned is
 | that the link exists but inactive, we must loop again and look for it.
*/
void
queue_file(FileName, FileSize, ToAddr, Entry)
const char	*FileName, *ToAddr;
const int	FileSize;	/* File size in blocks */
struct QUEUE	*Entry;
{
	register char	*p;
	register int	i;
	int	rc, primline = -1, altline = -1;
	char	MessageSender[10];
	struct	FILE_PARAMS FileParams;
	FILE	*InFile;
	char	auxline[LINESIZE];
	char	Format[16];	/* Not needed but returned by Find_line_index() */
	struct stat stats;
	
	if (ToAddr == NULL) {
	  Entry = NULL;
	  logger(3,"FILE_QUEUE: queue_file(%s,%d,NULL)\n",FileName,FileSize);

	  i = strlen(BITNET_QUEUE);
	  if (FileName[0] != '/' ||
	      strncmp(FileName,BITNET_QUEUE,i) != 0 ||
	      FileName[i] != '/') {
	    logger(1, "FILE_QUEUE: File to be queued must be within `%s/'. Its name was: `%s'\n",BITNET_QUEUE,FileName);
	    return;
	  }

	  if (STATFUNC(FileName,&stats) != 0 ||
	      !S_ISREG(stats.st_mode)) {
	    logger(1,"Either the file does not exist, or it is not a regular file, file: `%s'\n",FileName);
	    return;
	  }
	  FileParams.FileStats = stats;
	  strcpy(FileParams.SpoolFileName,FileName);

	  InFile = fopen(FileName,"r+");
	  if (InFile == NULL) {
	    logger(1, "FILE_QUEUE: Couldn't open file '%s' for envelope analysis.\n",FileName);
	    return;
	  }
	  *FileParams.line = 0;
	  rc = parse_envelope( InFile, &FileParams, 0 );
	  fclose( InFile );
	  if ((rc < 0) || (*FileParams.From == '*')) {
	    /* Bogus file. Move to hideout... */
	    logger(1,"FILE_QUEUE, queue_file(%s) got bad file - header contains junk..\n",FileName);
	    rename_file(&FileParams, RN_JUNK, F_OUTPUT_FILE);
	    return;
	  }
	} else {		/* Re-queueing it, no parse.. */
	  logger(3,"FILE_QUEUE: queue_file(%s,%d,\"%s\")\n",FileName,FileSize,ToAddr);
	  strcpy(FileParams.SpoolFileName, FileName );
	  strcpy(FileParams.To, ToAddr);
#ifdef UNIX
	  if (Entry != NULL)
	    FileParams.FileStats = Entry->fstats;
	  else
	    stat(FileParams.SpoolFileName,&FileParams.FileStats);
#endif
	}

	if ((p = strchr(FileParams.To,'@')) == NULL) {
	  /* Huh! No '@' in target address ! */
	  rename_file(&FileParams, RN_JUNK, F_OUTPUT_FILE);
	  logger(1,"FILE_QUEUE: ### No '@' in target address !!!  File:%s\n",
		 FileParams.SpoolFileName);
	  return;
	}
	++p;	/* P points to target node.  Find route! */
	switch (i = find_line_index(p, FileParams.line, Format,
				    &primline, &altline)) {
	  case NO_SUCH_NODE:
	      /* Pass to local mailer. */
	      rename_file(&FileParams, RN_JUNK, F_OUTPUT_FILE);
	      logger(1, "FILE_QUEUE: Can't find line # for file '%s', line '%s', Tonode='%s'\n",

		     FileName,FileParams.line,p);
	      break;
	  case ROUTE_VIA_LOCAL_NODE:
	      inform_filearrival( FileName,&FileParams,auxline );
	      /* Log a receive */
	      rscsacct_log(&FileParams,0);
	      sprintf(MessageSender, "@%s", LOCAL_NAME);
	      /* Send message back only if not found the QUIET option. */
	      if (((FileParams.type & F_NOQUIET) != 0) &&
		  (*auxline != 0)) {
		send_nmr((char*)MessageSender,
			 FileParams.From,
			 auxline, strlen(auxline),
			 ASCII, CMD_MSG);
	      } else
		logger(3,"FILE_QUEUE: Quiet ack of received file - no msg back.\n");
	      break;

	  case LINK_INACTIVE:	/* No alternate route available... */
	      /* Anyway place the file to there.. */
	      /* Queue it */
	      FileName = rename_file(&FileParams,RN_NORMAL,F_OUTPUT_FILE);
	      if (Entry && Entry->primline != primline) {
		free(Entry);
		Entry = NULL;
	      }
	      if (!Entry)
		Entry = build_queue_entry(FileName, primline, FileSize,
					  FileParams.To, &FileParams);
	      add_to_file_queue(&IoLines[primline],primline,Entry);
	      logger(2,"FILE_QUEUE: Queued file \"%s\" to inactive link \"%s\"\n",
		     FileName,IoLines[primline].HostName);
	      return;

	  default:	/* Hopefully a line index */
	      if ((i < 0) || (i > MAX_LINES) ||
		  IoLines[i].HostName[0] == 0) {
		logger(1, "FILE_QUEUE, Find_line_index() returned erronous index (%d) for node %s\n", i, p);
		return;
	      }
	      FileName = rename_file(&FileParams,RN_NORMAL,F_OUTPUT_FILE);
	      if (Entry && Entry->primline != primline) {
		free(Entry);
		Entry = NULL;
	      }
	      if (!Entry)
		Entry = build_queue_entry(FileName, primline, FileSize,
					  FileParams.To, &FileParams);
	      add_to_file_queue(&IoLines[i],i,Entry);
	      logger(2,"FILE_QUEUE: Queued file \"%s\" to active link \"%s\"\n",
		     FileName,IoLines[i].HostName);
	      break;
	}
}

/*
 | Given a filename and the line number (index into IoLines array),
 | the filename will be queued into that line.  Order the list according
 | to the file's size.
 */

void
add_to_file_queue(Line, Index, Entry)
struct LINE *Line;
const int Index;
struct QUEUE *Entry;
{
	struct	QUEUE	*keep, *keep2;
	int FileSize = Entry->FileSize;

	if (hash_find(Entry) != NULL) {
	  logger(2,"FILE_QUEUE: add_to_file_queue(%s, %s) already in the queue\n",
		 Line->HostName,Entry->FileName);
	  free(Entry);
	  return;
	}

	if (Line->QueueStart == NULL) { /* Init the list */
	  /* Empty list - insert at the head */
	  Line->QueueStart = Line->QueueEnd = Entry;
	} else {
	  /* Look for right place to put it */
	  keep = Line->QueueStart;
	  while ((keep->next != NULL) &&
		 (keep->next->FileSize < FileSize))
	    keep = keep->next;
#ifdef	UNIX
	  keep2 = keep;
	  while (keep2 != NULL && FileSize == keep2->FileSize) {
	    if (keep2->fstats.st_dev  == Entry->fstats.st_dev &&
		keep2->fstats.st_ino  == Entry->fstats.st_ino &&
		keep2->fstats.st_size == Entry->fstats.st_size) {
	      logger(2,"FILE_QUEUE: add_to_file_queue(%s, %s) already in the queue\n",
		     Line->HostName,Entry->FileName);
	      free(Entry);
	      return;
	    }
	    keep2 = keep2->next;
	  }
#endif
	  if ((keep == Line->QueueStart) &&
	      (keep->FileSize > FileSize)) {
	    /* Have to add at queue's head */
	    Entry->next = keep;
	    Line->QueueStart = Entry;
	  } else {		/* Insert in middle of queue */
	    Entry->next = keep->next;
	    keep->next = Entry;
	  }
	  if (Entry->next == 0)	/* Added as the last item */
	    Line->QueueEnd = Entry;
	}
	if (Entry == 0) {
#ifdef VMS
	  logger(1, "FILE_QUEUE, Can't malloc. Errno=%d, VmsErrno=%d\n",
		 errno, vaxc$errno);
#else
	  logger(1, "FILE_QUEUE, Can't malloc. Errno=%d\n",
		 errno);
#endif
	  bug_check("FILE_QUEUE, Can't Malloc for file queue.");
	}
	hash_insert(Entry);
	Line->QueuedFiles++;		/* Increment count */
	Line->QueuedFilesWaiting++;	/* Increment count */
}


struct QUEUE *
build_queue_entry(FileName, LineIndex, FileSize, ToAddr, FileParams)
const char *FileName, *ToAddr;
const int LineIndex;
const int FileSize;
struct FILE_PARAMS *FileParams;
{
	struct	QUEUE	*Entry;
	struct	LINE	*Line;

	Line = &(IoLines[LineIndex]);
	Entry = (struct QUEUE *)malloc(sizeof(struct QUEUE));

	if (Entry == NULL) {
#ifdef VMS
	  logger(1, "FILE_QUEUE, Can't malloc. Errno=%d, VmsErrno=%d\n",
		 errno, vaxc$errno);
#else
	  logger(1, "FILE_QUEUE, Can't malloc. Errno=%d\n",
		 errno);
#endif
	  bug_check("FILE_QUEUE, Can't malloc() memory");
	}

	if (FileParams == NULL) {
	  if (STATFUNC(FileName,&Entry->fstats) != 0) {
	    logger(1,"** FILE_QUEUE: Tried to queue a non-existent file: `%s' on line %d\n",FileName,LineIndex);
	    return NULL;
	  }
	} else
	  Entry->fstats = FileParams->FileStats;

	strcpy(Entry->FileName, FileName);
	strcpy(Entry->ToAddr, ToAddr);
#ifdef UNIX
	Entry->FileSize = Entry->fstats.st_size; /* So we can see it in
						    SHOW QUEUE */
	Entry->hash = ((Entry->fstats.st_size ^ Entry->fstats.st_ino) ^
		       Entry->fstats.st_dev);
#else
	Entry->FileSize = FileSize;
	{
	  char *s = FileName; long hash = 0;
	  while (*s) { if (hash & 1) hash = (~hash) >> 1; hash += *s++; }
	  Entry->hash     = hash;
	}
#endif
	Entry->hashnext = NULL;
	Entry->next     = NULL;
	Entry->state    = 0;
	Entry->primline = LineIndex;
	Entry->altline  = -1;

	return Entry;
#if 0 /* XX: Think more about multiple parallel queues.. */
#endif
}

/*
 | Show the files queued for each line.
 */
void
show_files_queue(UserName,LinkName)
const char	*UserName;		/* To whom to broadcast the reply */
char *LinkName;
{
	int	i, j = 0;
	struct	QUEUE	*QE;
	char	line[LINESIZE],
		from[SHORTLINE];		/* The sender (local daemon) */
	int	queuelen = strlen(BITNET_QUEUE)+1;
	int	maxlines = 30;

	/* Create the sender and receiver's addresses */
	sprintf(from, "@%s", LOCAL_NAME);

	upperstr(LinkName);
	if (*LinkName) {
	  for (i = 0; i < MAX_LINES; ++i) {
	    if (IoLines[i].HostName[0] != 0 &&
		strcmp(IoLines[i].HostName,LinkName)==0) {
	      if ((QE = IoLines[i].QueueStart) != 0) {
		sprintf(line, "Showing at most %d files on link %s queue",
			maxlines, LinkName);
		send_nmr(from, UserName, line, strlen(line), ASCII, CMD_MSG);
		while (QE != NULL) {
		  ++j;
		  if (j <= maxlines) {
		    sprintf(line, " %3d   %s, %4d kB %s", j,
			    QE->FileName+queuelen, QE->FileSize/1024,
			    (QE->state<0) ? "HELD" : ((QE->state > 0) ?
							"Sending":"Waiting"));
		    send_nmr(from, UserName, line, strlen(line), ASCII, CMD_MSG);
		  }
		  QE = QE->next;
		}
		sprintf(line, "End of list: %d out of %d files",maxlines,j);
	      } else {
		sprintf(line, "No files queued on link %s", LinkName);
	      }
	      send_nmr(from, UserName, line, strlen(line), ASCII, CMD_MSG);
	      return;
	    }
	  }
	  sprintf(line, "Link %s is not defined", LinkName);
	  send_nmr(from, UserName, line, strlen(line), ASCII, CMD_MSG);
	  return;
	} else {

	  sprintf(line, "Files waiting:");
	  send_nmr(from, UserName, line, strlen(line), ASCII, CMD_MSG);
	  j = 0;
	  for (i = 0; i < MAX_LINES; i++) {
	    if (IoLines[i].HostName[0] != 0 &&
		(QE = IoLines[i].QueueStart) != 0) {
	      sprintf(line, "Files queued for link %s:",
		      IoLines[i].HostName);
	      send_nmr(from, UserName, line, strlen(line),
		       ASCII, CMD_MSG);
	      while (QE != 0) {
		++j;
		if (j <= maxlines) {
		  sprintf(line, "      %s, %4dkB %s",
			  QE->FileName+queuelen, QE->FileSize/1024,
			  (QE->state<0) ? "HELD" : ((QE->state > 0) ?
						    "Sending" : "Waiting"));
		  send_nmr(from, UserName, line, strlen(line), ASCII, CMD_MSG);
		}
		QE = QE->next;
	      }
	    }
	  }
	}
	sprintf(line, "End of list: (%d files, %d shown)",j,maxlines);
	send_nmr(from, UserName, line, strlen(line), ASCII, CMD_MSG);
}


struct QUEUE *
dequeue_file_entry_ok(Index,Line, freeflg)
const int Index, freeflg;
struct LINE	*Line;
{
	struct	QUEUE	*FE,*FE2, *QFE = NULL;
	struct	FILE_PARAMS *FP = & Line->OutFileParams[Line->CurrentStream];

	if (FP->FileEntry == NULL) {
	  /* We can be called from  requeue_file_entry()  when aborting and
	     requeueing all files on a link -> the queue can be empty! */
	  /* logger(1,"FILE_QUEUE: dequeue_file_entry_ok(%s:%d) Non-active stream dequeued!\n",
	     Line->HostName,Line->CurrentStream); */
	  return NULL; /* Will propably cause SIGSEGV.. */
	}

	/* logger(2,"FILE_QUEUE: dequeue_file_entry_ok(%s:%d) fn=%s, state=%d\n",
	   Line->HostName,Line->CurrentStream,FP->SpoolFileName,
	   FP->FileEntry->state); */
	FP->FileEntry->state=0;
	QFE = FP->FileEntry;

	hash_delete(FP->FileEntry);

	if (FP->FileEntry->next == NULL && FP->FileEntry != Line->QueueEnd) {
	  /* Actually do nothing, as this is a lone devil left over from
	     DEBUG RESCAN -- an active stream at the time of the rescan.. */
	} else if (FP->FileEntry == Line->QueueStart) {
	  /* Ours is the first one.. */
	  Line->QueueStart = FP->FileEntry->next;
	  if (Line->QueueStart == NULL)	/* And the only one.. */
	    Line->QueueEnd = NULL;

	} else {
	  FE = Line->QueueStart;
	  FE2 = FE;
	  while (FE->next) {	/* MUST go at least once, as else it was
				   the first one.. */
	    if (FE == FP->FileEntry) break;
	    FE2 = FE;
	    FE = FE->next;
	  }
	  /* We might have ordered DEBUG RESCAN, which can cause
	     a situation, where only pointer to the file entry is
	     FP->FileEntry...					 */
	  if (FP->FileEntry == FE) {
	    /* Now FE points to the entry to be dequeued,
	       and FE2 points to its predecessor		*/
	    FE2->next = FE->next;
	    if (FE2->next == NULL) /* End of list, predecessor is the tail */
	      Line->QueueEnd = FE2;
	  }
	}
	FP->FileEntry = NULL;
	Line->QueuedFiles -= 1;

	if (freeflg) { free(QFE); QFE = NULL; }

	return QFE;
}

void
requeue_file_entry(Index,Line)
const int Index;
struct LINE	*Line;
{
	int i = Line->CurrentStream;
	struct QUEUE *Entry;

	Entry = dequeue_file_entry_ok(Index,Line, 0);
	/* Requeue file */
	queue_file(Line->OutFileParams[i].SpoolFileName,
		   Line->OutFileParams[i].FileSize,
		   Line->OutFileParams[i].To,
		   Entry);
}


struct QUEUE *
pick_file_entry(Index,Line)
const int Index;
const struct LINE *Line;
{
	struct QUEUE *FileEntry = Line->QueueStart;
	int i = Line->QueuedFiles+1;

	/* In case the file is queued on an alternate line as well, we have
	   to make sure that when the primary link is active, we don't rob
	   files from there to the alternate link.. */

#if 1
	while (FileEntry && FileEntry->state != 0 && --i > 0)
#else
	while (FileEntry && FileEntry->state != 0 && --i > 0 &&
	       FileEntry->altline == Index &&
	       IoLines[FileEntry->primline].state == ACTIVE)
#endif

	  FileEntry = FileEntry->next;


	if (i <= 0)
	  bug_check("Aborting because of corruption of  file-entry queue!");
	return FileEntry;
}


int /* Count actives */
requeue_file_queue(Line)
struct LINE *Line;
{
	struct QUEUE	*Entry, *NextEntry;
	int activecnt = 0;

	Entry = Line->QueueStart;

	/* Free all non-active streams, and collect active ones
	   back into the link queue ... */

	Line->QueueStart = NULL;
	Line->QueueEnd   = NULL;
	
	while (Entry != NULL) {
	  /* Separate the entries */
	  NextEntry = Entry->next;
	  Entry->next = NULL;

	  if (Line->state != ACTIVE)
	    Entry->state = 0;		/* Reset it in every case if
					   the link is not active */

	  /* States are: 0: Waiting, -1: held, +1: active */
	  if (Entry->state < 1) {	/* Don't free active files! */
	    hash_delete(Entry);
	    Line->QueuedFiles -= 1;
	    Line->QueuedFilesWaiting -= 1;
	    queue_file(Entry->FileName,Entry->FileSize,Entry->ToAddr,Entry);
	  } else {
	    ++activecnt;
	    if (Line->QueueStart == NULL)
	      /* Can be NULL only at the first time. */
	      Line->QueueStart = Entry;
	    else
	      /* Inserted at second, and later times.*/
	      Line->QueueEnd->next = Entry;
	    Line->QueueEnd = Entry;
	  }
	  Entry = NextEntry;
	}

	return activecnt;
}


int /* Count actives */
delete_file_queue(Line)
struct LINE *Line;
{
	struct QUEUE	*Entry, *NextEntry;
	int activecnt = 0;

	Entry = Line->QueueStart;

	/* Free all non-active streams, and collect active ones
	   back into the link queue ... */

	Line->QueueStart = NULL;
	Line->QueueEnd   = NULL;

	while (Entry != NULL) {
	  NextEntry = Entry->next;
	  Entry->next = NULL;


	  if (Line->state != ACTIVE)
	    Entry->state = 0;		/* Reset it in every case if
					   the link is not active */

	  /* States are: 0: Waiting, -1: held, +1: active */
	  if (Entry->state < 1) {	/* Don't free active files! */
	    hash_delete(Entry);
	    Line->QueuedFiles        -= 1;
	    Line->QueuedFilesWaiting -= 1;
	    free(Entry);
	  } else {
	    ++activecnt;
	    if (Line->QueueStart == NULL)
	      /* Can be NULL only at the first time. */
	      Line->QueueStart = Entry;
	    else
	      /* Inserted at second, and later times.*/
	      Line->QueueEnd->next = Entry;
	    Line->QueueEnd = Entry;
	  }
	  Entry = NextEntry;
	}

	return activecnt;
}
