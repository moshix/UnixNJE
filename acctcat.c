/* acctcat -- Sample RSCS Accounting file processor for FUNET-NJE.
   By Matti Aarnio <mea@nic.funet.fi> <mea@finfiles.bitnet>
   (c) 1993, 1994 Finnish University and Research Network, FUNET
 */


#include "prototypes.h"
#include "rscsacct.h"
#include "clientutils.h"

char LOCAL_NAME[80];
char BITNET_QUEUE[80];

void
usage()
{
	fprintf(stderr,"Usage:  acctcat /path/to/rscs/acct-file\n");
	exit(2);
}

int
main(argc,argv)
const int argc;
const char *argv[];
{
	struct RSCSLOG RL,RLa;
	FILE *logfil = NULL;

	if (argc != 2) usage();

	logfil = fopen(argv[1],"r");
	if (!logfil) usage();

	read_configuration();
	read_etable();

	while (fread(&RL,1,sizeof(RL),logfil) == sizeof(RL)) {
	  /* See that it is RSCS accounting entry (EBCDIC "C0") */
	  if (RL.ACNTTYPE[0] != 0xC3 || RL.ACNTTYPE[1] != 0xF0) continue;

	  EBCDIC_TO_ASCII(&RL,&RLa,sizeof RL);

	  /* Early on there was a mistake, and seconds were lost.. */
	  if (RLa.ACNTDATE[10] < '0' || RLa.ACNTDATE[10] > '9' ||
	      RLa.ACNTDATE[11] < '0' || RLa.ACNTDATE[11] > '9') {
	    RLa.ACNTDATE[10] = '0';
	    RLa.ACNTDATE[11] = '0';
	  }

	  printf("%c %-12.12s %04d %-8.8s %-8.8s %04d -> %-8.8s %-8.8s %s %c %5ld recs\n",
		 (RL.ACNTCODE == 1) ? 'T' : 'R', RLa.ACNTDATE,
		 ntohs(RL.ACNTID),
		 RLa.ACNTUSER, RLa.ACNTILOC,
		 ntohs(RL.ACNTOID),
		 RLa.ACNTTOVM, RLa.ACNTDEST,
		 (RL.ACNTINDV & 0xF0) == 0x80 ? "PUN" : "PRT", RLa.ACNTCLAS,
		 ntohl(RL.ACNTRECS));
	}
	fclose(logfil);
	return 0;
}
