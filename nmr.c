/* NMR.C    V2.7-mea
 | Copyright (c) 1988,1989,1990 by
 | The Hebrew University of Jerusalem, Computation Center.
 |
 |   This software is distributed under a license from the Hebrew University
 | of Jerusalem. It may be copied only under the terms listed in the license
 | agreement. This copyright message should never be changed or removed.
 |   This software is gievn without any warranty, and the Hebrew University
 | of Jerusalem assumes no responsibility for any damage that might be caused
 | by use or misuse of this software.
 |
 | Handle NMR records (commands and messages).
 | The function Handle_NMR is called when an NMR is received via an NJE line.
 | If it is intended to the local node, it is broadcasted. If not, it is
 | forwarded (in EBCDIC, as-is) to SEND_NMR to forward it to the correct line.
 | We do not <???>
 | Send-NMR checks first whether this is a local address. If so, it is
 | broadcasted localy. If not, it is queued on the correct line. If the message
 | is a forwarded one, we take it as-is and save us formatting work.
 |   Commands are assumed to be in upper case (either received from the other
 | side, ot translated so by the local user's interface).
 |
 | Document:
 |   Network Job Entry - Formats and Protocols (IBM)
 |	SC23-0070-02
 |	GG22-9373-02 (older version)
 */

#include "consts.h"
#include "headers.h"
#include "prototypes.h"

extern void  list_users __(( const char *Faddress, const char *Taddress, const char cmd, const char *Username ));
extern void  send_welcome_message __(( const char *Faddress, const char *Taddress ));
extern void  send_cpu_time __(( const char *Faddress, const char *Taddress ));
static void  query_route __(( const char *Faddress, const char *Taddress, char *NodeName ));


/* The help text. Must end with a null string: */
char	HelpText[][80] = {
	"Commands available for FUNET-NJE emulator:",
	"   Query SYStem       - Show lines status summary report, and activity",
        "   Query STATs        - Show lines statictics",
	"   Query Nodename     - Show the route entry to that node",
	"   Query linkname A/F - Available via Query SYStem",
	"   Query linkname Q   - Show 30 first files in queue on the link",
	"   CPQuery Names      - List all users logged on",
	"   CPQuery User       - Tell how many users are logged on",
	"   CPQuery User username - Look for a specific username",
	"   CPQuery LOG        - Send the WELCOME message",
	"   CPQuery CPU, CPLEVEL, IND, T - Machine type and time",
	"" };


/*
 | The main entry point. All NMR's are received here and dispatched from
 | Here.
 */
