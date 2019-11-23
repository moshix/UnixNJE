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

/*
 |  Write the given string into the file.
*/

int
Uwrite(outfil, string, size)
FILE *outfil;
const void	*string;	/* Line descption */
const int	size;
{
	unsigned short Size = htons(size);

	if(fwrite(&Size, 1, sizeof(Size), outfil) != sizeof(Size)) {
		logger(1, "UWRITE: Can't fwrite, error: %s\n", PRINT_ERRNO);
		return 0;
	}
	if(fwrite(string, 1, size, outfil) != size) {
		logger(1, "UWRITE: Can't fwrite, error: %s\n", PRINT_ERRNO);
		return 0;
	}
	return 1; /* Done ok */
}
