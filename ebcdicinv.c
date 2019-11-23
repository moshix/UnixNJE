/*
 * ASCII_EBCDIC table verify & interter -- tool
 */

#include <stdio.h>
#define MAIN
#include "ebcdic.h"


int
main(argc,argv)
int argc;
char *argv[];
{
	int i, c;
	for (i=0; i < 256; ++i)
	  EBCDIC_ASCII[i] = 0;

	for (i=0; i < 256; ++i) {
	  c = ASCII_EBCDIC[i];
	  if (EBCDIC_ASCII[c] != 0) {
	    printf("Duplicate map: A=%03o->E=%03o & A=%03o->E=%03o\n",
		   i,c,EBCDIC_ASCII[c],c);
	  }
	  EBCDIC_ASCII[c] = i;
	}

	for (i=0; i < 256; ) {
	  printf("\t");
	  for (c = 0; c < 8; ++c)
	    printf("0x%02x, ",EBCDIC_ASCII[i+c]);
	  printf("\t\t/* %03o .. %03o */\n",i,i+7);
	  i += 8;
	}
	for (i=1; i<256; ++i)
	  if (EBCDIC_ASCII[i] == 0)
	    printf("Inversion failure - EBCDIC-code 0x%02x not listed on ASCII->EBCDIC table\n",i);

	return 0;
}
