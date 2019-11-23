/*	FUNET-NJE/HUJI-NJE		Client utilities
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 26-Sep-1993
 */


#include "prototypes.h"
#include "clientutils.h"

/* Uread() -- for a couple of external routines of HUJI-NJE */
int
Uread( buf,size,fd)
     void *buf;
     const int size;
     FILE *fd;
{
	short NewSize;
	long filepos = ftell(fd);

	if (fread(&NewSize, sizeof(NewSize), 1, fd) != 1)
	  return 0;	/* Probably end of file */
	NewSize = ntohs(NewSize);
	if (NewSize > size) {	/* Can't reduce size, so can't recover */
	  logger(1, "UREAD: have to read %d into a buffer of only %d, filepos=%d\n",
		 NewSize, size, filepos);
	  bug_check("Uread - buffer too small");
	}
	if (fread(buf, NewSize, 1, fd) == 1) {
	  return NewSize;
	}
	return 0;
}
