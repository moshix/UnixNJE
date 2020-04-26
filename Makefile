#
#	Makefile for FUNET-NJE package.
#
#	This file is made by  mea@nic.funet.fi  while compiling
#	HUJI-NJE/FUNET-NJE on Sun4/330 SunOS4.0.3 (and later versions
#	of FINFILES machine)
#	This also contains a few other target systems accumulated over
#	the years..
#

#       -DHAS_PUTENV    The system has putenv() instead of setenv().
#                       This should be valid for most of the SysV based
#                       systems while the BSD based systems usually have
#                       setenv().  POSIX.1 doesn't say either way :-(
#	-DHAS_LSTAT	System has lstat(2) call in addition to stat(2).
#			(If you have it, USE IT!)
#	-D_POSIX_SOURCE	As POSIX.1 as possible..
#	-DDEBUG		Well, as name says..
#
#  Following are actually system defaults now, and defined on  consts.h:
#
#	-DNBCONNECT	Can do non-blocking connect(2) and associated
#			tricks.  WERRY USEFULL as the system won't have to
#			wait, and timeout synchronously for some dead system..
#	-DNBSTREAM	Does whole TCP stream in Non-blocking mode!
#			(*This is default mode set on  consts.h! *)
#	-DUSE_XMIT_QUEUE  Necessary for NBSTREAM!
#			(*This is default mode set on  consts.h! *)
#
#  In case you don't want the default stuff, you can define:
#
#	-DNO_NBSTREAM	If you want to block NBSTREAM and XMIT_QUEUE.
#
#  Other settable things:
#  
#	-DUSE_SOCKOPT	Does  setsockopt() for SO_RCVBUF, and SO_SNDBUF to
#			set them to 256k instead of the default whatever (4k?)
#			(*This is default mode set on  consts.h! *)
#	-DNO_SOCKOPT	Block  USE_SOCKOPT.
#	-DSOCKBUFSIZE	(Alter the default to be something.. Value in bytes)
#	-DNO_GETTIMEOFDAY Define if your system does not have it.
#
#	-DBSD_SIGCHLDS	Do SIGC(H)LD handling via a signal trapper.
#			Some (most?) SYSV's can safely ignore the child, but
#			BSDs (SunOS 4.1.3) can't.
#
#	-DUSE_ENUM_TYPES  If your compiler allows it, do it!
#			  Debugging is smarter..
#			(*This is default mode set on  consts.h! *)
#	-DNO_ENUM_TYPES  Negates previous
#
#
#	You can override a couple configuration things (these are defaults):
#	-DCONFIG_FILE='"/etc/funetnje.cf"'
#	-DPID_FILE='"/etc/funetnje.cf"'
#
#	If your system MAILER is to be called something else, than "MAILER":
#	-DMAILERNAME="$SYSMAIL"
#
#	Define following for Zmailer as a system mailer, othervice will use
#	/usr/lib/sendmail to send the email..  Needed ONLY by  mailify.c
#		-DUSE_ZMAILER -I/usr/local/include
#
#	-fno-builtin	Trouble with over-eager optimization on SPARC.
#			A fixed-size memcpy (2/4/8 bytes) was altered to
#			fetches and stored, but it dies on nonalignment..
#			(A typical CISC vs. RISC program trouble, says FSF
#			 and notes that it should not matter in well written
#			 program..)
#			8-Oct-93: Code fixed => this isn't needed anymore..
#
#	-DUSE_OWN_PROTOS  If the "prototypes.h" -prototypes for various things
#			can fit your system, and your system does not have
#			ANSI headers of its own, then use this...

# Convex OS V10.2 -- very POSIX.1 beast indeed..
#CC     = gcc -fno-builtin -fpcc-struct-return
#CPP    = gcc -E
#CDEFS  = -O -D_POSIX_SOURCE  -DHAS_LSTAT
#CFLAGS = -g $(CDEFS)
# Have MAILIFY compiled by uncommenting following ones:
#MAILIFY=mailify
##MAILIFYCFLAGS= $(CFLAGS) -DUSE_ZMAILER -I/usr/local/include
##LIBMAILIFY= -lzmailer
##MAILIFYCFLAGS= $(CFLAGS)
##LIBMAILIFY=
#NETW   = -L/usr/local/lib -lresolv # -lulsock
#LIBS=$(NETW)
#RANLIB = ranlib
#INSTALL=install

