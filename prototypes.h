/* This file is for FUNET-NJE and it should contain all prototypes
   for all routines used in this program.
 */

/* NOTE::  FOR UNIX - ESPECIALLY SunOS 4.1 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#if !defined(__STDC__) && !defined(convex)
# include <memory.h>
#endif
#ifdef DIRENTFILE
# include DIRENTFILE
#endif
#include <sys/stat.h>
#ifdef	AIX
# include <sys/select.h>
#endif /* AIX */
#include <errno.h>
#include <pwd.h>
#include <utmp.h>

#ifdef	DEBUG_FOPEN
#define fopen _nje_fopen
#define fclose _nje_fclose
#define fdopen _nje_fdopen
#define freopen _nje_freopen
#endif

#if 0
/*      ----- this should be at routines needing it.. */
#ifdef	GDBM
# include <gdbm.h>
#endif
#ifdef  NDBM
# include <ndbm.h>
#endif
#endif

#ifndef __
# ifdef __STDC__
#  define __(x) x  /* With any STDC it is possible to check prototypes */
# else
#  define __(x) ()
#  define volatile /* Propably 'volatile' is unknown keywork... */
#  define const  /* Propably 'const' is unknown keyword also... */
# endif
#endif

/* A patch to ConvexOS 10.2 -- feof() LIBRARY routine is broken, it tests
   wrong bit, and its  stdio.h  is broken..  Next library release works,
   says Convex.. */

#if defined(__convex__) && !defined(feof)
#  define   feof(p)     ((p)->_flag&_IOEOF)
#endif


/* There are systems with proper library prototypes available, let them
   not to surprise here! */

extern int	errno;
#ifdef __DARWIN_UNIX03
extern const int	sys_nerr;	/* Maximum error number recognised */
#else
extern int	sys_nerr;	/* Maximum error number recognised */
#endif

/* extern char	*sys_errlist[];	*/ /* List of error messages */
#define	PRINT_ERRNO	(errno > sys_nerr ? "***" : sys_errlist[errno])


#if	defined(__POSIX_SOURCE) || defined(__POSIX_C_SOURCE) || defined(__svr4__)
#define GETDTABLESIZE(x) sysconf(_SC_OPEN_MAX)
#else
#if	defined(BSD)
#define GETDTABLESIZE(x) getdtablesize()
#else
#define GETDTABLESIZE(x) NOFILE
#endif
#endif

#if defined(USE_OWN_PROTOS)

extern void	srandom __((const int seed));
extern long	random __((void));

extern int	tolower __((const int c));
extern int	toupper __((const int c));

extern char    *strcat __((char *s1, const char *s2));
extern char    *strncat __((char *s1, const char *s2, const size_t n));
extern int	strcmp __((const char *s1, const char *s2));
extern int	strncmp __((const char *s1, const char *s2, const size_t n));
extern int	strcasecmp __((const char *s1, const char *s2));
extern int	strncasecmp __((const char *s1, const char *s2, const size_t n));
extern char    *strcpy __((char *s1, const char* s2));
extern char    *strncpy __((char *s1, const char* s2, const size_t n));
extern size_t	strlen __((const char *s));
extern char    *strchr __((const char *s, const int c));
extern char    *strrchr __((const char *s, const int c));

extern void    *memchr __((const void *s, const int c, size_t));
extern int	memcmp __((const void *s1, const void *s2, size_t));
extern void    *memcpy __((void *s1, const void *s2, size_t));
extern void    *memset __((void *s, int c, size_t));

/*extern char    *sprintf __((char *buf, const char *fmt, ...));*/
extern int	fprintf __((FILE *fd, const char *fmt, ...));
extern int	vfprintf __((FILE *fd, const char *fmt, ...));
extern int	printf __((const char *fmt, ...));
extern int	sscanf __((const char *buf, const char *fmt, ...));
extern int	fscanf __((FILE *fd, const char *fmt, ...));

extern FILE    *fopen __((const char *filename, const char *type));
extern int	fclose __((FILE *stream));
extern int	fflush __((FILE *stream));
extern int	fread __(( void *ptr, const int size, const int nitems, FILE *stream ));
extern int	fwrite __(( const void *ptr, const int size, const int nitems, FILE *stream ));
extern char    *fgets __(( char *s, const int n, FILE *stream ));
extern int	fputs __(( const void *s, FILE *stream ));
extern int	puts __(( const char *s ));
/* Following two exist in SunOS stdio.h's getc(), and putc() macroes */
extern int	_flsbuf __(( unsigned char c, FILE *stream ));
extern int	_filbuf __(( FILE *stream ));
extern void	setbuf __(( FILE *stream, char *buf ));