void
handle_NMR(NmrRecord, size)
struct	NMR_MESSAGE	*NmrRecord;
const int	size;
{
	char	ToName[20], ToNode[20], MessageText[132+8+2],
		OriginNode[20], OriginName[20], line[LINESIZE];
	unsigned char	*up;	/* For EBCDIC fields handling */
	register int	i, s;

	/* Retrieve the origin site name, the destination
	   sitename and username */
	EBCDIC_TO_ASCII(NmrRecord->NMRFMNOD, OriginNode, 8);
	EBCDIC_TO_ASCII(NmrRecord->NMRTONOD, ToNode, 8);
	EBCDIC_TO_ASCII(NmrRecord->NMRUSER, ToName, 8);
	despace(OriginNode,8);	/* Remove trailing blanks */
	despace(ToNode,8);
	despace(ToName,8);

	/* Test whether it is for our node */
#ifdef DEBUG
	logger(3, "NMR: Message from node '%s' to %s@%s\n",
		OriginNode, ToName, ToNode);
	trace((void*)NmrRecord,size,5);
#endif

	*OriginName = '\0';	/* Assume no username */
	if ((NmrRecord->NMRFLAG & 0x80) != 0) {
	  strcpy(OriginName,ToName);
	  *ToName = 0;
	}

	up = &NmrRecord->NMRMSG[0];
	s = (NmrRecord->NMRML & 0xff);
	if ((NmrRecord->NMRTYPE & 0x08) != 0) {
	  /* There is username (sender) inside the message's text */

	  if ((NmrRecord->NMRTYPE & 0x04) != 0) {	/* No time stamp */
	    /* First 8 characters of message are the sender's username */
	    EBCDIC_TO_ASCII(NmrRecord->NMRMSG, OriginName, 8);
	    despace(OriginName,8);
	    up += 8; s -= 8;	/* Username removed from message */
	  } else {
	    /* There is timestamp, username, and then the message's text */
	    EBCDIC_TO_ASCII(&up[8], OriginName, 8);
	    despace(OriginName,8);
	    /* Copy the message's text over the username */
	    memcpy(&up[8], &up[16], s - 16);
	    s -= 8;		/* Username removed */
	    /* logger(2,"NMR msg with a timestamp! From %s(%s)\n",
	       OriginNode,OriginName); */
	  }
	}

	if (!(strcasecmp(ToNode, LOCAL_NAME) == 0 ||
	      (*LOCAL_ALIAS != 0 && strcasecmp(ToNode,LOCAL_ALIAS) == 0))) {

	  /* It is not a local name, nor local alias,
	     we try forwarding it			*/

	  /* If we can't forward a command we send a rejection message,
	     so create the sender's address.			*/

	  sprintf(line, "%s@%s", OriginName, OriginNode);

	  if ((NmrRecord->NMRFLAG & 0x80) == 0) /* Message */
	    send_nmr(line, ToNode, NmrRecord, size,
		     EBCDIC, CMD_MSG);
	  else		/* Command */
	    send_nmr(line, ToNode, NmrRecord, size,
		     EBCDIC, CMD_CMD);
	  return;
	}

	/* Test whether this is a command or message. */

	if ((NmrRecord->NMRFLAG & 0x80) != 0) {
	  logger(3, "NMR: command from '%s@%s' to node %s\n",
		 OriginName, OriginNode, ToNode);
	  /* up = NmrRecord->NMRMSG; */
	  i = (NmrRecord->NMRML & 0xff);
	  EBCDIC_TO_ASCII(up, MessageText, i);
	  MessageText[i] = '\0';
	  logger(3, "Command is: '%s'\n", MessageText);
	  message_exit(ToName, ToNode, OriginName, OriginNode,
		       CMD_CMD, MessageText);
	  /*handle_local_command(OriginNode, ToName, MessageText);*/
	  return;		/* It's a message - ignore it */
	}

	/* Save the start of the original text in RealMessageText. See
	   explanations in return-message */
	EBCDIC_TO_ASCII(up, MessageText, s);

	/* Remove all codes whose ASCII is less than 32 */
	MessageText[s] = '\0';
	for (up = (unsigned char *)MessageText; *up != 0; up++)
	  if (*up < ' ')
	    *up = ' ';

#ifdef DEBUG
	logger(4, "Sending message to user '%s'\n", ToName);
#endif

	/* Send to user and check whether he received it */
	if (*ToName == '\0') {	/* Happens sometimes */
#ifdef DEBUG
	  logger(3, "NMR message to null user: '%s'\n",
		 MessageText);
#endif
	  return;
	}

	message_exit(ToName, ToNode, OriginName, OriginNode,
		     CMD_MSG, MessageText);
}


/*
 | Send the NMR. Parse the address (which arrives as User@Site), and queue it
 | to the correct line.
 */
void
send_nmr(Faddress, Taddress, Text, size, format, Cflag)
const	MSGCODES format;	/* ASCII or EBCDIC	*/
const	int	Cflag;		/* Command or Message	*/
	int	size;