# # DEC AxpOSF/1 3.2 -- GCC-2.6.3
# #CC=gcc -Wall -O6 #-fno-builtin
# CC=cc -migrate -D__alpha__ -O5 -inline speed
# #CC=cc -D__alpha__
# CPP=gcc -E
# CDEFS=  -DBSD_SIGCHLDS -DHAS_LSTAT -DHAS_PUTENV #-DDEBUG
# CFLAGS= -g3 $(CDEFS)
# # Have MAILIFY compiled by uncommenting following ones:
# MAILIFY=mailify
# MAILIFYCFLAGS= $(CFLAGS) -DUSE_ZMAILER -I/l/include
# LIBMAILIFY= -lzmailer
# ##MAILIFYCFLAGS= $(CFLAGS)
# ##LIBMAILIFY=
# NETW=
# LIBS=$(NETW)
# RANLIB=ranlib
# INSTALL=installbsd

# SunOS 5.3 (Solaris 2.3) -- GNU-CC 2.4.6 on SPARC
#   Your PATH  MUST contain  /usr/ccs/bin:/opt/gnu/bin:/usr/ucb
#   for compilation to succeed without pains..
#CC=gcc -Wall -D__STDC__=0
#CPP=gcc -E
#CDEFS=  -O -I. -DUSG -DUSE_POLL -DHAS_LSTAT -DHAS_PUTENV #-DDEBUG
#CFLAGS= -g $(CDEFS)
## Have MAILIFY compiled by uncommenting following ones:
##MAILIFY=mailify
##MAILIFYCFLAGS= $(CFLAGS) -DUSE_ZMAILER -I/usr/local/include
##LIBMAILIFY= -lzmailer
##MAILIFYCFLAGS= $(CFLAGS)
##LIBMAILIFY=
#NETW=
#LIBS= -lsocket -lnsl $(NETW)
#RANLIB= :
#INSTALL=/usr/ucb/install


# SunOS --  GNU-CC 2.4.5 on SPARC SunOS 4.1.3
#CC=gcc -Wall #-fno-builtin
#CPP=gcc -E
#CDEFS=  -O -DBSD_SIGCHLDS -DHAS_LSTAT -DHAS_PUTENV
#CFLAGS= -g $(CDEFS)
# Have MAILIFY compiled by uncommenting following ones:
#MAILIFY=mailify
#MAILIFYCFLAGS= $(CFLAGS) -DUSE_ZMAILER -I/usr/local/include
#LIBMAILIFY= -lzmailer
###MAILIFYCFLAGS= $(CFLAGS)
###LIBMAILIFY=
#NETW=
#LIBS=$(NETW)
#RANLIB=ranlib
#INSTALL=install

# SunOS -- SunOS 4.1.3 bundled cc
#CC=cc
#CPP=/lib/cpp
#CDEFS=  -O -DBSD_SIGCHLDS -DHAS_LSTAT -DHAS_PUTENV
#CFLAGS=  $(CDEFS)
# Have MAILIFY compiled by uncommenting following ones:
MAILIFY=mailify
##MAILIFYCFLAGS= $(CFLAGS) -DUSE_ZMAILER -I/usr/local/include
##LIBMAILIFY= -lzmailer
##MAILIFYCFLAGS= $(CFLAGS)
##LIBMAILIFY=
#NETW=
#LIBS=$(NETW)
#RANLIB=ranlib
#INSTALL=install

# Linux 1.2.x  (w/o using -D_POSIX_SOURCE)
#CDEFS= -O6 -DHAS_LSTAT -DHAS_PUTENV
#CC=gcc
#CPP=gcc -E
#CFLAGS= -g $(CDEFS)
#NETW=
# Have MAILIFY compiled by uncommenting following ones:
#MAILIFY=mailify
##MAILIFYCFLAGS= $(CFLAGS) -DUSE_ZMAILER -I/usr/local/include
##LIBMAILIFY= -lzmailer
##MAILIFYCFLAGS= $(CFLAGS)
##LIBMAILIFY=
#LIBS=$(NETW)
#RANLIB=ranlib
#INSTALL=install

