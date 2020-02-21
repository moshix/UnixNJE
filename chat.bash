#!/bin/bash
mesg y
if [ -z $1 ]
 then

 echo "Chat commands available: /LOGON /LOGOFF /STATS /HELP /PSYCHO"


else
  echo "chat command processor v1.2 args: "$1
  echo "Please wait 3-4 seconds for output.."
  /usr/local/bin/send -m ROOT@RELAY $1 2>&1
fi