const	char	*Faddress, *Taddress;
void		*Text;
{
	char	*text = (char *)Text;
	char	*p, line[LINESIZE], FromName[20], NodeKey[20];
	char FrName[20],*FrNode,ToName[20],*ToNode;
	unsigned char	*up;
	struct	NMR_MESSAGE	NmrRecord;
	struct	MESSAGE Msg;
	int	MessageSize = 0;
	int	AddUserName = 0;	/* Do we include Username when
					   sending a message? */

	if (size > LINESIZE) {	/* We don't have room for longer message */
	  logger(1, "NMR, Message text size(%d) too long for '%s'. Ignored\n",
		 size, Taddress);
	  return;
	}

	/* Copy the message text into the NMR buffer */
	if (format == EBCDIC) {
	  if ((p = strchr(Taddress, '@')) != NULL)
	    strcpy(NodeKey, ++p);
	  else
	    strcpy(NodeKey, Taddress);	/* In this case this is only
					   the node name */
	  goto Emessage;	/* Ebcdic message - send as-is */
	}

	/* Remove traling .EARN, .BITNET from the Taddress */
	if ((p = strchr(Taddress, '.')) != NULL) {
	  if ((strcasecmp(p, ".BITNET") == 0) ||
	     (strcasecmp(p, ".EARN") == 0))
	    *p = '\0';	/* Drop it. */
	}


	strcpy(FrName,Faddress);
	if ((FrNode = strchr(FrName,'@')) != NULL)
	  *FrNode++ = 0;
	else
	  FrNode = LOCAL_NAME;
	strcpy(ToName,Taddress);
	if ((ToNode = strchr(ToName,'@')) != NULL)
	  *ToNode++ = 0;
	else
	  ToNode = LOCAL_NAME;


	/* Test whether this message is for our node. If so, send it locally */
	if (*ToNode != 0 &&
	    (strcasecmp(ToNode, LOCAL_NAME) == 0 ||
	     (*LOCAL_ALIAS != 0 && strcasecmp(ToNode, LOCAL_ALIAS)==0))) {
	  /* If it is a command - execute it. If not - broadcast */
	  if (Cflag == CMD_CMD) {

	    message_exit(ToName, ToNode, FrName, FrNode, CMD_CMD, text);
	  } else {
	    /* Remove all codes which ASCII is less than 32 */
	    for (up = (unsigned char *)text; *up != 0; ++up)
	      if (*up < ' ') *up = ' ';
	    message_exit(ToName, ToNode, FrName, FrNode, CMD_MSG, text);
	  }
	  return;
	}

	/* The message is in ASCII = no forwarded message.
	   Check whether it is intended to some specific addresses
	   that we don't want to send messages to them,
	   since they usually can't receive messages.
	   */

	/* Create the From address */
	strcpy(FromName, Faddress);
	strcpy(line, Faddress);
	if ((p = strchr(line, '@')) == NULL) {
	  logger(1, "NMR, Illegal from address '%s'\n", Faddress);
	  return;
	}
	size = strlen(FrNode);
	ASCII_TO_EBCDIC(FrNode, NmrRecord.NMRFMNOD, size);
	PAD_BLANKS(NmrRecord.NMRFMNOD, size, 8);
	/* Put the sender username as the first 8 bytes of message text */
	if (Cflag == CMD_CMD) {	/* NMRUSER holds the originator if command */

	  size = strlen(FrName);
	  ASCII_TO_EBCDIC(FrName, NmrRecord.NMRUSER, size);
	  PAD_BLANKS(NmrRecord.NMRUSER, size, 8);

	} else {	/* Message - Write the username as
			   first message's field */

	  if (*FrName != '\0') {	/* There is username there */

	    size = strlen(FrName);
	    ASCII_TO_EBCDIC(FrName, NmrRecord.NMRMSG, size);
	    PAD_BLANKS(NmrRecord.NMRMSG, size, 8);
	    AddUserName = 1;
	  } else
	    AddUserName = 0;
	}
	if (Cflag == CMD_MSG) {
	  /* Message - If we put the username before, then we have to
	     skip now over it (it occipies the first 8 characters of the
	     message's text) */
	  if (AddUserName == 0) {	/* No username is there */
	    size = strlen(text);
	    ASCII_TO_EBCDIC(text, NmrRecord.NMRMSG, size);
	  } else {	/* The username is there */
	    up = &NmrRecord.NMRMSG[8];
	    size = strlen(text);
	    ASCII_TO_EBCDIC(text, up, size);
	    size += 8;	/* Add 8 for the username that was before */
	  }
	} else {	/* Command - do not start message with username */
	  up = NmrRecord.NMRMSG;
	  size = strlen(text);
	  ASCII_TO_EBCDIC(text, up, size);
	}

	NmrRecord.NMRML = (size & 0xff);
	MessageSize = size;	/* The text size */
	NmrRecord.NMRFLAG = 0x20;	/* We send the username */

	if (Cflag == CMD_CMD)
	  NmrRecord.NMRFLAG |= 0x80;	/* The command flag */

	NmrRecord.NMRLEVEL = 0x77;	/* RSCS does it... */

	if (Cflag == CMD_MSG) {
	  if (AddUserName != 0)
	    NmrRecord.NMRTYPE = 0xc; /* Username, no timestamp... */
	  else
	    NmrRecord.NMRTYPE = 0x4; /* no Username, no timestamp... */
	} else
	  NmrRecord.NMRTYPE = 0;
	/*memcpy(NmrRecord.NMRFMNOD, E_BITnet_name, 8);*/

	/* Create the destination address */

	strcpy(line, Taddress);
	if (*ToNode == 0) {
	  logger(1, "NMR, Illegal address '%s' to send message to.\n",
		 Taddress);

	  /* Send a reply back to the user only if the message
	     if of type ASCII (thus locally originated) and
	     the "from" is a real from */

	  if (format == ASCII) {
	    if (*FrNode == 0) return;
	    if ((strcasecmp(FrNode,LOCAL_NAME) != 0) /*Sender not on our node*/
		|| (*FrName == '\0'))		     /*Not a real username   */
	      return;
	    sprintf(line,
		    "\007NJE: Can't send message to %s",
		    Taddress);
	    message_exit(FrName, FrNode, "", LOCAL_NAME, CMD_MSG, line);
	  }
	  return;
	}

	strcpy(NodeKey, ToNode);
	size = strlen(ToNode);
	ASCII_TO_EBCDIC(ToNode, NmrRecord.NMRTONOD, size);
	PAD_BLANKS(NmrRecord.NMRTONOD, size, 8);
	if (Cflag == CMD_MSG) {	/* NMRUSER holds the sender if message */
	  size = strlen(ToName);
	  ASCII_TO_EBCDIC(ToName, NmrRecord.NMRUSER, size);
	  PAD_BLANKS(NmrRecord.NMRUSER, size, 8);
	}
	NmrRecord.NMRFMQUL = 0;
	NmrRecord.NMRTOQUL = 0;

	/* Create the RCB, etc.: */
    Emessage:
	if (format == ASCII) {	/* Create the whole NMR record */
	  size = 0;
	  line[size++] = 0x9a;	/* RCB */
	  line[size++] = 0x80;	/* SRCB */
	  /* There are implementations which do not like redundant bytes
	     after the end of the message's text. Hence, when we call
	     Compress_SCB, give the real size.
	     This is the text's size and 30 for all the controls before it.*/
	  size += compress_scb(&NmrRecord, &line[2], MessageSize + 30);

	} else {
	  /* It's EBCDIC - Have to add only the first RCB+SRCB. Last RCB
	     is not added since the sending routine might block a few
	     messages in one transmission block. Size contains now the
	     size of the received NMR */
	  line[0] = 0x9a;
	  line[1] = 0x80;
	  size = 2 + compress_scb(text, &line[2], size);
	}
	/* Check whether we do not have a buffer overflow */
	if (size >= LINESIZE)
	  bug_check("NMR: NMR record size overflow (after compress)");

	/* Convert the nodename to uppercase */
	upperstr(NodeKey);


	Msg.length = size;
	/* For debugging and safety checks */
	if (size > sizeof(Msg.text)) {
	  logger(1, "NMR, Too long Message. Malloc will overflow!\n");
	  size = sizeof(Msg.text);
	}
	memcpy(Msg.text, line, size);
	strncpy(Msg.Faddress, Faddress, sizeof(Msg.Faddress));
	Msg.Faddress[sizeof(Msg.Faddress)-1] = 0;
	strncpy(Msg.node,NodeKey,sizeof(Msg.node));
	Msg.node[sizeof(Msg.node)-1] = 0;
	Msg.type = Cflag;
	Msg.candiscard = 0;
	if (text[0] == '*' || Faddress[0] == '@')
	  Msg.candiscard = 1;

	nmr_queue_msg(&Msg);
}

