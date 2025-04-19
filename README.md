
NJE for UNIX
=============



This NJE repo is is the third incarnation of the Hebrew University of Jersualem NJE protocol for UNIX and VMS. The original VMS repo is here: https://github.com/moshix/HUJInje

In this new version, it's been made to compile cleanly on Linux, BSD (including latest Macos!), as well as with the latest gcc and glibc systems present in Ubuntu 22.04 [thanks wschaub!]

You will need to ask to peer with somebody on HNET to use this protocol. Contact me if you have a 24/7 Linux server and you want to peer with my HNET main node RELAY (which is also based on UNIXNJE). See more: www.moshix.tech 

UnixNJE is fully compatible with the NJE protocol and connects effortlessly to VM/SP or z/VM RSCS, NJE38 for MVS 3.8, SineNomine NJE, JNET for openVMS, NJE for Windows. 



INSTALL
------

get repo to your Linux/BSD/Macos (anything from the last 6-7 years should work)

then type "make" and watch it compile

type sudo make install

Configuration
-------------

To install an example configuration type sudo make install1

In /etc/funetnje create a file called funetnje.cf like this (just an example):
<pre>
#       Configuration file for FUNET-NJE program
#
NAME            SELXM1
IPADDRESS       111.111.111.195  #my IP address
QUEUE           /var/spool/bitnet # where my work spool is
CMDMAILBOX      U 127.0.0.1 175   #my port 175
LOG             /var/log/bitnet.log #log location
TABLE           /usr/local/funetnje/funetnje.route  #location of routes information (see below)
INFORM          MAINT@NODE1    # who will be informed of issues
USEREXITS       /usr/local/funetnje/file-exit.cf  #see my video (link below)
MSGEXITS        /usr/local/funetnje/msg-exit.cf   # see y video (link below)
LLEVEL          5                                      # log level
#LLEVELCLIENT    1
DEFAULT-ROUTE   NODE1 # defaultr route for all nodes

LINE 0          NODE1
 TYPE           UNIX_TCP
 BUFSIZE        8192
 TIMEOUT        10
 IPPORT         1175
 TCPNAME        222.222.222.30
 TCP-SIZE       8192
 MAX-STREAMS     7
</pre>

in file /usr/local/funetnje/funetnje.route.header:
<pre>
ROUTE SELXM1 LOCAL ASCII
</pre>
In file /usr/local/funetnje/funetnje.route.routes:
<pre>
ROUTE MOSHIX2       NODE1 ETHNET   CH
ROUTE MOSHIX4       NODE1 ETHNET   CH
ROUTE MOSHIX3       NODE1 ETHNET   CH
ROUTE MOSHIX5       NODE1 ETHNET   CH
ROUTE MOSHIX        NODE1 ETHNET   CH
</pre>

inside /usr/local/funetnje run:
<pre>
./njeroutes funetnje.route.header funetnje.route.routes funetnje.route
</pre>
Running It
----------

To execute it you need sudo privileges, or need to be root. 

Run the njeroutes command described in the previous section to create /usr/local/funetnje/funetnje.route, then start funetnje as a daemon

Enable "mesg yes" for your shell session to receive messages from other nodes. 

Commands available send to sendfiles or messages or commands. "qrdr" to see the file queue. See my video (link below)

Watch this video to see how to get this to works: https://youtu.be/1iHrNNH7plY


Enjoy

moshix  
Moscow, January 2025  
