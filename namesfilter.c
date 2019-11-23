/*
 *  namesfilter  -- generate Zmailer's  routes.bitnet  database
 *		    by parsing BITEARN.NODES node database
 *
 *  Matti Aarnio <mea@nic.funet.fi> 1994-Sep-27
 *
 *  Format of the BITEARN NODES -file is described (well, partially)
 *  in the  NEWTAGS DESCRIPT  available from your nearest NETSERV..
 */

#include <stdio.h>
#include "sysexits.h"

extern char **namesparse(/*FILE *namesfile, char **picktags, char *selector*/);

char *progname = "<namesfilter>";

void
usage(msg)
	char *msg;
{
	if (msg) {
	  fprintf(stderr,"%s: %s\n",progname,msg);
	  exit(EX_USAGE);
	}
	fprintf(stderr,"\
%s:  Filters BITEARN.NODES data to mailer databases\n\
     namesfilter -zmailer  < bitearn.nodes | sort > bitnet.routes\n\
     namesfilter -smail    < bitearn.nodes | sort > bitnet.routes\n\
     namesfilter -sendmail < bitearn.nodes | sort > bitnet.routes\n\
     namesfilter -netsoft  < bitearn.nodes | sort > netsoft.list\n",
		progname);
	exit(EX_USAGE);
}

extern char *strchr(), *strrchr();
extern int strncmp();

#define FMT_ZMAILER  1
#define FMT_SENDMAIL 2
#define FMT_SMAIL    3
#define FMT_NETSOFT  4

void strlower(s)
	char *s;
{
	while (*s != 0) {
	  if (*s >= 'A' && *s <= 'Z')
	    *s += ('a' - 'A');
	  ++s;
	}
}

int
main(argc,argv)
	int argc;
	char *argv[];
{
	char **np;
	static char *picklist[] = { "node.","internet.",
				      "servers1.", "servers2.", "servers3.",
				      "servers4.", "servers5.", "servers6.",
				      "servers7.", "servers8.", "servers9.",
				      "servers10.","servers11.","servers12.",
				      NULL };
	static char *selector   = ":type.NJE";
	char *node;
	char *mailer;
	char *internet;
	char *netsoft;
	int i;
	int outfmt;
	char *smailtail = "";

	node = (char*)strrchr(argv[0],'/');
	if (node) progname = node+1;

	if (argc != 2) usage(NULL);
	if (strcmp(argv[1],"-zmailer")==0) {
	  outfmt = FMT_ZMAILER;
	} else if (strcmp(argv[1],"-smail")==0) {
	  outfmt = FMT_SMAIL;
	} else if (strcmp(argv[1],"-sendmail")==0) {
	  outfmt = FMT_ZMAILER; /* At least P.Bryant uses this same format.. */
	} else if (strcmp(argv[1],"-netsoft")==0) {
	  outfmt = FMT_NETSOFT;
	  /* picklist[0] == "node." */
	  picklist[1] = "netsoft.";
	  picklist[2] = NULL;
	} else {
	  usage("Unknown format specifier!  Valid ones: -zmailer, -sendmail, -smail");
	}
	
	if (outfmt == FMT_SMAIL)
	  smailtail = "!%s";
	else
	  smailtail = "";

	/* The FIRST one is a version stamp, we skip it.. */
	np = namesparse(stdin,picklist,NULL);

	while (!feof(stdin) && !ferror(stdin)) {
	  np = namesparse(stdin,picklist,selector);
	  if (!np) break;

	  node     = NULL;
	  mailer   = NULL;
	  internet = NULL;
	  netsoft  = NULL;
	  for (i=0; np[i] != NULL; ++i) {
	    if (strncmp(np[i],":node.",6)==0)
	      node = np[i]+6;
	    else if (strncmp(np[i],":internet.",10)==0)
	      internet = np[i]+10;
	    else if (strncmp(np[i],":servers",8)==0) {
	      /* There can be a LOT of ":serversN." -tags */
	      char *s = np[i]+8;
	      while ((*s >= '0' && *s <= '9') || *s == '.') ++s;
	      mailer = s;
	      while (*s != 0) {
		/* Server's that are OUTONLY shall not be used
		   for destination.. */
		if (*s == 'O') {
		  if (strncmp("OUTONLY",s,7)==0) {
		    mailer = NULL;
		    break;
		  }
		}
		++s;
	      }
	    } else if (strncmp(np[i],":netsoft.",9)==0)
	      netsoft = np[i]+9;
	  }
	  strlower(node);
	  printf("%s\t",node);
	  if (strlen(node)<8)
	    putc('\t',stdout);

	  if (outfmt == FMT_NETSOFT) {

	    if (netsoft == NULL)
	      netsoft = "(unlisted)";
	    printf("%s\n",netsoft);

	  } else {	/* All MAILER formats */

	    if (!mailer) {
	      printf("defrt1!%s%s\n",node,smailtail);
	    } else {
	      /* Some mailer defined, what it is ? */
	      char *m0, *m1, *m2, *m3, *m4;
	      int netdata = 0;
	      int rfc822  = 0;
	      m0 = mailer;	/* The MAILER definition! */
	      while (*mailer != 0 && *mailer != '(') ++mailer;
	      if (*mailer == '(') *mailer++ = 0;
	      m1 = strchr(m0,'@'); if (m1 != NULL) *m1 = '!';
	      m1 = mailer;	/* "MAIL", ignored.. */
	      while (*mailer != 0 && *mailer != ',') ++mailer;
	      if (*mailer == ',') *mailer++ = 0;
	      m2 = mailer;	/* PU, ND, LP .. format, default: PU */
	      while (*mailer != 0 && *mailer != ',') ++mailer;
	      if (*mailer == ',') *mailer++ = 0;
	      m3 = mailer;	/* Class, ignored */
	      while (*mailer != 0 && *mailer != ',') ++mailer;
	      if (*mailer == ',') *mailer++ = 0;
	      m4 = mailer;	/* BSMTP, BSMTP RFC822, default: BSMTP */
	      while (*mailer != 0 && *mailer != ',' && *mailer != ')') ++mailer;
	      /* It is split.. */
	      while (*m2 != 0) {
		if (strncmp(m2,"ND",2)==0) {
		  netdata = 1;
		  ++m2;
		}
		++m2;
	      }
	      while (*m4 != 0) {
		if (strncmp(m4,"BSMTP",5)==0) {
		  m4 += 4;
		} else if (strncmp(m4,"RFC822",6)==0) {
		  rfc822 = 1;
		  m4 += 5;
		}
		++m4;
	      }
	      strlower(m0);
	      /* The entry is parsed, output it.. */

	      if (netdata == 0 && rfc822 == 0)
		printf("bsmtp3!%s%s\n",m0,smailtail);
	      else if (netdata == 0 && rfc822 != 0)
		printf("bsmtp3rfc!%s%s\n",m0,smailtail);
	      else if (netdata != 0 && rfc822 == 0)
		printf("bsmtp3nd!%s%s\n",m0,smailtail);
	      else
		printf("bsmtp3ndrfc!%s%s\n",m0,smailtail);
	    }
	  }
	}
	return 0;
}
