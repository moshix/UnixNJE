#!/bin/sh
#
# A first-order approximation of remote printing in FUNET's NJE
#
# Add following into  file-exit.cf (prefix it with PRT file matching)
#	RUN /path/rprint $SPOOL lpname $FRUSER $FRNODE $TOUSER $FID
#
# Written by  Wilfried Maschtera  <maschti@alijku64>
# and somewhat edited (commented) by Matti Aarnio <mea@finfiles>
#
HUJIBIN=/usr/local/bin
LPRBIN=/usr/ucb/lpr
pgname=`basename $0`
fid=$6
$HUJIBIN/receive  -o /tmp/$pgname.$$ $1 2> /dev/null
$LPRBIN -P$2 -J $3 /tmp/$pgname.$$ 2> /dev/null
rc=$?
if [ $rc -eq 0 ]
   then
   $HUJIBIN/send $3@$4 "Sent file ($fid) on link $5 to $2(SYSTEM)"
fi
rm /tmp/$pgname.$$ 2> /dev/null