extern int	chown __(( char *path, uid_t owner, gid_t group ));
extern int	chmod __(( char *path, mode_t mode ));
extern int	unlink __(( char *path ));
extern int	link __(( char *name1, char *name2 ));
extern char    *mktemp __(( char *template ));
extern int	mkstemp __(( char *template ));


extern void	free __(( void *ptr ));
extern void    *malloc __(( size_t size ));
extern void    *realloc __(( void *ptr, size_t size ));

extern time_t	time __(( time_t *tloc ));
extern int	gettimeofday __(( struct timeval *tp, struct timezone *tzp ));
extern struct tm *localtime __(( time_t *clock ));
extern struct tm *gmtime __(( time_t *clock ));
extern int	strftime __((char *buf, int bufsize, char *fmt, struct tm *tm));

extern pid_t	wait __(( int *loc ));
#if	defined(BSD)||defined(sun)
extern pid_t	wait3 __(( union wait *statusp, const int options, struct rusage *rusage ));
#endif
#ifdef _POSIX_SOURCE
extern int	waitpid __(( const pid_t pid, int *status, const int options ));
#endif /* _POSIX_SOURCE */
extern pid_t	fork __(( void ));
extern int	execve __(( char *path, char *argv[], char *envp[] ));
extern int	execl __(( char *path, ... ));
extern int	system __(( const char *cmdstr ));
extern void   (*signal __(( const int sig, void (*func))))();
extern unsigned	sleep __(( unsigned int secs ));
extern char    *getenv __(( const char *varname ));
extern unsigned alarm __(( unsigned seconds ));

#ifdef	DIRENTFILE
extern DIR     *opendir __(( const char *filename ));
extern DIRENTTYPE *readdir __(( DIR *dirp ));
extern int	closedir __(( DIR *dirp ));
#endif
extern int	mkdir __(( char *path, mode_t mode ));

extern void exit __(( int status ));
extern void _exit __(( int status ));
extern pid_t	getpid __(( void ));
extern uid_t	getuid __(( void ));
extern uid_t	geteuid __(( void ));
extern gid_t	getgid __(( void ));
extern gid_t	getegid __(( void ));
extern int	setuid __(( uid_t uid ));
extern int	seteuid __(( uid_t uid ));
extern int	setruid __(( uid_t uid ));
extern int	setreuid __(( uid_t ruid, uid_t euid ));
extern int	setgid __(( gid_t gid ));
extern int	setegid __(( gid_t gid ));
extern int	setrgid __(( gid_t gid ));
extern int	setregid __(( gid_t rgid, gid_t egid ));

extern struct hostent *gethostbyname __(( const char *HostName ));
extern unsigned long inet_addr __(( const char *cp ));
extern char    *inet_ntoa __(( const struct in_addr in ));

extern int	socket __(( const int domain, const int type, const int protocol));
extern int	bind __(( const int fd, struct sockaddr *name, const int namelen ));
extern int	connect __(( const int fd, const struct sockaddr *addr, const int addrlen ));
extern int	listen __(( const int sock, const int maxbacklog ));
extern int	accept __(( const int fd, struct sockaddr *addr, int *addrlen ));
extern int	setsockopt __(( const int sock, const int level, const int optname, const void *optval, const int optlen ));
extern int	getsockopt __(( const int sock, const int level, const int optname, void *optval, const int optlen ));

extern int	open __(( const char *path, const int flags, const int mode ));
extern int	close __(( int fd ));
extern int	write __(( int fd, char *buf, unsigned int length ));
extern int	read __(( int fd, char *buf, unsigned int length ));
extern int	fseek __(( FILE *stream, const long offset, const int ptrname ));
extern int	rename __(( const char *from, const char *to ));
extern int	stat __(( char *path, struct stat *buf ));
#ifdef	HAS_LSTAT
extern int	lstat __(( char *path, struct stat *buf ));
#endif
extern int	fstat __(( int fd, struct stat *buf ));
extern int	getdtablesize __(( void ));
extern int	ioctl __(( const int fd, const int request, void *args));
extern void	bzero __(( void *buf, const int size ));
extern int	select __(( const int width, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout ));
extern int	recv __(( const int fd, void *buf, const int len, const int flags ));
extern int	recvfrom __(( const int fd, void *buf, const int len, const int flags, struct sockaddr_in *from, int *fromlen ));
extern int	sendto __(( const int fd, const void *buf, const int len, const int flags, const struct sockaddr_in *to, const int tolen ));
extern void	perror __((const char *Str));

extern struct passwd *getpwnam __(( const char *name ));

#endif /* not when library prototypes are available.. */


/* ---- various source modules ---- */

#ifdef MAX_STREAMS  /* All main-program modules include  consts.h */

/* main.c -shrunk*/
/* main() */
extern void	can_shut_down __(( void ));

