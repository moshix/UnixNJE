#!/bin/sh
#
#  mailify.sh -- do it simple mailify utility by using intelligent
#		 subprograms - and letting mailer do most...
#  Copyright (c) Finnish University and Research Network, FUNET 1991, 1993,1994
#

nodel=0
TMPFILE=/usr/tmp/mlfy$$.tmp
TMPLOG=/dev/null
TMPLOG=/usr/tmp/mailify.runlog
#touch $TMPLOG

if [ "x$1" = "x-n" ]; then
	shift; nodel=1
fi

telusage=0
if [ "x$1" != "x" ]; then
	if [ ! -r $1 ]; then
		telusage=1
	fi
else
	telusage=1
fi
if [ $telusage = 1 ]; then
	echo "MAILIFY - program for HUJI-NJE incoming mail processing"
	echo "          Not for casual users invocation."
	echo "Args: [-n] spoolfilepath"
	echo "      -n  for no original file removal."
	exit 3
fi
(
	echo "Mailify: $*"
	/usr/local/bin/receive -a -n -o $TMPFILE $1
	rc=$?
	if [ $rc != 0 ]; then exit $rc; fi
	headword="`head -1 $TMPFILE|cut -c-5`"
	if [ "$headword" = "HELO " ]; then
		# Zmailer interface module, your mileage may vary..
		/usr/lib/sendmail -v -bs 2>&1 < $TMPFILE
	else
		/usr/lib/sendmail -bm 2>&1 < $TMPFILE
	fi
	rc=$?

	rm $TMPFILE

	if [ $nodel = 1 ]; then exit 0; fi
	if [ $rc != 0 ]; then exit $rc; fi

	/bin/rm -f $1
	exit 0
) >> $TMPLOG

exit $?
