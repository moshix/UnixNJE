/*	FUNET-NJE		Client utilities
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

/* Always usefull:  strsave() */

char *strsave(str)
char *str;
{
	int len = strlen(str);
	char *ss = (char*)malloc(len+1);

	if (ss)
	  memcpy(ss,str,len+1);

	return ss;
}