/* logger.c - shrunk */
extern void	logger    __((int lvl, ...));
extern void	trace __(( const void *ptr, const int n, const int lvl ));
extern char    *local_time __(( void ));
extern volatile void  bug_check __(( const char *text ));

/* read_configuration.c */
extern int	read_configuration __(( void ));
extern char	FileExits[];	/* Filename for exit config */
extern char	MsgExits[];	/* Filename for exit config */

/* headers.c */
extern void	init_headers __(( void ));

/* unix.c -shrunk*/
extern void	init_command_mailbox __(( void ));
extern void	close_command_mailbox __(( void ));
extern void	send_opcom __(( const char *text ));
extern void	init_timer __(( void ));
extern time_t	timer_ast __(( void ));
extern int	queue_timer __(( const int expiration, const int Index, const TimerType WhatToDo ));
extern void	dequeue_timer __(( const int TimerIndex ));
extern void	delete_line_timeouts __(( const int Index ));
extern void	poll_sockets __(( void  ));
extern void	handle_childs __(( const int n ));
extern void	handle_sighup __(( const int n ));
extern void	handle_sigterm __(( const int n ));
extern void	handle_sigusr1 __(( const int n ));
extern void	handle_sigalrm __(( const int n ));
extern void	log_line_stats __(( void ));
extern int	find_file __(( const char *FileMask, char *FileName, DIR **context ));
extern int	open_xmit_file __(( const int Index, const int Stream, const char *FileName ));
extern int	open_recv_file __(( const int Index, const int Stream ));
extern int 	uwrite __(( const int Index, const int DecimalStreamNumber, const void *buffer, const int size));
extern int	uread __(( const int Index, const int DecimalStreamNumber, unsigned char *string, const int size ));
extern void	delete_file __(( const int Index, const int Stream, const int direction ));
extern int	close_file __(( const int Index, const int Stream, const int direction ));
extern char    *rename_file __(( struct FILE_PARAMS *FileParams, const int flag, const int direction));
extern int	get_file_size __(( const char *FileName ));
extern void	ibm_time __(( u_int32 *QuadWord ));
extern time_t	ibmtime2unixtime __(( const u_int32 *QuadWord ));
extern int	make_dirs __(( char *path ));
extern void	read_ebcasc_table __(( char *filepath ));

/* unix_tcp.c -shrunk*/
extern void	init_active_tcp_connection __(( const int Index, const int finalize ));
extern void	init_passive_tcp_connection __(( const int TcpType ));
extern void	send_unix_tcp __(( const int Index, const void *line, const int size ));
extern void	tcp_partial_write __(( const int Index, const int flg ));
extern void	close_unix_tcp_channel __(( const int Index ));
extern void	accept_tcp_connection __(( void ));
extern void	read_passive_tcp_connection __(( void ));
extern void	unix_tcp_receive __(( const int Index, struct LINE *Line ));
extern int	writen __(( const int fd, const void *buf, const int len ));
extern int	readn __(( const int fd, void *buf, const int len ));

/* file_queue.c */
extern int	init_files_queue __(( void ));
extern void	queue_file __(( const char *FileName, const int FileSize, const char *ToAddr, struct QUEUE *Entry ));
extern struct QUEUE *build_queue_entry __((const char *, const int, const int, const char *, struct FILE_PARAMS *));
extern void	add_to_file_queue __(( struct LINE *, const int, struct QUEUE *));
extern void	show_files_queue __(( const char *UserName, char *LinkName ));
extern void	requeue_file_entry __(( const int Index, struct LINE *temp ));
extern struct QUEUE *dequeue_file_entry_ok __(( const int Index, struct LINE *temp, const int freeflg ));
extern struct QUEUE *pick_file_entry __(( const int Index, const struct LINE *temp ));
extern int	delete_file_queue __(( struct LINE *Line ));
extern int	requeue_file_queue __(( struct LINE *Line ));

/* io.c -shrunk*/
extern void	show_lines_status __(( const char *ToWho, const int infoprint));
extern void	show_lines_stats __(( const char *ToWho));
extern void	init_communication_lines __(( void ));
extern void	init_link_state __(( const int Index ));
extern void	restart_line __(( const int Index ));
extern void	queue_receive __(( const int Index ));
extern void	send_data __(( const int Index, const void *buffer, const int Size, const int AddEnvelope ));
extern void	close_line __(( const int Index ));
extern void	compute_stats __(( void ));
extern void	vmnet_monitor __(( void ));
extern void	parse_operator_command __(( unsigned char *line, const int length ));
extern void	debug_rescan_queue __(( const char *UserName, const char opt ));
extern char	*StreamStateStr __(( const StreamStates state ));

