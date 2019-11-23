#!/bin/sh
# transfer.sh

# Testing call arguments, not for general production use..

( echo "Argc= $#"
  echo "Argv= $0 $@"
  DESTNAME=`basename $1`
  /bin/mv $1 /usr/spool/bitnet/$DESTNAME
  /usr/local/lib/huji/transfer.exe /usr/spool/bitnet/$DESTNAME $2 ) >> /root/transfer.log
