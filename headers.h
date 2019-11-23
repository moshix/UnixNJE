/* HEADERS.H   V1.3-mea
 | Copyright (c) 1988,1989,1990,1991,1992 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.
 |
 | The definition of the structures used by NJE.
 |
 | Document:
 |   Network Job Entry - Formats and Protocols (IBM)
 |	SC23-0070-01
 |	GG22-9373-02 (older version)
 |
 */

/* Some hard-ware dependencies */
#ifndef __U_INT32
#if	defined(__alpha__) /* 64 bit.. */
#define __U_INT32
typedef unsigned int u_int32;
typedef int int32;
typedef unsigned short u_int16;
typedef short int16;
#else
#define __U_INT32
typedef unsigned long u_int32;
typedef long int32;
typedef unsigned short u_int16;
typedef short int16;
#endif
#endif

#include "ebcdic.h"

/* The RSCS version we emulate (currently 1.3) --  [mea] 2.1 ! */
#define	RSCS_VERSION	2
#define	RSCS_RELEASE	1

/* The Enquiry block */
struct	ENQUIRE {
		unsigned char	soh, enq, pad;	/* 3 characters block */
	} ;

/* The negative ACK block */
struct	NEGATIVE_ACK {
		unsigned char	nak, pad;
	} ;

/* The positive ACK */
struct	POSITIVE_ACK {
		unsigned char	dle, ack, pad;
	} ;

/* Final signoff */
struct	SIGN_OFF {
		unsigned char	RCB, SRCB, pad;
	};

/* End of File block */
struct	EOF_BLOCK {
		unsigned char	RCB, SRCB, F1, F2;
	};

/* Permit reception of file */
struct	PERMIT_FILE {
		unsigned char	RCB, SRCB, SCB, END_RCB;
	};

/* Ack transmission complete */
struct	COMPLETE_FILE {
		unsigned char	RCB, SRCB, SCB, END_RCB;
	};

/* Reject a file request */
struct	REJECT_FILE {
		unsigned char	RCB, SRCB, SCB, END_RCB;
	};

struct	JOB_HEADER {
		unsigned short	LENGTH;	/* The header which comes before */
		unsigned char	FLAG,
				SEQUENCE;
		unsigned short	LENGTH_4;	/* Length of general section */
		unsigned char	ID,		/* Section ident             */
				MODIFIER;	/* The record itself         */
/* M */		unsigned short	NJHGJID;	/* Job Ident                 */
		unsigned char	NJHGJCLS,	/* Job Class                 */
				NJHGMCLS,	/* Message Class             */
				NJHGFLG1,	/* Flags                     */
/*
   NJHGFLG1	0x80	Don't recompute priority
		0x40	NJHGJID field is set
		0x08	suppress forwarding message
		0x04	suppress acceptance message
*/
				NJHGPRIO,	/* Selection Priority        */
				NJHGORGQ,	/* Orig node system qual.    */
				NJHGJCPY,	/* Job copy count            */
				NJHGLNCT,	/* Job line count            */
				r1, r2, r3,	/* Reserved                  */
/* M */				NJHGACCT[8],	/* Networking account number */
/* M */				NJHGJNAM[8],	/* Job Name                  */
/* M */				NJHGUSID[8],	/* Userid (TSO,VM/SP)        */
/* M */				NJHGPASS[8],	/* Password                  */
/* M */				NJHGNPAS[8];	/* New Password              */
/* M */		u_int32		NJHGETS[2];	/* Entry Date/Time Stamp     */
/* M */		unsigned char	NJHGORGN[8],	/* Origin node name          */
/* M */				NJHGORGR[8],	/* Origin remote name        */
/* M */				NJHGXEQN[8],	/* Execution node name       */
/* M */				NJHGXEQU[8],	/* Execution user ID (VM/SP) */
/* M */				NJHGPRTN[8],	/* Default print node name   */
/* M */				NJHGPRTR[8],	/* Default print remote name */
/* M */				NJHGPUNN[8],	/* Default punch node name   */
/* M */				NJHGPUNR[8],	/* Default punch remote name */
/* M */				NJHGFORM[8];	/* Job forms                 */
		u_int32		NJHGICRD,	/* Input card count          */
	 	 		NJHGETIM,	/* Estimated execution time  */
				NJHGELIN,	/* Estimated output lines    */
				NJHGECRD;	/* Estimated output cards    */
/* M */		unsigned char	NJHGPRGN[20],	/* Programmers name          */
/* M */				NJHGROOM[8],	/* Programmers room number   */
/* M */				NJHGDEPT[8],	/* Prgmr's dept. number      */
/* M */				NJHGBLDG[8];	/* Prgmr's building number   */
		u_int32		NJHGNREC;	/* Record count on output    */
		};

