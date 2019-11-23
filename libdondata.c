/*	FUNET-NJE/HUJI-NJE		Client utilities
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 26-Sep-1993
 */


#include <stdio.h>
#include "prototypes.h"
#include "clientutils.h"
#include "ndlib.h"

int
do_netdata(infile, PUNCH, From, To, fname, ftype, lrecl, recfm, binary, askack)
FILE	*infile;
struct puncher *PUNCH;
char	*From, *To, *fname, *ftype, *askack;
int	lrecl, recfm, binary;
{
	char	*linebuf = malloc(lrecl+8);
	int	inputsize = 0;

	/* ================ Make INMR headers ================ */

	if (!fill_inmr01(PUNCH, infile, From, To,
			 fname, ftype, lrecl, recfm, askack))
	  return 0;

	/* ================ While there is input.. ================ */

	while (!feof(infile) && !ferror(infile)) {

	  /* ================ Collect one datarecord ================ */

	  if (binary) {
	    if ((inputsize = fread(linebuf+2,1,lrecl,infile)) == 0)
	      break;					/* Read until EOF */
	  } else {
	    if (!fgets(linebuf+2,lrecl,infile)) break;	/* Read until EOF */
	    inputsize = strlen(linebuf+2);
	    if (inputsize)		/* ASCII: Zap trailing newline.. */
	      if (linebuf[inputsize-1+2] == '\n')
		--inputsize;
	    linebuf[inputsize+2] = ' ';
	    if (inputsize == 0) {	/* Must have at least one byte! */
	      inputsize = 1;
	      linebuf[0+2] = ' ';
	    }
	    ASCII_TO_EBCDIC(linebuf+2,linebuf+2,inputsize);
	  }

	  /* ================ Output that record ================ */

	  if (!punch_nddata(PUNCH,linebuf,inputsize))
	    return 0;

	}

	/* ================ Data done, EOF record ================ */

	if (!fill_inmr06(PUNCH)) return 0;

	free(linebuf);
	return 1; /* Punched it ok. */
}
