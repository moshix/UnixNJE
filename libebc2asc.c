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

extern int EtblAltered;

void
EBCDIC_TO_ASCII(INSTRING, OUTSTRING, LENGTH)
const void *INSTRING;
void *OUTSTRING;
const int LENGTH;
{
	int TempVar;
	unsigned char *op = OUTSTRING;
	const unsigned char *ip = INSTRING;

	if (!EtblAltered) read_etable();

	for(TempVar = 0; TempVar < LENGTH; TempVar++)
		op[TempVar] = EBCDIC_ASCII[ ip[TempVar] ];
}