struct	DATASET_HEADER_G {		/* General section */
	/*  4*/	unsigned short	LENGTH_4;
	/*  6*/	unsigned char	ID,		/* ID = X'00'		     */
	/*  7*/			MODIFIER;	/* MODIFIER = X'00'	     */
/* M */	/*  8*/	unsigned char	NDHGNODE[8],	/* Destination node name     */
	/* 16*/			NDHGRMT[8],	/* Destination remote name   */
/* M */	/* 24*/			NDHGPROC[8],	/* Proc invocation name	     */
/* M */	/* 32*/			NDHGSTEP[8],	/* Step name 		     */
/* M */	/* 40*/			NDHGDD[8];	/* DD name		     */
	/* 48*/	unsigned short	NDHDSNO;	/* Data set number	     */
	/* 50*/	unsigned char	r1,		/* Reserved		     */
	/* 51*/			NDHGCLAS;	/* Output class		     */
/* M */	/* 52*/	u_int32		NDHGNREC;	/* Record count		     */
	/* 56*/	unsigned char	NDHGFLG1,	/* Flags		     */
/*
   NDHGFLG1	0x80	spin dataset
		0x40	hold dataset at destination
		0x20	JOB LOG indication (file is a log of SYSIN job)
		0x10	page overflow indication
		0x08	punch interpret indication (file is a punch)
		0x04    ??? (seen set, purpose unknown)
 */
	/* 57*/			NDHGRCFM;	/* Record Format	     */
/* M */	/* 58*/	unsigned short	NDHGLREC;	/* Max logical record length */
	/* 60*/	unsigned char	NDHGDSCT,	/* Data set copy count	     */
	/* 61*/			NDHGFCBI,	/* 3211 FCB index	     */
	/* 62*/			NDHGLNCT;
	/* 63*/	unsigned char	r2,		/* Reserved		     */
/* M */	/* 64*/			NDHGFORM[8],	/* Forms ID		     */
/* M */	/* 72*/			NDHGFCB[8],	/* FCB ID		     */
/* M */	/* 80*/			NDHGUCS[8],	/* UCS ID		     */
/* M */	/* 88*/			NDHGXWTR[8],	/* External writer ID	     */
	/* 96*/			r3[8];
	/*104*/	unsigned char	NDHGFLG2,	/* 2nd flag byte	     */
/*
   NDHGFLG2	0x80	dataset is to be printed
		0x40	dataset is to be punched
		0x20..	reserved
 */
	/*105*/			NDHGUCSO,	/* UCS option byte	     */
/*
   NDHGUCSO	0x80	block datacheck option
		0x40	UCS fold option
 */
	/*106*/			r4[2],
/* M */	/*108*/			NDHGPMDE[8];
	/*116*/	};

struct	DATASET_HEADER_RSCS {		/* RSCS section */
	/*116*/	unsigned short	LENGTH_4;
	/*118*/	unsigned char	ID,
	/*119*/			MODIFIER;
	/*120*/	unsigned char	NDHVFLG1,	/* Flags		     */
/*
   NDHVFLG1	0x80	file created by *LIST processor
		0x40	first DSH of its kind
		0x20	personalized section
		0x10	???
		0x08	suppress forwarding messages
		0x04	suppress acceptance messages
		0x02	suspend all active datasets (LIST processors only)
		0x01	resume all active datasets (LIST processors only)
 */
	/*121*/			NDHVCLAS,	/* VM/SP spool file class    */
	/*122*/			NDHVIDEV,	/* VM/SP origin device type  */
	/*123*/			NDHVPGLE,	/* VM/SP virtual 3800 page
						   length		     */
	/*124*/			NDHVDIST[8],	/* VM/SP DIST code	     */
	/*132*/			NDHVFNAM[12],	/* VM/SP file name	     */
	/*144*/			NDHVFTYP[12];	/* VM/SP file type	     */
	/*156*/	unsigned short	NDHVPRIO;	/* VM/SP transmission prty   */
	/*158*/	unsigned char	NDHVVRSN,	/* RSCS version number of
						   headers writer	     */
	/*159*/			NDHVRELN;	/* RSCS release number of
						   headers writer	     */
	/*160*/	unsigned char	NDHVTAGR[136];	/* RSCS TAG data	     */
	/*296*/ };

