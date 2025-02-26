//NJEQUEUE JOB (FUNET-NJE),
//             'Send NJE Data',
//             CLASS=A,
//             MSGCLASS=Z,
//             MSGLEVEL=(0,0)
//*
//********************************************************************
//*
//* Name: mvs38_recv.jcl
//*
//* Desc: Send confirmation message to the originating TSO user after
//*       having transmitted a card deck to the FUNET-NJE spool.
//*
//********************************************************************
//*
//INFORM  EXEC PGM=IKJEFT01
//SYSPRINT DD  SYSOUT=*
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
send 'XFID enqueued for - 
XTONODE(XTOUSER) -- origin XFRNODE(XFRUSER) -
XDATE', -
user(XFRUSER),LOGON
/*
//