/* nmr.c -shrunk*/
#ifdef	RSCS_VERSION /* These go together.. */
extern void	handle_NMR __(( struct NMR_MESSAGE *NmrRecord, const int size ));
#endif
extern void	send_nmr __(( const char *Faddress, const char *Taddress, void *text, int size, const MSGCODES format, const int Cflag ));
extern int	find_line_index __(( char *NodeName, char *LinkName, char *characterSet, int *primline, int *altline ));
extern void	handle_local_command __(( const char *OriginNode, const char *UserName, const char *MessageText ));
extern void	nmr_queue_msg __(( struct MESSAGE *Msg ));

/* bcb_crc.c -shrunk*/
extern int	check_crc __(( void *buffer, int *size ));
extern int	remove_dles __(( void *buffer, int *size ));
extern int	add_bcb_crc __(( const int Index, const void *buffer, const int size, void *NewLine, const int BCBcount ));
extern int	add_bcb __(( const int Index, const void *buffer, const int size, void *NewLine, const int BCBcount ));

/* unix_route.c -shrunk*/
extern int	open_route_file __(( void ));
extern void	close_route_file __(( void ));
extern int	get_route_record __(( const char *key, char *line, const int linesize ));
extern void	change_route __(( const char *Node, char *Route ));

/* util.c -shrunk*/
extern int	compare __(( const void *s1, const void *s2 )); /* Equivalent to for strcasecmp() ! */
extern int	parse_envelope __(( FILE *inpfil, struct FILE_PARAMS *FileParams, const int assign_oid ));
extern int	compress_scb __(( const void *InString, void *OutString, int InSize ));
extern int	uncompress_scb __(( const void *InString, void *OutString, int InSize, const int OutSize, int *SizeConsumed ));
extern void	ASCII_TO_EBCDIC __(( const void *InString, void *OutString, const int Size ));
extern void	EBCDIC_TO_ASCII __(( const void *InString, void *OutString, const int Size ));
extern void	PAD_BLANKS	__(( void *String, const int StartPos, const int FinalPos ));
extern int	despace __(( char *string, int len ));
extern int32	timevalsub __((struct timeval *result, struct timeval *a, struct timeval *b));
extern int32	timevaladd __((struct timeval *result, struct timeval *a, struct timeval *b));
extern int	get_send_fileid __(( void ));
extern int	get_user_fileid __(( const char *uname ));
extern void	set_user_fileid __(( const char *uname, const int spoolid ));
extern char	*MsecAgeStr __((TIMETYPE *timeptr, TIMETYPE *ageptr ));
extern char	*BytesPerSecStr __((long bytecount, TIMETYPE *deltatimep ));

/* protocol.c -shrunk */
extern void	restart_channel __(( const int Index ));
extern void	input_arrived __(( const int Index, const int status, void *buffer, const int size ));
extern void	handle_ack __(( const int Index, const int flag ));
extern void	income_signon __(( const int Index, struct LINE *temp, const unsigned char *buffer ));

/* send_file.c -shrunk */
extern void	send_file_buffer __(( const int Index ));
extern int	send_njh_dsh_record __(( const int Index, const int flag, const int SYSIN ));
extern void	send_njt __(( const int Index, const int SYSIN ));

/* recv_file.c *//*rev*/
extern int	receive_file __(( const int Index, const void *buffer, const int BufferSize ));
extern int	recv_file_open __(( const int Index, const int Stream ));

/* unix_brdcst.c -shrunk*/
extern int	send_user __(( const char *user, char *msg ));

/* unix_msgs.c */
extern int	read_message_config __(( const char *path ));
extern void	message_exit __(( const char *ToName, const char *ToNode, const char *FrName, const char *FrNode, const NMRCODES cmdmsgflg, char *MessageText ));
extern void	msg_register __(( const void *ptr, const int size ));
extern int	msg_helper __(( const char *FrAddr, const char *ToAddr ));

/* gone_server.c */
extern void	add_gone_user __(( const char *line ));
extern void	del_gone_user __(( const char *UserName ));
extern int	send_gone __(( const char *UserName, char *string ));
extern void	shut_gone_users __(( void ));
extern void	dump_gone_list __(( void ));
extern int	get_gone_user __(( const char *UserName, char **LoginDir ));

/* detach.c */
extern void	detach    __(( void ));

/* unix_files.c */
extern char    *CreateUniqueFileid __(( struct FILE_PARAMS *fp, const char *PathOrDir, const char *ToUser, const char *HomeDir ));
extern void	inform_filearrival __(( const char *path, struct FILE_PARAMS *FileParams, char *OutLine ));
extern int	read_exit_config __(( const char *path ));

/* rscsacct.c */
extern void	rscsacct_open __((const char *logfilename));
extern void	rscsacct_close __((void));
extern void	rscsacct_log __((const struct FILE_PARAMS *FP, const int xmit_or_recv));

/* lib{u,l}str.c */
extern char	*upperstr __((char *str));
extern char	*lowerstr __((char *str));

#endif
