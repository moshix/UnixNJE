/* UNIX_ROUTE.C (Formerly UNIX_SEARCH_ROUTE)	V1.2-mea-1.4
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
 | Full rewrite for  FUNET-NJE package, and own `bintree' database by
 | (c) Finnish University and Research Network, FUNET, 1993, 1994.
 */
#include "consts.h"
#include "prototypes.h"
#include "bintree.h"

extern int	errno;
extern int	errno;
#ifdef __DARWIN_UNIX03
extern const int	sys_nerr;	/* Maximum error number recognised */
#else
extern int	sys_nerr;	/* Maximum error number recognised */
#endif
/* extern char	*sys_errlist[];	*/ /* List of error messages */
#define	PRINT_ERRNO	(errno > sys_nerr ? "***" : sys_errlist[errno])

struct Bintree *routedb = NULL;

/* Database needs this comparison routine.. */
int CompareRoute(r1,r2)
struct ROUTE_DATA *r1, *r2;
{
	return strcmp(r1->DestNode, r2->DestNode);
}


/*
 | Open the address file.
 */
int
open_route_file()
{

	routedb = bintree_open(TABLE_FILE,
				sizeof(struct ROUTE_DATA),
				CompareRoute);
	if (routedb == NULL) {
	  logger(1, "UNIX_ROUTE: can't open Bintree database file `%s', error: %s\n",
		 TABLE_FILE, PRINT_ERRNO);
	  return 0;
	}
	return 1;	/* Success */
}

/*
 | Close the file. Since the descriptor is static, we need this routine.
 */
void
close_route_file()
{
	bintree_close(routedb);
	routedb = NULL;
}


/*
 | Get the record whose key is the node we look for. Return the line
 | which corresponds to it, and 1. Return 0, if nothing found, and no
 | 'defaultroute'.    Flag 'defaultroute' by returning -1.
 */
int
get_route_record(key, line, LineSize)
const char	*key;
char		*line;
const int	LineSize;
{
	struct ROUTE_DATA Key, *Route;


	/*  Copy the site name and pad it with blanks to 8 characters */
	memcpy(Key.DestNode, key, 8);
	Key.DestNode[8] = 0;
	/* It is already in upper-case.. */

	Route = bintree_find(routedb, &Key);
	if (Route == NULL) {
	  /* Not found. If there is default route - use
	     it now and return success */
	  if (*DefaultRoute != '\0') {
#ifdef DEBUG
	    logger(4, "Using default route for '%s'\n", key);
#endif
	    sprintf(line, "%s E", DefaultRoute);
	    return -1;
	  }
#ifdef	DEBUG
	  logger(2,"Not found a route record for key: `%s'/\n",key);
#endif
	  return 0;	/* Not found */
	}

	sprintf(line, "%s %c", Route->DestLink, Route->Fmt);
	return 1;
}



/*
 | Change a route of node. If it exists, update its record. If not, add it.
 | If the node is not local, the format used is EBCDIC.
 */
void
change_route(Node, Route)
const char	*Node;
char		*Route;
{
	char	line[LINESIZE];
	struct ROUTE_DATA Key, *Routep;
	int	delete;

	delete =  (strcasecmp(Route, "DEFAULT")==0 ||
		   strcasecmp(Route, "OFF")==0);

	/* Convert to upper case and add trailing blanks. */
	memcpy(Key.DestNode, Node, 8);
	Key.DestNode[8] = 0;
	upperstr(Key.DestNode);
	memcpy(Key.DestLink, Route, 8);
	Key.DestLink[8] = 0;
	upperstr(Key.DestLink);
	Key.Fmt = 'E';

	/* Try retrieving it. If exists - use update */

	Routep = bintree_find(routedb,&Key);

	*line = 0;
	if (Routep != NULL) {
	  sprintf(line, "%s %s %c",
		  Routep->DestNode, Routep->DestLink, Routep->Fmt);
	  if (delete) {
	    bintree_delete(routedb,&Key);
	    logger(1, "CHANGE_ROUTE: Deleted route '%s'\n",line);
	    return;
	  }
	}
	if (delete) return; /* Already off.. */

	if (strcmp(Route, "LOCAL") == 0)
	  Key.Fmt = 'A';
	else
	  Key.Fmt = 'E';

	if (Routep != NULL) {
	  memcpy(Routep,&Key,sizeof(Key));
	  bintree_update(routedb,Routep);
	} else {
	  bintree_insert(routedb,&Key);
	}
	if (*line == 0)	/* New route */
	  logger(1, "CHANGE_ROUTE, New routing record added: '%s %s %c'\n",
		 Key.DestNode, Key.DestLink, Key.Fmt);
	else		/* Updated route - show the old one also */
	  logger(1, "CHANGE_ROUTE, New routing record added: '%s %s %c' instead of '%s'\n",
		 Key.DestNode, Key.DestLink, Key.Fmt, line);
}
