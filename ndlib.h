/*	FUNET-NJE		Client utilities Header file
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  14-Nov-1993
 */
/* NETDATA stuff.. */

#ifndef	__
# ifdef	__STDC__
#  define __(x) x
# else
#  define __(x) ()
#  define volatile /*K&R has no volatile */
#  define const    /*K&R has no const    */
# endif
#endif

struct puncher {
	FILE	*fd;
	char	buf[82];
	int	len;
	int	punchcnt;
};

extern int	punch_buffered __((struct puncher *PUNCH, void *buf,
				   int size, int flushflg));

/*  Values of  recfm  for netdata generation, OR them together..  */
#define	ND_VAR		0x4000	/* Records within block: Variable */
#define ND_VARY		0x0002	/* Varying length records w/o 4 byte length */
#define ND_FIXED	0x8000	/* Fixed length records */
#define ND_BLOCKED	0x1000	/* Blocked records */
#define	ND_ASA_CC	0x4000	/* ASA carriage control */

extern int	fill_inmr01 __((struct puncher *PUNCH, FILE *infile,
				char *From, char *To, char *fname, char *ftype,
				int lrecl, int recfm, char *askack));
extern int	fill_inmr06 __((struct puncher *PUNCH));
extern int	punch_nddata __((struct puncher *PUNCH, void *buf, int size));

extern int	do_netdata __((FILE *infile, struct puncher *PUNCH,
			       char *From, char *To, char *fname, char *ftype,
			       int lrecl, int recfm, int binary,char *askack));

struct ndparam {
	FILE *outfile;
	char const *outname;
	char *defoutname;
	int  profsnote;
	int  ackwanted;
	char ACKKEY[40];
	int  ackkeylen;
	void (*outproc)();
	void (*closeproc)();
	char DSNAM[40];
	char MEMBR[10];
	char FTIMES[16];
	time_t ftimet;
	char CTIMES[16];
	time_t ctimet;
	int  RecordsCount;
};

extern int	parse_netdata __((unsigned char *buffer, int size,
				  struct ndparam *ndp,
				  int binary, int debugdump));
