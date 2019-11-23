#!/bin/sh
#
#  NETINIT processor
#
#  This is run once a day, and when new data is available,
#  it is processed straight away.
#
#  Part of FUNETNJE software package
#

#N=-n		# Don't remove the file from the reader
N=		# Remove the file from the reader

NJEDIR=/l/funetnje	# Where FUNETNJE programs are
export N NJEDIR
nodename=finfiles	# lowercase
NODENAME=FINFILES	# uppercase

cd /l/mail/db	# ZMailer router database directory

PATH=/usr/local/bin:/usr/local/etc:$PATH	# Dirs where  qrdr, receive,
export PATH					#      and  ucp  reside..

qrdr -u NETINIT -l |		\
(   while read  fname datatype src dest fn ft junk1 class junk 
    do {
	if [ "$fn.$ft" = "${NODENAME}.NETINIT" ]; then
	    echo "${NODENAME}.NETINIT: " $fname $datatype \
		 $src $dest $fn.$ft $class
	    receive $N -a -o ${NJEDIR}/${nodename}.netinit $fname	&& \
	    {   # Received most successfully the NETINIT file
		echo "Ndparse ok, generating routes.."
		(
			cd ${NJEDIR}	# Change the dir to NJEDIR..
			./njeroutes ${nodename}.header ${nodename}.netinit \
				    ${nodename}.routes
		)
		# Routing regenerated, order route database reopen
		ucp rescan route
	    }
		
	elif [ "$fn.$ft" = "BITEARN.NODES" ]; then
	    echo "BITEARN.NODES: " $fname $datatype $src \
		 $dest $fn.$ft $class

	    receive $N -a -o bitearn.nodes $fname		&& \
	    {   # Received BITEARN.NODES successfully.
		${NJEDIR}/namesfilter -zmailer < bitearn.nodes | \
						 sort > routes.bitnet
		/etc/zmailer router
	    }
	else
	    echo "Unknown file for NETINIT: " $fname \
		 $datatype $src $dest $fn.$ft $class
	fi
    } done
)

exit 0
