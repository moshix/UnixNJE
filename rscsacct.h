/* RSCS accounting record  for  FUNET-NJE */

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


struct RSCSLOG {
/*  0 */	unsigned char ACCTLOGU[8];	/* Logger userid 'RSCSEARN' */
/*  8 */	unsigned char ACNTUSER[8];	/* originating location uid */
/* 16 */	unsigned char ACNTDATE[12];	/* yymmddhhmmss		    */
/* 28 */	short	      ACNTOID;		/* Origin spool file ID     */
/* 30 */	short	      ACNTID;		/* Local spool file ID      */
/* 32 */	unsigned char ACNTILOC[8];	/* originating location id  */
/* 40 */	unsigned char ACNTDEST[8];	/* destination location id  */
/* 48 */	unsigned char ACNTCLAS;		/* class letter		    */
/* 49 */	unsigned char ACNTINDV;		/* origin dev. type:
						    0x8n=Punch, 0x4n=Print  */
/* 50 */	char filler[2];			/* 2 filling blanks         */
/* 52 */	int32         ACNTRECS;		/* Number of records in file*/
/* 56 */	unsigned char ACNTTOVM[8];	/* Destination location uid */
/* 64 */	char filler2[8];		/* 8 filling blanks         */
/* 72 */	unsigned char ACNTSYS[5];	/* Sys id (serial+cpu model)*/
/* 77 */	unsigned char ACNTCODE;		/* transmission code;
						    01 = send, 02 = receive */
/* 78 */	unsigned char ACNTTYPE[2];	/* the CP assigned record type
						   for RSCS accounting C'C0'*/
/* 80 - end */
};
