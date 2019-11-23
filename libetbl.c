/*	FUNET-NJE		Client utilities
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 26-Sep-1993
 */

#include "prototypes.h"

# define MAIN -1		/* We compile EBCDIC tables inline */
#include "ebcdic.h"

extern char DefaultEBCDICTBL[];

int EtblAltered = 0;

void
read_etable()
{
	char *etablep;
	int fd;
	unsigned char ebuf[8+512+10];
	int rc;
	char *ecvar = "Environment";

	if (EtblAltered) return;
	EtblAltered = 1;

	/* If environment says there is a table, try to read it in.. */
	if (!(etablep = getenv("EBCDICTBL"))) {
	  if (*DefaultEBCDICTBL == 0)
	    return;
	  etablep = DefaultEBCDICTBL;
	  ecvar = "Configuration";
	}

	fd = open(etablep,O_RDONLY,0);
	if (fd < 0) {
	  fprintf(stderr,"LIBETBL: %s variable EBCDICTBL defined, but its value `%s' can't be opened as a file.\n",ecvar,etablep);
	  return;
	}
	rc = read(fd,ebuf,sizeof(ebuf));
	close(fd);
	if (rc != 522 ||
	    memcmp("ASC<=>EBC\n",ebuf,10) != 0) {
	  fprintf(stderr,"LIBETBL: %s variable EBCDICTBL defined, but file pointed by it is not of proper format.\n",ecvar);
	  return;
	}
	memcpy(ASCII_EBCDIC,ebuf+10,    256);
	memcpy(EBCDIC_ASCII,ebuf+10+256,256);
}
