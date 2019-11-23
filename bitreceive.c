/*  BITRCV -- BITRECEIVE -- Counterpart of BITSEND program

    This is for HUJI-NJE/FUNET-NJE system

    ???  Obsoleted with `receive' ???
 */

#define BitcatPath	"/usr/local/bin/huji/bitcat"

#include "prototypes.h"
#include "clientutils.h"    

#define STRING_LEN	256
#define LONG_STRING	1024

#define TRANSF_NONAME	8
#define TRANSF_NOTYPE	4
#define TRANSF_FILE	2
#define TRANSF_MAIL	1
#define TRANSF_UNKNOWN	0
#define TRANSF_TYPE	0xf

#define	lastch(S)   (S[strlen (S) - 1])
/* #define toascii(C)  (C & 0x7f) */

extern char   **environ;
static char    folders[LONG_STRING];

/*------------------------------------------------------------*/

char           *
getline (stream, buf, bufsize)
FILE *stream;
char *buf;
int bufsize;
{
    register int    n,
                    c;
    register char  *t;

    if (!buf)
        return NULL;

    for (n = bufsize, t = buf;
            --n && ((c = fgetc (stream)) != '\n') && (c != EOF);)
        *t++ = c;

    *t = '\0';

    if ((t == buf) && (c == EOF))
        return NULL;

    return buf;
}

char	*
set_folders (char *folder_dir) {

    if (folder_dir[0] == '/') {
	strncpy (folders, folder_dir, sizeof (folders));
	folders[sizeof (folders) - 1] = '\0';
    }
    else {
	return ((char *) 0);
    }
    return (folder_dir);
}

int
allow_clobber (char *fname) {

    static int	first_time = 1;
    char	tmpstr[64];
    char	*cc;

    if (! isatty (fileno (stdin))) {
	if (first_time) {
	    fprintf (stderr, "Your stdin is not bound to a tty.\n");
	    first_time = 0;
	}
	return (0);
    }

    fprintf (stderr, "You have an old version of %s.\n", fname);

ReadAnswer:
    fprintf (stderr,
	     "Move it to %s.BAK and create a new one %s? ",
	     fname, "(y/n)");

    fflush (stdin);
    fgets (tmpstr, sizeof (tmpstr), stdin);

    for (cc = tmpstr; *cc && isspace (*cc); cc++) ;

    switch (tolower (*cc)) {

    case    'y':
	return (1);

    case    'n':
	return (0);

    default:
	fprintf (stderr, "Please answer yes or no.\n");
	goto ReadAnswer;
    }
}

int
do_bitsave (char *BitFName, char *RawTargetName, int yes_mode) {

    size_t	    xfrsize;
    int		    clobber = 0;
    struct stat	    status;
    char	    TargetName[STRING_LEN];
    char	    *tmp;
    FILE	    *input,
		    *output;
    char	    buffer[BUFSIZ],
		    cmd[LONG_STRING];

    sprintf (TargetName, "~/%s", BITDIRNAME);

    tmp = expand_filename (TargetName);

    if (!tmp) {
	fprintf (stderr, "Failed to expand `%s'.\n", TargetName);
	exit (1);
    }

    if (stat (tmp, &status)) {
	if (errno != ENOENT) {
	    perror (TargetName);
	    exit (1);
	}

	if (mkdir (tmp, 0700)) {
	    perror (tmp);
	    fprintf (stderr, "Cannot create directory `%s'.\n", tmp);
	    exit (1);
	}
    }

    sprintf (TargetName, "~/%s/%s", BITDIRNAME, RawTargetName);

    tmp = expand_filename (TargetName);

    if (!tmp) {
	fprintf (stderr, "Failed to expand `%s'.\n", TargetName);
	exit (1);
    }

    if (!stat (tmp, &status)) {
	if (yes_mode || (clobber = allow_clobber (RawTargetName))) {
	    strcpy (buffer, tmp);
	    strcat (buffer, ".BAK");
	    if (rename (tmp, buffer)) {
		perror (buffer);
		return (-1);
	    }
	    fprintf (stderr,
		     "Renamed an old version of %s to %s.BAK.\n\n",
		     RawTargetName, RawTargetName);
	}
	else {
	    return (-1);
	}
    }
    else if (errno != ENOENT) {
	perror (tmp);
	return (-1);
    }

    output = fopen (tmp, "w");

    if (!output) {
	fprintf (stderr, "Cannot create file `%s'.\n", TargetName);
	return (-1);
    }

    sprintf (cmd, "%s %s", BitcatPath, BitFName);

    input = popen (cmd, "r");

    while (xfrsize = fread (buffer, sizeof (char), BUFSIZ, input)) {
	if (xfrsize > fwrite (buffer, sizeof (char),
			      xfrsize, output)) {
	    perror (TargetName);
	    return (-1);
	}
    }

    pclose (input);
    fclose (output);

    return (0);
}