void nmr_queue_msg(Msg)
struct MESSAGE *Msg;
{
	char	TempLine[LINESIZE], LineName[20], line[LINESIZE];
	char	CharacterSet[16];	/* Not needed but returned
					   by Find_line_index() */
	int	PrimLine,  AltLine, i;
	struct MESSAGE *Message;

	/* look for the line number we need - it returns the link to
	   send the NMR on (the direct link or alternate route): */

	switch ((i = find_line_index(Msg->node, LineName, CharacterSet,
				     &PrimLine, &AltLine))) {
	  case NO_SUCH_NODE:	/* Does not exist at all */
	      logger(2, "NMR, Node '%s' unknown\n", Msg->node);
	      if (!Msg->candiscard) {
		/* It isn't a comment, and sender is not some system:
		   Return a message to the sender */
		sprintf(line, "Node %s not recognised.", Msg->node);
		sprintf(TempLine, "@%s", LOCAL_NAME);	/* Sender */
		send_nmr(TempLine, Msg->Faddress, line, strlen(line),
			 ASCII, CMD_MSG);
	      }
	      break;
	  case ROUTE_VIA_LOCAL_NODE:	/* Not connected via NJE */
	      if (!Msg->candiscard) {
		/* It isn't a comment, and sender is not some system:
		   Return a message to the sender */
		sprintf(line, "Node %s can't receive commands/msgs",
			Msg->node);
		sprintf(TempLine, "@%s", LOCAL_NAME);	/* Sender */
		send_nmr(TempLine, Msg->Faddress, line, strlen(line),
			 ASCII, CMD_MSG);
	      }
	      break;
	  case LINK_INACTIVE:	/* Both link and alternate routes inactives */
	      if (!Msg->candiscard) {
		/* It isn't a comment, and sender is not some system:
		   Return a message to the sender */
		sprintf(line, "Link %s to node %s inactive", LineName,
			Msg->node);
		sprintf(TempLine, "@%s", LOCAL_NAME);	/* Sender */
		send_nmr(TempLine, Msg->Faddress, line, strlen(line),
			 ASCII, CMD_MSG);
	      }
	      break;
	  default:	/* All other values are index values
			   of line to queue on */
	      if ((i < 0) || (i >= MAX_LINES)) {
		logger(1, "NMR, Illegal line number #%d returned by find_file_index() for node %s\n", i, Msg->node);
		break;
	      }
	      /* OK - get memory and queue for line */
	      if (IoLines[i].MessageQstart == NULL) {
		/* Init the list */
		Message = (struct MESSAGE*)malloc(sizeof(struct MESSAGE));
		IoLines[i].MessageQstart = Message;
		IoLines[i].MessageQend   = Message;
	      } else {
		Message = (struct MESSAGE*)malloc(sizeof(struct MESSAGE));
		IoLines[i].MessageQend->next =  Message;
		IoLines[i].MessageQend   = Message;
	      }
/* logger(2,"NMR: *DEBUG* struct MESSAGE * -> 0x%x, size=%d, line %s(%d)\n",
   Message, size, IoLines[i].HostName, i); */

	      if (Message == NULL) {
#ifdef VMS
		logger(1, "NMR, Can't malloc. Errno=%d, VmsErrno=%d\n",
		       errno, vaxc$errno);
#else
		logger(1, "NMR, Can't malloc. Errno=%d\n",
		       errno);
#endif
		bug_check("NMR, Can't Malloc for message queue.");
	      }
	      memcpy(Message,Msg,sizeof(*Msg));
	      Message->next = NULL;
#ifdef DEBUG
	      logger(3, "NMR, queued message to address %s, size=%d.\n",
		     Taddress, size);
#endif
	      break;
	}
}


