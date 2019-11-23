/* etbwriter2 -- FUNET-NJE 3.0
   -- quick hack to write the current ASCII-EBCDIC tables into nroff
      format for making man-pages..

   Copyright (C)  :-)  Finnish University and Research Network, FUNET
 */

#include <stdio.h>
#define MAIN -1
#include "ebcdic.h"

main()
{
  int i;

  for (i=0; i<256; ++i) {
    if ((i % 8) == 0) printf("\\f4 0x%02X |",i);
    printf("0x%02X|",ASCII_EBCDIC[i]);
    if ((i % 8) == 7) printf("\n");
  }
  for (i=0; i<256; ++i) {
    if ((i % 8) == 0) printf("\\f4 0x%02X |",i);
    printf("0x%02X|",EBCDIC_ASCII[i]);
    if ((i % 8) == 7) printf("\n");
  }
  return 0;
}
