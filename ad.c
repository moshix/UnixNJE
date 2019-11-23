#include <stdio.h>
#define MAIN -1
#include "ebcdic.h"

unsigned char prttable[256];
int HUJI = 0;  /* Show EBCDIC... after header */


main(argc, argv)
int argc;
char *argv[];
{
    FILE *fp;
    int i;

/*  for(i=0;i<256;++i) prttable[i] = '.';
    for(i=32;i<127;++i) prttable[i] = i;
    for(i=128;i<176;++i) prttable[i] = i;
    for(i=224;i<254;++i) prttable[i] = i;*/
#define isprint(c) ( ((c) >=32) && ((c) <= 126) )
    for(i=0;i<256;++i) { prttable[i] = '.';
			 if (isprint(i)) prttable[i] = i; }

    if (argc > 1)
      if (strcmp(argv[1],"-H")==0) {
	HUJI += 1;
	++argv;
	--argc;
      }
    if (argc > 1)
      if (strcmp(argv[1],"-H")==0) {
	HUJI += 1;
	++argv;
	--argc;
      }

    if (argc == 1) {
	dofile(stdin);
	exit (0);
    }
    while (--argc) {
	argv++;
	if ((fp = fopen(*argv, "r")) == NULL) {
	    perror(*argv);
	    continue;
	}
	dofile(fp);
	fclose(fp);
    }
    exit(0);
}

dofile (fp)
FILE *fp;
{
    int adr, n, i;
    unsigned char buf[16];
    int EbcdicDump = 0;
    unsigned char ENDM[4],c;

    if (HUJI>1) EbcdicDump = 1;

    adr = 0;
    while (1) {
      if ((n = fread(buf, 1, 16, fp)) != 16)
	break;

      printf ("%05x  ", adr);
      for (n=0; n<16; n++)
	printf ("%02x ", buf[n] & 0xff);

      printf (" +");
      if (!EbcdicDump)
	for (n=0; n<16; n++)
	  printf( "%c",prttable[buf[n]] );
      else
	for (n=0; n<16; n++) {
	  c = prttable[EBCDIC_ASCII[buf[n]]];
	  if (!isprint(c)) c = '.';
	  printf( "%c",c );
	}
      
      printf ("+\n");
      adr += 16;
      if (HUJI && (EbcdicDump == 0)) {
	if (adr > 130 ) EbcdicDump = 1;
      }
      
    }
    if ((n > 0) && (n < 16)) {
	printf ("%05x  ", adr);
	for (i=0; i<n; i++)
	  printf ("%02x ", buf[i] & 0xff);
	i = n;
	while (i++ < 16)
	  printf ("   ");

	printf (" +");
	if (!EbcdicDump)
	  for (n=0; n<16; n++)
	    printf( "%c",prttable[buf[n]] );
	else
	  for (n=0; n<16; n++) {
	    c = prttable[EBCDIC_ASCII[buf[n]]];
	    if (!isprint(c)) c = '.';
	    printf( "%c",c );
	  }
	printf ("+\n");
    }
}
