/* TRANSFER.C - v 0.1 */
/* By <mea@nic.funet.fi>, and <root@alijku65.edvz.uni-linz.ac.at>  */
/*
   FUNET-NJE companion program to transfer a spoolfile to arbitary
   destination - does NJE-header rewrite when necessary.
*/


/*
 Programs are executed with following ARGV:
  0  =  ProgPath
  1  =  Forward@Address
  2  =  FilePath
*/

#include "prototypes.h"
#include "clientutils.h"

#define isprint(c) ( ((c) >=32) && ((c) <= 126) )

char LOCAL_NAME   [10];
char BITNET_QUEUE [80];
char LOG_FILE     [80] = "-";

int LogLevel = 3;
FILE *LogFd  = NULL;


/* ================================================================ */

void
transfer( transparency,path,Destin )
     const int transparency;
     const char *path; 
	   char *Destin;
{
  
	FILE	*TransferF;
	char	ToA[80];
	struct stat stats;
/*	unsigned char ToUser[9],ToNode[9]; */
	unsigned char buf[512]; /* longest NJE header is 257... */
	int	ebcdic = 0;
	long	FmTpos = -1;
	long	ToApos = -1;
	long	FiDpos = -1;
	long	OiDpos = -1;
	long	FrMpos = -1;
	long	pos0;
	char	newpath[200];

	/* Now have changed a bit... */
	/* Actual location of file is found from: path */

	/* Send message to given user@node */
	/* Action->Path -- This points to file+message recipient */
	TransferF = fopen(path,"r+");
	if (TransferF == NULL) {
	  logger(1,"UNIX_FILES: TRANSFER-TO fopen('%s','r+') failed ! (%s)\n",
		 path,PRINT_ERRNO);
	  return;
	}
	pos0 = -1;
	*buf = 0; /* Pre-empt buffer contents */
	while (!feof(TransferF) && !ferror(TransferF)) {
	  pos0 = ftell( TransferF );
	  *buf = 0; /* Pre-empt buffer contents */
	  if (fgets( (void*)buf,sizeof buf,TransferF ) == NULL)
	    break;  /* File read failed! */
	  if (strcmp("FMT: EBCDIC\n",buf)==0) {
	    FmTpos = pos0;
	    ebcdic = 1;	/* Right, we have NJE headers... */
	  } else if (!transparency && strncmp("FID: ",buf,5) == 0)
	    FiDpos = pos0;
	  else if (!transparency && strncmp("OID: ",buf,5) == 0)
	    OiDpos = pos0;
	  else if (!transparency && strncmp("FRM: ",buf,5)==0)
	    FrMpos = pos0;
	  else if (strncmp("TOA: ",buf,5) == 0) {
	    strcpy( ToA,buf );
	    ToApos = pos0;
	  } else if (strcmp("END:\n",buf) == 0)
	    break; /* last entry on headers */
	}
	if (ToApos < 0) {
	  /* Something wrong ! */
	  logger(1,"UNIX_FILES: Parse of header for TRANSFER-TO faulted, didn't find TOA:!\n");
	  return;
	}

	if (ebcdic) {
	  /* Rewrite FMT: BINARY */
	  fseek( TransferF,FmTpos,0 /* from begin of file */ );
	  fprintf( TransferF,"FMT: BINARY\n" );
	}

	if (FiDpos >= 0) {
	  fseek( TransferF,FiDpos,0 );
	  fputs( "FID: 0000", TransferF );
	}
	if (OiDpos >= 0) {
	  fseek( TransferF,OiDpos,0 );
	  fputs( "OID: 0000", TransferF );
	}
	if (FrMpos >= 0) {
	  char From[20];

	  if (mcuserid(From) == NULL) {
	    fprintf(stderr,"Can't determine who you are. Aborting!\n");
	    exit(2);
	  }
	  upperstr(From);
	  strcat(From,"@");
	  strcat(From,LOCAL_NAME);
	  fseek(TransferF,FrMpos,0);
	  fprintf(TransferF, "FRM: %-17.17s\n", From);
	}

	/* Rewrite TOA: record... */    
	fseek( TransferF,ToApos,0 /* from begin of file */ );
	fprintf( TransferF,"TOA: %-17.17s\n",Destin );
	fclose( TransferF );

	stat(path,&stats);

	sprintf(newpath,"%s/BITtr%ld",BITNET_QUEUE,(long)stats.st_ino);
	if (rename( path,newpath ) == 0)
	  path = newpath;

	/* Right, now TOA: is redirected.  Lets send it forward... */
	submit_file( path,(stats.st_size+511)/512 );
}


/* ================================================================ */

int
main(argc,argv)
     int argc;
     char *argv[];
{
	const char *path;
	char *Destin;
	int t_flag  = 0;

	if (argc == 4 && strcmp(argv[1],"-t")==0 &&
	    getuid() == 0) {
	  t_flag = 1;
	  ++argv;
	  --argc;
	}

	if (argc != 3) {
	  printf(
"TRANSFER - program for FUNET-NJE Spool Transfer processing.\n\
          Not for casual users invocation.\n\
Args:  [-t] Destin@Address absolute-filepath\n\
  When file does have NJE headers, they are rewritten too to\n\
  reflect new destination address.  Else we would propably get\n\
  forwarded file back very soon.  Non-super-user can't do it\n\
  transparently (-t).\n");
	  exit(3); /* Too few args... */
	}

	Destin = argv[1];
	path  = argv[2];

	if (*path != '/') {
	  printf("TRANSFER: given file to be trasnferred is NOT an ABSOLUTE path: \"%s\"\n",path);
	  exit(4);
	}

	read_configuration();
	/* read_etable() */  /* Not needed if we don't mess
				with the real headers..		  */

/*logger(2,"Argc=%d, Argv=[%s,%s,%s]\n",argc,argv[0],argv[1],argv[2]);*/

	upperstr(Destin);
	transfer( t_flag, path, Destin );

	return 0;
}

/* ================================================================ */

/*
 | Log the string, and then abort.
 */
volatile void
bug_check(string)
const char	*string;
{
	logger(1, "Bug check: %s\n", string);
	exit(100);
}
