/* etbwriter -- FUNET-NJE 3.0
   -- quick hack to write to a file the current ASCII-EBCDIC tables

   Copyright (C)  :-)  Finnish University and Research Network, FUNET
 */

#include <stdio.h>
#define MAIN -1
#include "ebcdic.h"

main()
{
  fprintf(stdout,"ASC<=>EBC\n");
  fwrite(ASCII_EBCDIC,1,256,stdout);
  fwrite(EBCDIC_ASCII,1,256,stdout);
  return 0;
}