# Linux drb version
CDEFS=-DUSG -DNBCONNECT -DNBSTREAM -DUSE_XMIT_QUEUE \
	-DUSE_SOCKOPT -DSOCKBUFSIZE=8192 -DUSE_ENUM_TYPES -DDEBUG \
	-DCONFIG_FILE='"/etc/funetnje/funetnje.cf"' \
	-DPID_FILE='"/run/funetnje.pid"'
CC=gcc
CPP=gcc -E
CFLAGS= -g -O2 -Wpacked -Wpadded $(CDEFS)
#NETW=
# Have MAILIFY compiled by uncommenting following ones:
#MAILIFY=mailify
##MAILIFYCFLAGS= $(CFLAGS) -DUSE_ZMAILER -I/usr/local/include
##LIBMAILIFY= -lzmailer
##MAILIFYCFLAGS= $(CFLAGS)
##LIBMAILIFY=
#LIBS=$(NETW)
RANLIB=ranlib
INSTALL=install

# IBM AIX ?





# Name of the group on which all communication using programs are
# SGIDed to..  Also the system manager must have that bit available
# to successfully use  UCP  program.
NJEGRP=funetnje
# On some machines there may exist `send' already, choose another name.
SEND=tell
PRINT=njeprint
# Assign directories
MANDIR= /usr/local/man
LIBDIR= /usr/local/funetnje
BINDIR= /usr/local/bin
ETCDIR= /usr/local/etc

# If you have a malloc library with GOOD debugging facilities..
#DEBUG_LIBMALLOC=-L.. -lmalloc_dgcc


SRC=	bcb_crc.c  bmail.c  file_queue.c  headers.c  io.c  main.c	\
	nmr.c  protocol.c  read_config.c  recv_file.c  send.c		\
	send_file.c  unix.c  unix_brdcst.c  unix_build.c gone_server.c	\
	ucp.c  unix_mail.c  unix_route.c  unix_tcp.c  util.c detach.c	\
	unix_files.c sendfile.c bitsend.c read_config.c qrdr.c bitcat.c	\
	ndparse.c libndparse.c libreceive.c receive.c mailify.c		\
	mailify.sh clientutils.h sysin.sh version.sh unix_msgs.c	\
	bintree.c __fopen.c
HDR=	consts.h  ebcdic.h  headers.h  site_consts.h unix_msgs.h
OBJ=	file_queue.o  headers.o  io.o  main.o				\
	nmr.o nmr_unix.o protocol.o  read_config.o  recv_file.o  send.o	\
	send_file.o  unix.o  unix_brdcst.o  unix_build.o gone_server.o	\
	ucp.o  unix_mail.o  unix_route.o  unix_tcp.o  util.o		\
	bcb_crc.o  bmail.o detach.o unix_files.o sendfile.o bitsend.o	\
	qrdr.o logger.o uread.o bitcat.o unix_msgs.o __fopen.o
OBJmain=	main.o  headers.o  unix.o  file_queue.o	read_config.o	\
		io.o  nmr.o  unix_tcp.o  bcb_crc.o  unix_route.o	\
		util.o  protocol.o  send_file.o  recv_file.o logger.o	\
		unix_brdcst.o  unix_files.o gone_server.o detach.o	\
		libustr.o liblstr.o unix_msgs.o rscsacct.o version.o	\
		nmr_unix.o bintree.o __fopen.o
CLIENTLIBobj=		\
		clientlib.a(libndparse.o)	clientlib.a(libdondata.o)  \
		clientlib.a(libetbl.o)		clientlib.a(libsendcmd.o)  \
		clientlib.a(libreadcfg.o)	clientlib.a(libexpnhome.o) \
		clientlib.a(liburead.o)		clientlib.a(libuwrite.o)   \
		clientlib.a(libsubmit.o)	clientlib.a(libasc2ebc.o)  \
		clientlib.a(libebc2asc.o)	clientlib.a(libpadbla.o)   \
		clientlib.a(libhdrtbx.o)	clientlib.a(libndfuncs.o)  \
		clientlib.a(libustr.o)		clientlib.a(liblstr.o)	   \
		clientlib.a(logger.o)		clientlib.a(libstrsave.o)  \
		clientlib.a(__fopen.o)		clientlib.a(libmcuserid.o)
