/*	FUNET-NJE		Client utilities
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 26-Sep-1993
 */


#include <stdio.h>
#include "clientutils.h"
#include <ctype.h>

/* Always usefull:  upperstr() */

char *upperstr(str)
char *str;
{
	register char *s = str;
	while (s && *s) {
	  if (islower(*s))
	    *s = toupper(*s);
	  ++s;
	}
	return str;
}
