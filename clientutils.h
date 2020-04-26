/*	HUJI-NJE		Client utilities Header file
 *
 *	Common set of client used utility routines.
 *	These are collected to here from various modules for eased
 *	maintance.
 *
 *	Matti Aarnio <mea@nic.funet.fi>  12-Feb-1991, 1993, 1994
 */

#ifndef	__
# ifdef	__STDC__
#  define __(x) x
# else
#  define __(x) ()
#  define volatile /*K&R has no volatile */
#  define const    /*K&R has no const    */
# endif
#endif

#ifndef CMD_SHUTDOWN_ABRT	/* Copied from  consts.h */
/* The command mailbox messages' codes */

#define	CMD_SHUTDOWN_ABRT 1	/* Shutdown the NJE emulator immediately */
#define	CMD_SHUTDOWN	14	/* Shut down after all lines are idle.	*/
#define	CMD_SHOW_LINES	2	/* Show lines status			*/
#define	CMD_QUEUE_FILE	3	/* Add a file to the queue		*/
#define	CMD_SHOW_QUEUE	4	/* Show the files queued		*/
#define	CMD_SEND_MESSAGE 5	/* Send NMR message			*/
#define	CMD_SEND_COMMAND 6	/* Send NMR command			*/
#define	CMD_START_LINE	7	/* Start an INACTIVE or SIGNOFF line	*/
#define	CMD_STOP_LINE	8	/* Stop the line after the current file.*/
#define	CMD_START_STREAM 9	/* Start a REFUSED stream		*/
#define	CMD_FORCE_LINE	11	/* Shut that line immediately.		*/
#define	CMD_FORCE_STREAM 12	/* Shut this stream immediately		*/
#define	CMD_DEBUG_DUMP	15	/* DEBUG - Dump all buffers to logfile	*/
#define	CMD_DEBUG_RESCAN 16	/* DEBUG - Rescan queue diretory
				   (to be used when manually rerouting
				   files to another line)		*/
#define	CMD_LOGLEVEL	17	/* Change log level during run		*/
#define	CMD_CHANGE_ROUTE 18	/* Change the route in database		*/
#define	CMD_GONE_ADD	19	/* Register a user in GONE database by YGONE */
#define	CMD_GONE_DEL	20	/* Remove a user from GONE database by YGONE */
#define	CMD_GONE_ADD_UCP 21	/* Register a user in GONE database by UCP */
#define	CMD_GONE_DEL_UCP 22	/* Remove a user from GONE database by UCP */
#define CMD_MSG_REGISTER 23	/* An MSG-registry command		*/
#define CMD_EXIT_RESCAN 24	/* Do a exit-rescan w/o sending SIGHUP	*/
#define CMD_ROUTE_RESCAN 25	/* Do a exit-rescan w/o sending SIGHUP	*/

/* The codes that a message can be in */
#define	ASCII		0
#define	EBCDIC		1
#define	BINARY		2

/* The file's type (Bitmask) */
#define	F_MAIL		0x0	/* Mail message				*/
#define	F_FILE		0x0001	/* File - send it in NETDATA format	*/
#define	F_PUNCH		0x0002	/* File - send it in PUNCH format	*/
#define	F_PRINT		0x0004	/* File - send it in PRINT format	*/
#define	F_ASA		0x0008	/* ASA carriage control			*/
#define F_JOB		0x0010	/* SYSIN job!				*/
#define	F_NOQUIET	0x0080	/* Don't use the QUIET form code	*/

/* SRCB types: */
#define	NJH_SRCB	0xC0	/* Job Header				*/
#define	DSH_SRCB	0xE0	/* Data set header			*/
#define	NJT_SRCB	0xD0	/* Job trailer				*/
#define	DSHT_SRCB	0xF0	/* Data set trailer. Not used		*/
#define	CC_NO_SRCB	0x80	/* No carriage control record		*/
#define	CC_MAC_SRCB	0x90	/* Machine carriage control record	*/
#define	CC_ASA_SRCB	0xA0	/* ASA carriage control			*/
#define	CC_CPDS_SRCB	0xB0	/* CPDS carriage control		*/

#endif	/* ---- end of 'copied from  consts.h' ---- */

extern char	*upperstr __((char *str));
extern char	*lowerstr __((char *str));
extern char	*strsave __((char *str));
extern int	send_cmd_msg __((const void *cmdbuf,const int cmdlen,
				 const int offlineok));

extern int	LuserUidLevel;
extern int	read_configuration __((void));
extern void	read_etable     __((void));
extern void	ASCII_TO_EBCDIC __(( const void *InString, void *OutString, const int Size ));
extern void	EBCDIC_TO_ASCII __(( const void *InString, void *OutString, const int Size ));
extern void	PAD_BLANKS	__(( void *String, const int StartPos, const int FinalPos ));
extern int	Uwrite __(( FILE *outfil, const void *buf, const int outsize ));
extern int	Uread __(( void *buf, const int maxsize, FILE *infil ));
extern int	submit_file __(( const char *FileName, const int FileSize ));
extern int	parse_header __(( FILE *fd, char *Frm, char *Toa, char *Fnm,
				  char *Ext,int *Typ, char *Cls, char *For,
				  int *Fmt, char *Tag, int *Fid ));
extern	char   *ExpandHomeDir __((const char *PathOrDir, const char *HomeDir,
				  const char *ToUser, char *path));
extern void	logger    __((int lvl, ...));
extern void	trace __(( const void *ptr, const int n, const int lvl ));
extern char	*local_time __(( void ));
extern volatile void  bug_check __(( const char *text ));
extern char	*mcuserid __((char *from ));