/*
 | Loop over links and find the requested line. If it is in ACTIVE/CONNECT
 | mode - good; return it. If not (no such line or link inactive), try looking
 | in the routing table. If the destination there is different, then call this
 | routine again in order to try finding an alternate link using the same
 | algorithm. This allows us to "chain" alternative routes.
 |   The character set for directly connected line is forced to be EBCDIC.
 */
int
find_line_index(NodeName, LineName, CharacterSet, PrimLinep, AltLinep)
	char	*NodeName;	/* The nodename we are looking for */
	char	*LineName;	/* The actual line to route to */
	char	*CharacterSet;	/* ASCII or EBCDIC. */
int	*PrimLinep, *AltLinep;	/* Store link indexes */
{
	int	i;
	char	NodeKey[16], LineName2[16], TempLine[LINESIZE];

	*PrimLinep = -1;
	*AltLinep  = -1;

	if (strcasecmp(NodeName,LOCAL_NAME) == 0 ||
	    (*LOCAL_ALIAS && strcasecmp(NodeName, LOCAL_ALIAS) == 0)) {
	  strcpy(LineName,"LOCAL");
	  return ROUTE_VIA_LOCAL_NODE;
	}

	/* Upcase nodename */
	strcpy(NodeKey, NodeName);
	upperstr(NodeKey);

	/* First - search for the line and see whether
	   it is in Connect state */

	for (i = 0; i < MAX_LINES; i++) {
	  if (IoLines[i].HostName[0] != 0 && /* This line is defined */
	      strcasecmp(IoLines[i].HostName, NodeKey) == 0) {
	    /* OK - found a directly connected line.   Report to
	       the caller that the linename to queue is the same
	       and if it is in connect mode, return the line index,
	       else just prepare for LINK_INACTIVE report */
	    *PrimLinep = i;
	    strcpy(LineName, NodeKey);
	    strcpy(CharacterSet, "EBCDIC");
	    break;
	  }
	}

	/* Just in case there is an alternate route,
	   look in the table for an entry for it. */

	i = get_route_record(NodeKey, TempLine, sizeof TempLine);

	if (i == 0) { /* No route record */
	  if (*PrimLinep < 0)
	    return NO_SUCH_NODE; /* Not defined in tables at all */
	  else
	    if (IoLines[*PrimLinep].state == ACTIVE)
	      return *PrimLinep; /* Ok, it is active, point to it.. */
	    else
	      return LINK_INACTIVE; /* It is inactive.. */
	}
	sscanf(TempLine, "%s %s", LineName2, CharacterSet);

	logger(3,"NMR: find_line_index() Node=`%s' -> `%s' (%s)\n",NodeKey,TempLine,LineName2);

	/* If this link is LOCAL, discard the message.
	   It is intended to sites that get mail services
	   from us but are not connected with NJE protocol.	*/

	if (strcasecmp(LineName2, "LOCAL") == 0)
	  return ROUTE_VIA_LOCAL_NODE;

	/* Find the target line */

	for (i = 0; i < MAX_LINES; i++) {
	  if (IoLines[i].HostName[0] != 0 && /* This line is defined */
	      strcasecmp(IoLines[i].HostName, LineName2) == 0) {
	    /* OK - found an indirectly connected line.   Report to
	       the caller that the linename to queue is the same
	       and if it is in connect mode, return the line index,
	       else just prepare for LINK_INACTIVE report */
	    if (*PrimLinep < 0)
	      strcpy(LineName, LineName2);
	    strcpy(CharacterSet, "EBCDIC");
	    /* We may have explicite route to our physical link,
	       for example when overriding 'default route' */
	    if (*PrimLinep < 0)
	      *PrimLinep = i;
	    else
	      if (i != *PrimLinep)
		*AltLinep = i;
	    if (IoLines[*PrimLinep].state != ACTIVE) {
	      if (IoLines[i].state == ACTIVE)
		return i;
	      else
		break;
	    } else
	      return *PrimLinep;
	  }
	}

	/* If no alternate route given, then retire... */

	if (*PrimLinep >= 0) /* Found a link earlier, but explicte route
				possibly didn't yield a valid link ! */
	  if (IoLines[*PrimLinep].state == ACTIVE) {
	    logger(1,"NMR: find_line_index('%s') looked into the database and got bad route entry: `%s'\n",
		   NodeName,TempLine);
	    return *PrimLinep;
	  } else
	    return LINK_INACTIVE;

	return NO_SUCH_NODE;
}