struct	DATASET_HEADER {	/* Includes the General section,
				   and the RSCS section */
	/*  0*/	unsigned short	LENGTH;
	/*  2*/	unsigned char	FLAG,
	/*  3*/			SEQUENCE;
	/*  4*/	struct DATASET_HEADER_G    NDH;
	/*116*/	struct DATASET_HEADER_RSCS RSCS;
	/*296*/	};

struct	JOB_TRAILER {
		unsigned short	LENGTH;
		unsigned char	FLAG,
				SEQUENCE;
		unsigned short	LENGTH_4;
		unsigned char	ID,
				MODIFIER;
		unsigned char	NJTGFLG1,
				NJTGXCLS,	/* Actual exec class         */
				r1[2];
		u_int32		NJTGSTRT[2],	/* Exec start time/date      */
				NJTGSTOP[2],	/* Exec stop time/date       */
				NJTGACPU,	/* Actual CPU time           */
/* M */				NJTGALIN,	/* Actual output lines       */
/* M */				NJTGCARD,	/* Actual output cards       */
				NJTGEXCP;	/* Excp count                */
		unsigned char	NJTGIXPR,	/* .. XEQ selection prty     */
				NJTGAXPR,	/* "                         */
				NJTGIOPR,	/* ... output selection prty */
				NJTGAOPR;	/* "                         */
		} ;


struct	SIGNON {		/* Initial and response */
		unsigned char	NCCRCB,		/* Gen rec ctrl byte	     */
				NCCSRCB,	/* Sub-rec ctrl byte	     */
				NCCIDL,		/* Length of logical rec.    */
/* M */				NCCINODE[8],	/* Node ident.		     */
				NCCIQUAL;	/* Qual if shared spool	     */
		u_int32		NCCIEVNT;	/* Event seq number	     */
		unsigned short	NCCIREST,	/* Partial node to node 
						   resistance		     */
/* M */				NCCIBFSZ;	/* Max transmission blk size */
/* M */		unsigned char	NCCILPAS[8],	/* Line password	     */
/* M */				NCCINPAS[8],	/* Node password	     */
				NCCIFLG;	/* Feature flags (X'80')     */
		u_int32		NCCIFEAT;
	};


/* Nodal messages records: */
struct	NMR_MESSAGE {		/* Message */
		unsigned char	NMRFLAG,	/* Flag byte		     */
/*
   NMRFLAG	0x80	Command vs. message (set means: command)
   		0x40	NMROUT from JES2, with JES2 RMT number
		0x20	NMROUT has user ID -- common case
		0x10	NMROUT has UCMID information (?)
		0x08	console is only remote authorized
		0x04	console is not job authorized
		0x02	console is not device authorized
		0x01	console is not system authorized
 */
				NMRLEVEL,	/* Importance level/output
						   priority		     */
				NMRTYPE,	/* Type byte		     */
/*
   NMRTYPE	0x08	NMROUT contains control info
   		0x04	msg text-only in NMRMSG
		0x02	formatted command in NMRMSG
 */
				NMRML,		/* Length of message	     */
				NMRTONOD[8],	/* To node name		     */
				NMRTOQUL,	/* To node qualifier	     */
				NMRUSER[8],	/* To user		     */
				NMRFMNOD[8],	/* From node name	     */
				NMRFMQUL,	/* From node qualifier	     */
				NMRMSG[132];	/* Message		     */
				/* NMROUT is the first 8 chars of the NMRMSG */
	};

/*
   Section ID codes:

	0x00	General section
			MODIFIER = 0x80 -> 3800 section
			MODIFIER = 0x40 -> record characteristics
			MODIFIER = 0x00 -> general section
	0x80	Unspecified subsystem
	0x81	ASP subsystem
	0x82	HASP subsystem
	0x83	JES/RES subsystem
	0x84	JES2 subsystem
	0x85	JES3 subsystem
	0x86	POWER/VS subsystem
	0x87	VM/370 subsystem
			MODIFIER = 0x00 -> RSCS
	0x89	Datastream section
			MODIFIER = 0x00 -- seen on MVS JOBLOG outputs
	0x89	Accounting section (in NJT)
	0x8A	Scheduling section (in NJH)
	0xC0	User-defined section
			MODIFIER = 0x00 -> user-section
*/
