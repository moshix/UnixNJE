/* A module for HUJINJE/FUNET -- rscsacct.c

   Log NJE accountting record in same format as IBM RSCSv1 uses
   
   By Matti Aarnio, and Gerald Hanusch.

 */

#include "consts.h"
#include "ebcdic.h"
#include "prototypes.h"
#include "rscsacct.h"



/* File for logging */
static int rscslog = -1;

static struct RSCSLOG RL;

void
rscsacct_open(logfilename)
const char *logfilename;
{
	if (rscslog >= 0) close(rscslog);

	rscslog = open(logfilename,O_RDWR|O_APPEND|O_CREAT,0644);

	/* logger(2,"RSCSACCT: open(%s) -> fd=%d\n",logfilename,rscslog); */

	/* Preset what you can */
	ASCII_TO_EBCDIC("HUJINJE ",RL.ACCTLOGU,8);
	memset(RL.filler, E_SP,2);
	memset(RL.filler2,E_SP,8);
	ASCII_TO_EBCDIC("UNIX ",RL.ACNTSYS,5); /* XX: Something portable ? */
	ASCII_TO_EBCDIC("C0",RL.ACNTTYPE,2);
}


void
rscsacct_close()
{
	if (rscslog >= 0) close(rscslog);
	rscslog = -1;
	/* logger(2,"RSCSACCT: close()\n"); */
}


void
rscsacct_log(FileParams,xmit_or_recv)
const struct FILE_PARAMS *FileParams;
const int xmit_or_recv;
{
	time_t now;
	struct tm *tm_var;
	char Abuf[20], *p;
	char frnode[10],frname[10],tonode[10],toname[10];

	/* logger(2,"RSCSACCT: log=%d, From=%s, To=%s, recs=%d\n",
	   rscslog,FileParams->From,FileParams->To,FileParams->RecordsCount); */

	if (rscslog < 0) return;	/* If no accounting file, quit.. */

	strncpy(frname,FileParams->From,9);
	frname[8] = 0;
	*frnode = 0;
	if ((p = strchr(frname,'@')) != NULL) *p = 0;
	if ((p = strchr(FileParams->From, '@')) != NULL) {
	  strncpy( frnode,p+1,sizeof(frnode)-1 );
	}
	strncpy(toname,FileParams->To,9);
	toname[8] = 0;
	*tonode = 0;
	if ((p = strchr(toname,'@')) != NULL) *p = 0;
	if ((p = strchr(FileParams->To, '@')) != NULL) {
	  strncpy( tonode,p+1,sizeof(tonode)-1 );
	}

	time(&now);
	tm_var = localtime(&now);

	strftime(Abuf,13,"%m%d%y%H%M%S",tm_var);
	ASCII_TO_EBCDIC(Abuf,RL.ACNTDATE,12);

	sprintf(Abuf,"%-8s",frname);
	ASCII_TO_EBCDIC(Abuf,RL.ACNTUSER,8);
	sprintf(Abuf,"%-8s",frnode);
	ASCII_TO_EBCDIC(Abuf,RL.ACNTILOC,8);
	sprintf(Abuf,"%-8s",toname);
	ASCII_TO_EBCDIC(Abuf,RL.ACNTTOVM,8);
	sprintf(Abuf,"%-8s",tonode);
	ASCII_TO_EBCDIC(Abuf,RL.ACNTDEST,8);

	RL.ACNTCLAS = ASCII_EBCDIC[(unsigned)FileParams->JobClass];
	RL.ACNTID   = htons(FileParams->FileId);
	RL.ACNTOID  = htons(FileParams->OurFileId);
	RL.ACNTINDV = (FileParams->type & F_PRINT) ? 0x41 : 0x81;
	RL.ACNTCODE = xmit_or_recv ? 1 : 2;

	RL.ACNTRECS = htonl(FileParams->RecordsCount);

	write(rscslog,(void*)&RL,80);
}

