/*	FUNET-NJE/HUJI-NJE		Client utilities
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 26-Sep-1993
 */


#include <stdio.h>
#include "clientutils.h"
#include "ebcdic.h"

void
PAD_BLANKS(STRING, ORIG_SIZE, FINAL_SIZE)
void *STRING;
const int ORIG_SIZE, FINAL_SIZE;
{
	int TempVar;
	unsigned char *s = (unsigned char *)STRING;

	for (TempVar = ORIG_SIZE; TempVar < FINAL_SIZE; TempVar++)
	  s[TempVar] = E_SP;
}
