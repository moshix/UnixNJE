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

 | VMS environment specific versions of some routines.
/*
 | Loop over all users in the system.
 | Look for those that are interactive users.
 | from those either list all of them (if UserName points to Null), or look
 | for a  specific username (if UserName points to some string).
 | If a list of users is requested, we try to block 6 usernames in a single
 | line.
 */
static struct	itmlist {	/* To retrive information from $GETJPI */
		short	length;
		short	code;
		long	address;
		long	rtn;
	} list[3];
void
list_users(Faddress, Taddress, cmd, UserName)
const char	*Faddress, *Taddress, *UserName;
const char	 cmd;
{
	char	line[LINESIZE];
	static	char	username[13];
	static	long	mode;
	static	long	status, PID;			/* Use for search */
	register int	i, LinePosition, counter;
	char	*p;

	if (*UserName == '\0') {
	  sprintf(line, "Users logged in on %s:", LOCAL_NAME);
	  send_nmr(Faddress, Taddress, line, strlen(line),
		   ASCII, CMD_MSG);
	}

	/* Init item's list */
	list[0].code = JPI$_MODE;
	list[0].address = &mode;  list[0].length = 4;
	list[1].code = JPI$_USERNAME;
	list[1].address = username;  list[1].length = (sizeof username);
	list[2].code = list[2].length = 0;

	/* Now - Call JPI */
	PID = -1; counter = LinePosition = 0;
	for (;;) {
	  status = sys$getjpiw(0, &PID, 0, list, 0, 0, 0);
	  if ((status & 0x1) == 0)
	    break;		/* Abort on any error */
	  if (mode != 3)		/* Not interactive */
	    continue;
	  if ((p = strchr(username, ' ')) != NULL) *p = '\0';
	  if (*UserName == '\0') { /* List all users */
	    strcpy(&line[LinePosition], username);
	    LinePosition += strlen(username);
	    /* Pad the username with blanks to 12 characetrs */
	    i = LinePosition;
	    LinePosition = ((LinePosition + 12) / 12) * 12;
	    for (; i < LinePosition; i++) line[i] = ' ';
	    if (counter++ >= 5) {
	      line[LinePosition] = '\0';
	      send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	      LinePosition = counter = 0;
	    }
	  } else {		/* Check whether this is the username */
	    if (strcasecmp(username, UserName) == 0) {
	      sprintf(line, "%s logged in on %s", username, LOCAL_NAME);
	      send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	      return;
	    }
	  }
	}

	/* Check whether there was a request for a specific user.
	   If there was and we are here, then the user is not logged-in. */

	if (*UserName != '\0') {
	  sprintf(line, "%s is not logged in", UserName);
	  send_nmr(Faddress, Taddress, line, strlen(line),
		   ASCII, CMD_MSG);
	} else {
	  if (counter > 0) {	/* Incomplete line */
	    line[LinePosition] = '\0';
	    send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	  }
	  sprintf(line, "End of list");
	  send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	}
}

/*
 | Send the WELCOME file to the requestor. Try getting the translation of the
 | logical name SYS$WELCOME. If it doesn't start with @, send it as-is. If it
 | starts, try sending the file.
 */
void
send_welcome_message(Faddress, Taddress)
const char	*Faddress, *Taddress;
{
	long	status;
	static long		NameLength;
	static struct DESC	logname, tabname;
	char	*p, line[LINESIZE];
	FILE	*fd;
	char	SYSTEM_TABLE[] = "LNM$SYSTEM_TABLE";	/* The logical names
							   table. */
	char	LOGNAME[] = "SYS$WELCOME";

/* Get the translation of it */
	tabname.address = SYSTEM_TABLE;
	tabname.length  = strlen(SYSTEM_TABLE);
	logname.address = LOGNAME;
	logname.length  = strlen(LOGNAME);
	logname.type    = tabname.type = 0;

	list[0].length  = LINESIZE;
	list[0].code    = LNM$_STRING;	/* Get the equivalence string */
	list[0].address = line;
	list[0].rtn     = &NameLength;	/* The length of returned string */
	list[1].length  = list[1].code = list[1].address = 0;

	if (((status = sys$trnlnm(0,&tabname, &logname,0,list)) & 0x1) == 0) {
	  logger(1, "NMR, Can't get logical translation of '%s'\n", LOGNAME);
	  return;
	}
	line[NameLength] = '\0';

	/* Check whether it starts with @ */
	if (*line != '@') {	/* Yes, send as-is */
	  send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	  return;
	}

	/* Doesn't start with @ - open the file */
	if ((fd = fopen(&line[1], "r")) == NULL) {
	  logger(1, "NMR, Can't open '%s' for CPQ LOG command\n",
		 &line[1]);
	  sprintf(line, "LOG file not available");
	  send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	  return;
	}
	while (fgets(line, sizeof line, fd) != NULL) {
	  if ((p = strchr(line, '\n')) != NULL) *p = '\0';
	  send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
	}
	fclose(fd);
}


/*
 | Get the CPU type and VMS version, and send it with the current time.
 */
void
send_cpu_time(Faddress, Taddress)
const char	*Faddress, *Taddress;
{
	long	status;
	static	int	CpuLength;
	static char	CpuName[31], CpuVersion[9];
	char	*p, line[LINESIZE];
	char	*month[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug",
			    "Sep","Oct","Nov","Dec"};
	char	*weekday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	struct	tm	*tm, *localtime();
	long	clock;

	time(&clock);		/* Get the current time */
	tm = localtime(&clock);

	list[0].length = sizeof CpuName;
	list[0].code = SYI$_HW_NAME;
	list[0].address = CpuName;
	list[0].rtn = &CpuLength;
	list[1].length = sizeof CpuVersion;
	list[1].code = SYI$_VERSION;
	list[1].address = CpuVersion;
	list[1].rtn = 0;
	list[2].length = list[2].code = list[2].address = 0;

	if (((status = sys$getsyiw(0, 0, 0, list, 0, 0, 0)) & 0x1) == 0) {
	  logger(1, "NMR, can't get CPU type and version\n");
	  return;
	}

	CpuName[CpuLength] = '\0';
	CpuVersion[8] = '\0';
	if ((p = strchr(CpuVersion, ' ')) != NULL) *p = '\0';
	sprintf(line, "%s running VMS-%s, using HUJI-NJE, %-3.3s %d-%-3.3s-%d %d:%02d %s\n",
		CpuName, CpuVersion,
		weekday[tm->tm_wday], tm->tm_mday, month[tm->tm_mon],
		tm->tm_year, tm->tm_hour, tm->tm_min, GMT_OFFSET);
	send_nmr(Faddress, Taddress, line, strlen(line), ASCII, CMD_MSG);
}