OBJbmail=	bmail.o		clientlib.a
OBJsend=	send.o		clientlib.a
OBJsendfile=	sendfile.o	clientlib.a
OBJbitsend=	bitsend.o	clientlib.a
OBJtransfer=	transfer.o	clientlib.a
OBJqrdr=	qrdr.o		clientlib.a
OBJucp=		ucp.o		clientlib.a
OBJygone=	ygone.o		clientlib.a
OBJacctcat=	acctcat.o	clientlib.a
OBJreceive=	receive.o libreceive.o clientlib.a
OBJmailify=	mailify.o libreceive.o clientlib.a
OBJnjeroutes=	njeroutes.o	bintree.o __fopen.o
OBJnamesfilter=	namesfilter.o namesparser.o
# Phase these out, once the `receive' works
#OBJbitcat=	bitcat.o	clientlib.a
#OBJnetdata=	ndparse.o	clientlib.a
PROGRAMS=	funetnje receive bmail ${SEND} sendfile njeroutes bitsend \
		qrdr ygone transfer acctcat ucp $(MAILIFY) namesfilter
ObsoletePROGRAMS= bitcat ndparse
OTHERFILES= finfiles.cf finfiles.header unix.install file-exit.cf msg-exit.cf
SOURCES= $(SRC) $(HDR) Makefile $(OTHERFILES) $(MAN1) $(MAN8)
MAN1=	man/send.1 man/sendfile.1 man/transfer.1 man/ygone.1		\
	man/submit.1 man/print.1 man/punch.1 man/receive.1
# ObsoleteMAN1= man/bitcat.1 man/ndparse.1
MAN5=	man/bitspool.5 man/ebcdictbl.5
MAN8=	man/funetnje.8 man/qrdr.8 man/njeroutes.8 man/bmail.8 man/ucp.8	\
	man/mailify.8 man/sysin.8
MANSRCS= man/bitspool.5 man/bmail.8 man/ebcdictbl.5			\
	man/funetnje.8 man/mailify.8 man/njeroutes.8 man/qrdr.8		\
	man/receive.1 man/send.1 man/sendfile.1 man/ucp.8		\
# Obsoletes: man/bitcat.1 
#				# For  man-ps  target

all:	$(PROGRAMS)

# Following need GNU TeXinfo package (3.1 or later)
info:	funetnje.info
dvi:	funetnje.dvi
html:	funetnje.html

.PRECIOUS:	clientlib.a

#   A couple default conversions..
.c.o:
	$(CC) -c $(CFLAGS) $<

.c.a:
	$(CC) -c $(CFLAGS) $<
	ar rc clientlib.a $%
	$(RANLIB) clientlib.a
	#rm -f $%

.c.s:
	$(CC) -S $(CFLAGS) $<

