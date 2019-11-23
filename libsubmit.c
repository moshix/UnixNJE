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

int
submit_file(FileName, FileSize)
const char	*FileName;
int	FileSize;
{
	unsigned char	line[512];
	long	size;

	FileSize /= 512; /* system will multiply it again.. */

	*line = CMD_QUEUE_FILE;
	/* Send file size */
	line[1] = (unsigned char)((FileSize & 0xff00) >> 8);
	line[2] = (unsigned char)(FileSize & 0xff);
	strcpy(&line[3], FileName);
	size = strlen(&line[3]) + 4; /* Include terminating NULL */

	return send_cmd_msg(line, size, 1); /* Offline is ok */
}
