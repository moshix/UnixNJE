//XTOUSERR JOB (FUNET-NJE),
//             'Read NJE Data',
//             CLASS=A,
//             MSGCLASS=Z,
//             MSGLEVEL=(0,0)
//*
//********************************************************************
//*
//* Name: mvs38_send.jcl
//*
//* Desc: Read NJE card deck placed by mvs38_send script into
//*       the binary card reader and punch it to the MVS spool for
//*       end user retrieval.
//*
//********************************************************************
//*
//READNJE EXEC PGM=IEBGENER
//SYSUT1   DD  DISP=OLD,UNIT=10C,DCB=(RECFM=FB,LRECL=80,BLKSIZE=80)
//SYSUT2   DD  SYSOUT=X,HOLD=YES
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  DUMMY
//*
//INFORM  EXEC PGM=IKJEFT01
//SYSPRINT DD  SYSOUT=*
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
send 'File (XFID) spooled to XTOUSER -- origin -
XFRNODE(XFRUSER) XDATE',user(XTOUSER),LOGON
/*
//