dist:
	# This is at  FTP.FUNET.FI/FINFILES.BITNET  where FUNET edition
	# is developed..  Making a dump to the archive in easy way..
	./version.sh
	rm -f *~ man/*~ smail-configs/*~ sendmail-configs/*~ zmailer-configs/*~
	rm -f njesrc/man/* njesrc/smail-configs/* njesrc/zmailer-configs/* njesrc/sendmail-configs/*
	cd njesrc; for x in `find . -links 1 -print`; do rm $$x; ln ../$$x . ; done
	cd njesrc; for x in submit ${PRINT} punch sf bitprt; do rm -f $$x; ln -s sendfile $$x;done
	# If you want to call 'send' with name 'tell'
	# cd njesrc; rm -f tell; ln -s ${SEND} tell
	ln man/* njesrc/man
	ln sendmail-configs/* njesrc/sendmail-configs
	ln zmailer-configs/* njesrc/zmailer-configs
	ln smail-configs/* njesrc/smail-configs
	date=`date +%y%m%d`; mv njesrc njesrc-$$date; gtar czf /pub/unix/networking/bitnet/funetnje-$$date.tar.gz njesrc-$$date; mv njesrc-$$date njesrc; chmod 644 /pub/unix/networking/bitnet/funetnje-$$date.tar.gz
	# Make sure the archive directory cache notices some changes..
	cd /pub/unix/networking/bitnet; ls-regen

purge:	clean purgecode

purgecode:
	rm -f $(PROGRAMS)

clean:
	rm -f \#*\# core *.o *~ *.ln *.a

route:	nje.route

install-man:
	-cd $(MANDIR)/cat1 && (for x in $(MAN1); do rm -f `basename $$x`;done)
	-cd $(MANDIR)/cat5 && (for x in $(MAN5); do rm -f `basename $$x`;done)
	-cd $(MANDIR)/cat8 && (for x in $(MAN8); do rm -f `basename $$x`;done)
	for x in $(MAN1); do $(INSTALL) -c -m 644 $$x $(MANDIR)/man1;done
	for x in $(MAN5); do $(INSTALL) -c -m 644 $$x $(MANDIR)/man5;done
	for x in $(MAN8); do $(INSTALL) -c -m 644 $$x $(MANDIR)/man8;done

man-ps:
	for X in $(MANSRCS); do groff -man $$X >$$X.ps; done

nje.route:	finfiles.header finfiles.netinit
	@echo "THIS IS FOR NIC.FUNET.FI!"
	-rm nje.route*
	$(LIBDIR)/njeroutes  finfiles.header finfiles.netinit nje.route

route2:	nje.route2

nje.route2:	fintest1.header fintest1.netinit
	@echo "THIS IS FOR MEA.UTU.FI!"
	-rm nje.route*
	njeroutes  fintest1.header fintest1.netinit nje.route

route3: nje.route3

nje.route3:	finutu.header finutu.netinit
	@echo "THIS IS FOR HAMSTERIX.FUNET.FI!"
	-rm nje.route*
	njeroutes  hamsterx.header hamsterx.netinit nje.route

maketar:
	tar -cf huji.tar $(SOURCES)

makeuuetar: maketar
	-rm -f huji.tar.Z huji.tar.Z.uue
	compress huji.tar
	uuencode huji.tar.Z huji.tar.Z >huji.tar.Z.uue
	rm huji.tar.Z

install:
	echo "To install actual control/config files do 'make install1' or 'make install2'"
	@echo "Must propably be root for this also."
	-mkdir ${LIBDIR}
	$(INSTALL) -s -m 755 funetnje ${LIBDIR}/funetnje.x
	mv ${LIBDIR}/funetnje.x ${LIBDIR}/funetnje
	#$(INSTALL) -s -m 755 ndparse ${BINDIR}  # Obsolete
	$(INSTALL) -s -m 755 bitsend ${BINDIR}
	$(INSTALL) -s -m 755 qrdr ${BINDIR}
	#$(INSTALL) -s -m 755 bitcat ${BINDIR}   # Obsolete
	$(INSTALL) -s -g ${NJEGRP} -m 750 ucp ${ETCDIR}
	$(INSTALL) -s -g ${NJEGRP} -m 755 sendfile ${BINDIR}
	rm -f ${BINDIR}/${PRINT} ${BINDIR}/submit ${BINDIR}/punch
	rm -f ${BINDIR}/sf ${BINDIR}/bitprt
	ln ${BINDIR}/sendfile ${BINDIR}/sf
	ln ${BINDIR}/sendfile ${BINDIR}/${PRINT}
	ln ${BINDIR}/sendfile ${BINDIR}/bitprt
	ln ${BINDIR}/sendfile ${BINDIR}/punch
	ln ${BINDIR}/sendfile ${BINDIR}/submit
	$(INSTALL) -s -g ${NJEGRP} -m 755 tell ${BINDIR}/${SEND}
	# If you want to call 'send' with name 'tell'
	# rm -f ${BINDIR}/tell
	# ln ${BINDIR}/${SEND} ${BINDIR}/tell
	$(INSTALL) -s -g ${NJEGRP} -m 755 ygone ${BINDIR}
	$(INSTALL) -s -g ${NJEGRP} -m 755 receive ${BINDIR}
	$(INSTALL) -s -g ${NJEGRP} -m 750 bmail    ${LIBDIR}
	mkdir -p /var/spool/bitnet
	chgrp ${NJEGRP} /var/spool/bitnet
	chmod g+w  /var/spool/bitnet
	chmod g+s ${BINDIR}/sendfile ${BINDIR}/tell ${BINDIR}/ygone \
		 ${LIBDIR}/bmail
	$(INSTALL) -s -m 755 transfer ${LIBDIR}/transfer
	$(INSTALL) -s -m 755 njeroutes ${LIBDIR}/njeroutes
	$(INSTALL) -s -m 755 namesfilter ${LIBDIR}/namesfilter
	$(INSTALL) -s -g ${NJEGRP} -m 750 mailify ${LIBDIR}/mailify
	$(INSTALL) -c -g ${NJEGRP} -m 750 sysin.sh ${LIBDIR}/sysin

install1:	route
	@echo "MUST BE ROOT TO DO THIS!"
	@echo "(this is for NIC.FUNET.FI)"
	-mkdir ${LIBDIR}
	cp finfiles.cf /etc/funetnje.cf
	cp nje.route* ${LIBDIR}
	cp file-exit.cf ${LIBDIR}/file-exit.cf
	cp msg-exit.cf ${LIBDIR}/msg-exit.cf

acctcat:	$(OBJacctcat)
	$(CC) $(CFLAGS) -o $@ $(OBJacctcat) $(LIBS)

bmail:	$(OBJbmail)
	$(CC) $(CFLAGS) -o $@ $(OBJbmail) $(LIBS)

funetnje:	$(OBJmain)
	$(CC) $(CFLAGS) -o $@.x $(OBJmain) $(LIBS) $(DEBUG_LIBMALLOC)
	-mv $@ $2.old
	mv $@.x $@

clientlib.a: $(CLIENTLIBobj)

ygone:	$(OBJygone)
	$(CC) $(CFLAGS) -o $@ $(OBJygone) $(LIBS)

${SEND}:	$(OBJsend)
	$(CC) $(CFLAGS) -o $@ $(OBJsend) $(LIBS)

sendfile:	$(OBJsendfile)
	$(CC) $(CFLAGS) -o $@ $(OBJsendfile) $(LIBS)

bitsend:	$(OBJbitsend)
	$(CC) $(CFLAGS) -o $@ $(OBJbitsend) $(LIBS)

njeroutes:	$(OBJnjeroutes)
	$(CC) $(CFLAGS) -o $@ $(OBJnjeroutes)

ucp:	$(OBJucp)
	$(CC) $(CFLAGS) -o $@ $(OBJucp) $(LIBS)

mailify:	$(OBJmailify)
	$(CC) $(MAILIFYCFLAGS) -o $@ $(OBJmailify) $(LIBS) $(LIBMAILIFY)

transfer:	$(OBJtransfer)
	$(CC) $(CFLAGS) -o $@ $(OBJtransfer) $(LIBS)

qrdr:		$(OBJqrdr)
	$(CC) $(CFLAGS) -o $@ $(OBJqrdr) $(LIBS)

receive:	$(OBJreceive)
	$(CC) $(CFLAGS) -o $@ $(OBJreceive) $(LIBS)

namesfilter:	$(OBJnamesfilter)
	$(CC) $(CFLAGS) -o $@ $(OBJnamesfilter) $(LIBS)

#  OBSOLETES:
#bitcat:		$(OBJbitcat)
#	$(CC) $(CFLAGS) -o $@ $(OBJbitcat) $(LIBS)
#
#ndparse:	$(OBJnetdata)
#	$(CC) $(CFLAGS) -o $@ $(OBJnetdata) $(LIBS)
#

bintest:	bintest.o bintree.o
	$(CC) $(CFLAGS) -o $@ bintree.o bintest.o

version.c:
	@echo "** BUG!  version.c  is created at 'make dist',"
	@echo "**       and should be present all the time!"
	exit 1

version.o:	version.c
bcb_crc.o:	bcb_crc.c consts.h site_consts.h ebcdic.h
bitsend.o:	bitsend.c consts.h site_consts.h headers.h ebcdic.h
bmail.o:	bmail.c site_consts.h clientutils.h ndlib.h
detach.o:	detach.c
file_queue.o:	file_queue.c consts.h site_consts.h
gone_server.o:	gone_server.c consts.h site_consts.h
headers.o:	headers.c headers.h consts.h site_consts.h ebcdic.h
bintree.o:	bintree.c
bintest.o:	bintest.c bintree.h
io.o:		io.c headers.h consts.h site_consts.h ebcdic.h prototypes.h
clientlib.a(libasc2ebc.o):	libasc2ebc.c	clientutils.h ebcdic.h
clientlib.a(libdondata.o):	libdondata.c	clientutils.h ebcdic.h prototypes.h ndlib.h
clientlib.a(libebc2asc.o):	libebc2asc.c	clientutils.h ebcdic.h
clientlib.a(libetbl.o):		libetbl.c	ebcdic.h
clientlib.a(libexpnhome.o):	libexpnhome.c	clientutils.h
clientlib.a(libhdrtbx.o):	libhdrtbx.c	clientutils.h prototypes.h
clientlib.a(libndfuncs.o):	libndfuncs.c	clientutils.h prototypes.h ebcdic.h ndlib.h
clientlib.a(libndparse.o):	libndparse.c	clientutils.h prototypes.h ebcdic.h ndlib.h
clientlib.a(libpadbla.o):	libpadbla.c	clientutils.h ebcdic.h
clientlib.a(libreadcfg.o):	libreadcfg.c	clientutils.h prototypes.h consts.h
clientlib.a(libsendcmd.o):	libsendcmd.c	clientutils.h prototypes.h consts.h
clientlib.a(libsubmit.o):	libsubmit.c	clientutils.h prototypes.h
clientlib.a(liburead.o):	liburead.c	clientutils.h prototypes.h
clientlib.a(libuwrite.o):	libuwrite.c	clientutils.h prototypes.h
clientlib.a(libstrsave.o):	libstrsave.c	clientutils.h prototypes.h
clientlib.a(libmcuserid.o):	libmcuserid.c	clientutils.h prototypes.h

clientlib.a(liblstr.o):	liblstr.c	clientutils.h
	$(CC) -c $(CFLAGS) $<
	ar rc clientlib.a $%
	$(RANLIB) clientlib.a
clientlib.a(libustr.o):	libustr.c	clientutils.h
	$(CC) -c $(CFLAGS) $<
	ar rc clientlib.a $%
	$(RANLIB) clientlib.a

mailify.o:	mailify.c consts.h clientutils.h prototypes.h ndlib.h
	$(CC) -c $(MAILIFYCFLAGS) mailify.c

namesfilter.o:	namesfilter.c
namesparser.o:	namesparser.c
locks.o:	locks.c
logger.o:	logger.c prototypes.h consts.h ebcdic.h
main.o:		main.c prototypes.h headers.h consts.h site_consts.h
ndparse.o:	ndparse.c consts.h prototypes.h clientutils.h ndlib.h
njeroutes.o:	njeroutes.c consts.h prototypes.h site_consts.h bintree.h
nmr.o:		nmr.c headers.h consts.h site_consts.h prototypes.h
nmr_unix.o:	nmr_unix.c headers.h consts.h site_consts.h prototypes.h
protocol.o:	protocol.c headers.h consts.h site_consts.h prototypes.h
qrdr.o:		qrdr.c consts.h headers.h
read_config.o:	read_config.c consts.h site_consts.h
receive.o:	receive.c clientutils.h prototypes.h ndlib.h
libreceive.o:	libreceive.c clientutils.h prototypes.h ndlib.h
recv_file.o:	recv_file.c headers.h consts.h site_consts.h
send.o:		send.c clientutils.h prototypes.h
send_file.o:	send_file.c headers.h consts.h site_consts.h
sendfile.o:	sendfile.c clientutils.h prototypes.h ndlib.h
transfer.o:	transfer.c clientutils.h prototypes.h
ucp.o:		ucp.c clientutils.h prototypes.h
unix.o:		unix.c headers.h consts.h site_consts.h prototypes.h
unix_brdcst.o:	unix_brdcst.c consts.h site_consts.h prototypes.h
unix_files.o:	unix_files.c consts.h prototypes.h
unix_build.o:	unix_build.c consts.h site_consts.h prototypes.h
unix_msgs.o:	unix_msgs.c unix_msgs.h consts.h site_consts.h prototypes.h
unix_route.o:	unix_route.c consts.h site_consts.h prototypes.h bintree.h
unix_tcp.o:	unix_tcp.c headers.h consts.h site_consts.h prototypes.h
util.o:		util.c consts.h site_consts.h ebcdic.h prototypes.h bintree.h

funetnje.info:	funetnje.texinfo
	makeinfo funetnje.texinfo

funetnje.dvi:	funetnje.texinfo
	tex funetnje.texinfo

funetnje.html:	funetnje.texinfo
	texi2html funetnje.texinfo

lint:	lintlib
	lint -hc $(CDEFS) llib-lhuji.ln $(SRC)

lintlib:	llib-lhuji.ln

llib-lhuji.ln:	$(SRC)
	lint -Chuji $(CDEFS) $(SRC)

locktest: locktest.o locks.o
	$(CC) -o locktest locktest.o locks.o