int
GetTransferName (FILE * file, char *TransferName)
{

    register char  *line,
                   *ptr;
    char            TransferType[64],
		    FileName[STRING_LEN],
		    FileExt[STRING_LEN],
		    buff[LONG_STRING];
    int		    retcode = 0;

    while (line = getline (file, buff, LONG_STRING)) {
	for (ptr = line; isalnum (*ptr); ptr++) {
	    *ptr = toupper (*ptr);
	}

	if (!strncmp (line, "END:", 4)) {
	    break;
	}
	else if (!strncmp (line, "FNM:", 4)) {
	    for (ptr = &line[4];
		 (isspace (*ptr) || ispunct (*ptr)); ptr++);
	    strcpy (FileName, ptr);
	    for (ptr = FileName + strlen (FileName) - 1;
		 (ptr >= FileName) && (isspace (*ptr) || ispunct (*ptr));
		 *ptr-- = '\0');
	    for (ptr = FileName; *ptr; ptr++) {
		*ptr = isspace (*ptr) ? '_' : *ptr;
	    }
	}
	else if (!strncmp (line, "EXT:", 4)) {
	    for (ptr = &line[4];
		 (isspace (*ptr) || ispunct (*ptr)); ptr++);
	    strcpy (FileExt, ptr);
	    for (ptr = FileExt + strlen (FileExt) - 1;
		 (ptr >= FileExt) && (isspace (*ptr) || ispunct (*ptr));
		 *ptr-- = '\0');
	    for (ptr = FileExt; *ptr; ptr++) {
		*ptr = isspace (*ptr) ? '_' : *ptr;
	    }
	}
	else if (!strncmp (line, "TYP:", 4)) {
	    for (ptr = &line[4]; isspace (*ptr); ptr++);
	    strcpy (TransferType, ptr);
	    for (ptr = TransferType + strlen (TransferType) - 1;
		 (ptr >= TransferType) && isspace (*ptr); *ptr-- = '\0');
	    for (ptr = TransferType; *ptr; ptr++) {
		*ptr = isspace (*ptr) ? '_' : toupper (*ptr);
	    }
	}
    }

    sprintf (TransferName, "%s%s%s",
	     FileName,
	     FileExt[0] ? "." : "",
	     FileExt);

    if (!strlen (TransferName))
	retcode |= TRANSF_NONAME;

    if (!strlen (TransferType))
	retcode |= TRANSF_NOTYPE;
    else if (!strcmp (TransferType, "FILE"))
	retcode |= TRANSF_FILE;
    else if (!strcmp (TransferType, "MAIL"))
	retcode |= TRANSF_MAIL;
    else
	retcode |= TRANSF_UNKNOWN;

    return (retcode);
}