/*
 | A command was sent to the local node. Parse it and send the reply to
 | the originator.
 */
void
handle_local_command(OriginNode, UserName, MessageText)
const char	*OriginNode, *UserName, *MessageText;
{
	char	Faddress[20], Taddress[20], line[LINESIZE];
	char	KeyWord[SHORTLINE], param1[SHORTLINE], param2[SHORTLINE];
	int	i, NumKeyWords;
	const char *msg = MessageText;
	const char *p2p = NULL;

	/* Create first the addresses of the sender and receiver */

	sprintf(Faddress, "@%s", LOCAL_NAME);
	sprintf(Taddress, "%s@%s", UserName, OriginNode);

	/* Get the command keywords */

	*line = 0;
	*param1 = 0;
	*param2 = 0;

	NumKeyWords = 0;
	while (*msg == ' ' || *msg == '\t') ++msg;

	if (*msg == '*') /* RSCS comment.. */
	  return;

	i = 0;
	while (*msg != 0 && *msg != ' ' && *msg != '\t') {
	  if (i < (sizeof(KeyWord)-1))
	    KeyWord[i++] = *msg;
	  ++msg;
	}
	KeyWord[i] = 0;
	if (i > 0) NumKeyWords = 1;
	while (*msg == ' ' || *msg == '\t') ++msg;

	i = 0;
	while (*msg != 0 && *msg != ' ' && *msg != '\t') {
	  if (i < (sizeof(param1)-1))
	    param1[i++] = *msg;
	  ++msg;
	}
	param1[i] = 0;
	if (i > 0) NumKeyWords = 2;
	while (*msg == ' ' || *msg == '\t') ++msg;

	i = 0;
	p2p = msg;
	while (*msg != 0 && *msg != ' ' && *msg != '\t') {
	  if (i < (sizeof(param2)-1))
	    param2[i++] = *msg;
	  ++msg;
	}
	param2[i] = 0;
	if (i > 0) NumKeyWords = 3;
	while (*msg == ' ' || *msg == '\t') ++msg;

	if (*msg != 0)
	  NumKeyWords = 4;

	/* Try parsing messages we recognize */

	if (strcmp(KeyWord,"CMD")==0) { /* Redirect a command */
logger(1,"NMR: Local cmd CMD, Taddress='%s', msg='%s'\n",Taddress,MessageText);
	  if (NumKeyWords < 3 ||
	      strlen(param1) > 8) {
	    sprintf(line,"Bad argumentation of CMD -command");
	  } else {
	    char toaddr[20];
	    sprintf(toaddr,"@%s",param1);
	    strncpy(line,p2p,120);
	    line[120] = 0;
	    send_nmr(Taddress, toaddr, line, strlen(line), ASCII, CMD_CMD);
	    return;
	  }
	} else if (strcmp(KeyWord,"MSG")==0 ||
		   strcmp(KeyWord,"M")==0) { /* Redirect a msg */
logger(1,"NMR: Local cmd MSG, Taddress='%s', msg='%s'\n",Taddress,MessageText);
	  if (NumKeyWords < 4 || strlen(param1) > 8 || strlen(param2) > 8) {
	    /* PARAMETER ERROR! */
	    sprintf(line,"Bad argumentation of MSG -command");
	  } else {
	    char toaddr[20];
	    sprintf(toaddr,"%s@%s",param2,param1);
	    strncpy(line,msg,120);
	    line[120] = 0;
	    send_nmr(Taddress, toaddr, line, strlen(line), ASCII, CMD_MSG);
	    return;
	  }
	  
	} else if (*KeyWord == 'Q') {	/* Query command */
	  if (NumKeyWords < 2)
	    sprintf(line, "QUERY command needs parameters");
	  else if (strncmp(param1, "SYS", 3) == 0) {
	    show_lines_status(Taddress,*param2 == 'S');
	    return;
	  } else if (strncmp(param1, "STAT", 4) == 0) {
	    show_lines_stats(Taddress);
	    return;
	  } else {		/* Probably something for a link */
	    switch (*param2) {
	      case 'Q':
	          show_files_queue(Taddress,param1);
		  return;
		  break;
	      case 'F':
		  sprintf(line, "Use QUERY SYSTEM to see file's queue");
		  break;
	      case 'A':
		  sprintf(line, "Use QUERY SYSTEM to find active file");
		  break;
	      case '\0':  /* No parameter - show the route to that node */
		  query_route(Faddress, Taddress, param1);
		  return;
	      default:
		  sprintf(line, "Illegal Query command");
		  break;
	    }
	  }
	} else if (strncmp(KeyWord, "CPQ", 3) == 0) {
	  if (*param1 == 'N') {	/* CPQ Names */
	    list_users(Faddress, Taddress, 'N', "");	/* List all users */
	    return;
	  } else if (*param1 == 'U') {
	    list_users(Faddress, Taddress, 'U', param2);
	    /* List that user only */
	    return;
	  } else if (strcasecmp(param1, "LOG") == 0) {
	    send_welcome_message(Faddress, Taddress);
	    /* Send our WELCOME text */
	    return;
	  } else if ((strcasecmp(param1, "CPU") == 0) ||
		     (strcasecmp(param1, "CPLEVEL") == 0) ||
		     (strcasecmp(param1, "LOG") == 0) ||
		     (strcasecmp(param1, "IND") == 0) ||
		     (*param1 == 'T')) {
	    send_cpu_time(Faddress, Taddress);
	    /* Send our CPU type and time */
	    return;
	  } else
	    sprintf(line, "Illegal CPQuery command syntax. Try HELP");
	} else if (*KeyWord == 'H') {
	  if (!msg_helper(Faddress,Taddress))
	    for (i = 0; *HelpText[i] != '\0'; i++)
	      send_nmr(Faddress, Taddress, HelpText[i], strlen(HelpText[i]),
		       ASCII, CMD_MSG);
	  return;
	}
	/* Create a message text saying that we do
	   not recognise this command */
	else
	  sprintf(line, " Command '%s' unrecognised. Try HELP", MessageText);

	send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
}


