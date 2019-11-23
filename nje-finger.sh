#!/bin/sh
#
# Sample  nje-finger  application for doing command 'FINGER' at FUNET-NJE
#

echo "NJEFINGER: '$*'" >> /tmp/nje.finger.log
touser="$1"
tonode="$2"
shift; shift
argstr="$*"
echo "NJEFINGER: argstr='$argstr'" >> /tmp/nje.finger.log

/usr/ucb/finger "$argstr" |
	/usr/local/bin/send -u finger "$touser@$tonode"
