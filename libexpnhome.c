/*	FUNET-NJE		Client utilities
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 26-Sep-1993
 */


#include "prototypes.h"
#include "clientutils.h"

#include <sysexits.h>
#include <ctype.h>

/* ExpandHomeDir() -- taken from  unix_files.c -- by <mea@nic.funet.fi> */

extern char DefaultSpoolDir[256];

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

	if (strcmp(PathOrDir,"default")==0 || *PathOrDir == 0) {
	  PathOrDir = DefaultSpoolDir;
	}

	if (*PathOrDir == '~') {
	  if (PathOrDir[1] != '/') {
	    /* Uh.. ~/ would have been easy... */
	    strcpy( path2,PathOrDir+1 );
	    if ((s = strchr(path2,'/'))) *s = 0;
	    if ((passwds = getpwnam( path2 ))) {
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
	  }
	} else {  /* A '/' -terminated path. */
	  strcat( path,ToUser );
	  strcat( path,"/" );
	}
	return path;
}