/*
 | Show the route of that node. Show both the permannet route (in table) and
 | the active route (if we have alternate route active).
 */
static void
query_route(Faddress, Taddress, NodeName)
const	char	*Faddress, *Taddress;
	char	*NodeName;
{
	char	line[LINESIZE], ActiveRoute[16],
		CharacterSet[16];
	int	i, PrimLine, AltLine;

	/* Modify nodename to upper case */
	upperstr(NodeName);

	/* Get the active route */
	switch ((i = find_line_index(NodeName, ActiveRoute, CharacterSet,
				     &PrimLine, &AltLine))) {
	  case NO_SUCH_NODE:	/* Does not exist at all */
	      sprintf(line, "Node %s not known", NodeName);
	      send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	      return;
	  case ROUTE_VIA_LOCAL_NODE:	/* Not connected via NJE */
	      sprintf(line, "Node %s is local", NodeName);
	      send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	      return;
	  case LINK_INACTIVE:	/* Both link and alternate routes inactives */
	      break; /* Bypass to below.. */
	  default:
	      sprintf(line, "Node %s is routed via %s (%sactive)", NodeName,
		      IoLines[PrimLine].HostName,
		      IoLines[PrimLine].state == ACTIVE ? "" : "in");
	      if (AltLine >= 0) {
		char *s = line + strlen(line);
		sprintf(s," Alt: %s (%sactive)",
			IoLines[AltLine].HostName,
			IoLines[AltLine].state == ACTIVE ? "" : "in");
	      }
	      
	      send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	      return;
	}

	/* OK - The link is defined, but inactive.  If the node to route
	   through is different than the original nodename, then maybe
	   we have a line with that name which is inactive and we
	   got an alternate route. 				   */
	sprintf(line,"Link %s inactive. Routed via %s", NodeName, ActiveRoute);

	if (strcasecmp(NodeName, ActiveRoute) != 0) {
	  for (i = 0; i < MAX_LINES; i++) {
	    if (IoLines[i].HostName[0] != '\0' && /* This line is defined */
		strcasecmp(IoLines[i].HostName, NodeName) == 0) {
	      sprintf(line, "Link %s inactive. Routed via %s",
		      NodeName, ActiveRoute);
	      break;
	    }
	  }
	}

	send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
}
