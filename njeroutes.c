/* NJEROUTES.C 1.3-mea
 |
 |  Build up the route database for FUNET-NJE.
 |  Copyright (C) Finnish University and Research Network, FUNET
 |  1991,1993,1994
 |
 |  There is marginal resemblance with original UNIX_BUILD of
 |  HUJI-NJE, however this is no longer such a a primitive thing,
 |  as that was...
*/

#include "consts.h"
#include "prototypes.h"
#include "bintree.h"

/* extern char *sys_errlist[]; */
extern int errno;

char *pname;

int CompareRoute(r1,r2)
struct ROUTE_DATA *r1, *r2;
{
	return strcmp(r1->DestNode, r2->DestNode);
}

int
main(argc,argv)
     int argc;
     char *argv[];
{
	char	*c, line[132],
		site[21], LineName[21], format[20],
		temp_line[132];	/* Just a place holder */
	char	in_file[80], header_file[80], out_file[80];
	FILE	*Ifd, *fd, *Dup=NULL;
	int	counter, Dcount, lines;
	struct ROUTE_DATA  RouteRecord;
	struct Bintree *BT;

	if ((pname = strrchr(argv[0],'/')) != NULL)
	  ++pname;
	else
	  pname = argv[0];

/* Get files' names */
	if (argc != 4) {
	  printf("%s  Creates FUNET-NJE routing tables.\n",pname);
	  printf("%s  Given 3 arguments the interactive mode is not activated:\n",pname);
	  printf("%s  Args are in order:  Header-Fname, RoutingTbl-Fname, OutputFname\n",pname);
	  

	  printf("Header file: "); gets(header_file);
	  printf("Routing table: "); gets(in_file);
	  printf("Output file: "); gets(out_file);
	} else  {
	  strncpy( header_file,argv[1],sizeof header_file -1);
	  strncpy( in_file,argv[2],sizeof in_file -1);
	  strncpy( out_file,argv[3],sizeof out_file -1);
	}	  
	
	if ((c = strchr(header_file, '\n')) != NULL) *c = '\0';
	if ((c = strchr(in_file, '\n')) != NULL) *c = '\0';
	if ((c = strchr(out_file, '\n')) != NULL) *c = '\0';

	if ((Ifd = fopen(header_file, "r")) == NULL) {
	  printf("Can't open header file '%s'\n", header_file);
	  exit(1);
	}
	if ((fd = fopen(in_file, "r")) == NULL) {
	  printf("Can't open input file: %s\n", in_file);
	  exit(1);
	}

	/* Open the database file */

	/* Make sure it doesn't exist now.. */
	unlink(out_file);
	BT = bintree_open(out_file,
			  sizeof(struct ROUTE_DATA),
			  CompareRoute);
	if (BT == NULL) {
	  int oerrno = errno;
	  fprintf(stderr,"%s:  Open of `%s' database failed because of error: %s\n",
		  pname,out_file,PRINT_ERRNO);
	  exit(oerrno);
	}

	counter = 0;  Dcount = 0;
	lines = 0;
	printf("Records read:\n");

	/* The header file */

	while (fgets(line, sizeof line, Ifd) != NULL) {
	  ++lines;
	  if ((c = strchr(line, '\n')) != 0) *c = 0;
	  /*printf("Got line: '%s'\n",line);*/
	  if (*line == '*') continue; /* Comment line */
	  if (*line == '#') continue; /* Comment line */
	  if (*line == 0 )  continue; /* Empty line */
	  counter++;
	  if (counter % 100 == 0) {
	    printf("%d%s", counter,(counter%1000 == 0) ? "\n":" ");
	    fflush(stdout);
	  }
	  if (sscanf(line, "%*s %s %s %s",
		     site, LineName, format) < 3) {
	    printf("Illegal line: %s\n", line);
	    continue;
	  }
	  if (strcasecmp(LineName,"DEFAULT")==0 ||
	      strcasecmp(LineName,"SYSTEM")==0  ||
	      strcasecmp(LineName,"OFF")== 0) {
	    printf("Bad route target: %d: '%s'\n",lines, line);
	    continue;
	  }

	  memcpy(RouteRecord.DestNode,site,8);
	  RouteRecord.DestNode[8] = 0;
	  memcpy(RouteRecord.DestLink,LineName,8);
	  RouteRecord.DestLink[8] = 0;
	  RouteRecord.Fmt = *format;

	  if (bintree_find(BT,&RouteRecord) == NULL) {
	    /* Not found already, insert! */
	    bintree_insert(BT,&RouteRecord);
	  } else {
	    /* A duplicate ! */
	    ++Dcount;
	    if (!Dup) Dup = fopen("DUPLICATES.TMP","w");
	    if (Dup) fprintf(Dup,"HdrDup: %d: %s\n",lines, line);
	  }
	}

	printf("Processing routing table\n");
	
	/* Now process the routing table */

	lines = 0;
	while (fgets(line, sizeof line, fd) != NULL) {
	  ++lines;
	  if ((c = strchr(line, '\n')) != 0)
	    *c = 0;
	  /*printf("Got line: '%s'\n",line);*/
	  if (*line == '*') continue; /* Comment line */
	  if (*line == '#') continue; /* Comment line */
	  if (*line == 0 ) continue; /* Empty line */
	  counter++;
	  if (counter % 100 == 0) {
	    printf("%d%s", counter,(counter%1000 == 0) ? "\n":" ");
	    fflush(stdout);
	  }
	  /* 'ROUTE node ToLink' */
	  if (sscanf(line, "%s %s %s",
		     temp_line, site, LineName) < 2) {
	    printf("Illegal line: %s\n", line);
	    continue;
	  }
	  if (strcasecmp(LineName,"DEFAULT")==0 ||
	      strcasecmp(LineName,"SYSTEM")==0  ||
	      strcasecmp(LineName,"OFF")==0) {
	    printf("Bad route target: '%s'\n",line);
	    continue;
	  }
	  memcpy(RouteRecord.DestNode,site,8);
	  RouteRecord.DestNode[8] = 0;
	  memcpy(RouteRecord.DestLink,LineName,8);
	  RouteRecord.DestLink[8] = 0;
	  RouteRecord.Fmt = *format;

	  if (bintree_find(BT,&RouteRecord) == NULL) {
	    /* Not found already, insert! */
	    bintree_insert(BT,&RouteRecord);
	  } else {
	    /* A duplicate ! */
	    ++Dcount;
	    if (!Dup) Dup = fopen("DUPLICATES.TMP","w");
	    if (Dup) fprintf(Dup,"RteDup: %d: %s\n",lines, line);
	  }
	}

	fclose(fd);
	fclose(Ifd);
	bintree_close(BT);

	printf("\nTotal records inserted: %d, %d duplicates in DUPLICATE.TMP\n",
	       counter, Dcount);
	if (Dup) fclose(Dup);
	return 0;
}