int
main (unsigned int argc, char *argv[])
{
    static char	    who_am_I[STRING_LEN] = "";
    char	    TrueName[STRING_LEN];
    char	    bitspoolname[LONG_STRING],
		    BitFName[LONG_STRING];
    DIR		    *dirp;
    struct dirent   *SpoolEnt;
    struct  passwd  *pwdp, pwd;
    char	    *tmp;
    int		    FileCount = 0,
		    Bogus = 0,
		    retcode;
    FILE	    *bitfile;
    static int	    list_only_mode = 0,
		    yes_mode = 0,
		    tell_usage = 0;
    int		    opt,
		    getopt ();
    extern int	    optind;
    extern char	    *optarg;

    setlinebuf (stdin);

    if (pwdp = getpwuid (getuid ())) {
	memcpy (&pwd, pwdp, sizeof (pwd));
    }
    else {
	fprintf (stderr, "Who are you?\n");
	exit (1);
    }

    while ((opt = getopt (argc, argv, "lyu:")) != EOF) {

	switch (opt) {

	case	'l':
	    list_only_mode = 1;
	    break;

	case	'y':
	    yes_mode = 1;
	    break;

	case	'u':
	    if (!optarg || !*optarg) {
		fprintf (stderr,
			 "%s: Missing a name after -u option.\n",
			 argv[0]);
		exit (1);
	    }

	    strcpy (who_am_I, optarg);

	    if (pwd.pw_uid) {
		fprintf (stderr,
			 "%s: Only root can use -u option.\n",
			 argv[0]);
		exit (1);
	    }

	    break;

	case	'?':
	default:
	    tell_usage = 1;
	    break;
	}
    }

    if ((optind < argc) || tell_usage) {
	fprintf (stderr,
		 "Usage: %s [-l] [-y] [-u user]\n",
		 argv[0]);
	exit (1);
    }

    if (!who_am_I[0]) {
	strcpy (who_am_I, pwd.pw_name);
    }

    for (tmp = who_am_I; *tmp; tmp++) {
	/* *tmp = toascii (*tmp); */
	*tmp = toupper (*tmp);
    }

    sprintf (bitspoolname, "%s/%s", BaseBitSpool, who_am_I);

    dirp = opendir (bitspoolname);

    if (!dirp) {
	goto ExitBitReceive;
    }
    
    while (SpoolEnt = readdir (dirp)) {
	if (!SpoolEnt->d_name[0] ||
	    (SpoolEnt->d_name[0] == '.')) {
	    continue;
	}

	sprintf (BitFName, "%s/%s", bitspoolname, SpoolEnt->d_name);

	bitfile = fopen (BitFName, "r");

	if (!bitfile) {
	    perror (BitFName);
	    continue;
	}

	FileCount++;

	switch ((retcode = GetTransferName (bitfile, TrueName))
		& TRANSF_TYPE) {

	case	TRANSF_UNKNOWN:
	    fprintf (stderr,
		     "%s: Transfer type for `%s' unknown. File left as is.\n",
		     argv[0], BitFName);
	    continue;

	case	TRANSF_NOTYPE:
	    if (retcode & TRANSF_NONAME) {
		fprintf (stderr,
			 "%s: `%s' supposedly not a bitnet spool file\n",
			 argv[0],
			 BitFName);
	    }
	    break;

	case	TRANSF_MAIL:
	    do_mailify (BitFName);
	    break;

	case	TRANSF_FILE:
	case	TRANSF_NONAME:
	default:
	    if (!TrueName[0]) {
		sprintf (TrueName, "Bogus-%d", Bogus++);
		fprintf (stderr,
			 "BitNet transfer has no name. %s to `~/%s/%s'.\n",
			 list_only_mode
			 ? "Would be saved"
			 : "Saving",
			 BITDIRNAME, TrueName);
	    }
	    else {
		fprintf (stderr,
			 list_only_mode
			 ? "%s waiting in the bitnet queue.\n"
			 : "Collecting %s from the bitnet queue.\n",
			 TrueName);
	    }

	    if (!list_only_mode
		&& do_bitsave (BitFName, TrueName, yes_mode)) {
		fprintf (stderr,
			 "Failed to collect `%s'\n\n",
			 TrueName);
	    }
	    else if (!list_only_mode) {
		unlink (BitFName);
	    }
	    break;
	}

	fclose (bitfile);
    }

    closedir (dirp);

ExitBitReceive:
    if (!FileCount) {
	printf ("No files waiting for %s in the bitnet queue.\n\n",
		who_am_I);
    }

    exit (0);
}	
