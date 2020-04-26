/*============================ LOGGER =================================*/
/* Part of HUJI-NJE -- lifted from  main.c  */
/*
 | Write a logging line in our logfile. If the loglevel is 1, close the file
 | after writing, so we can look in it at any time.
 */

#include <stdio.h>
#include "consts.h"
#include "prototypes.h"
#include "ebcdic.h"
#include <stdarg.h>
#include <time.h>

extern int LogLevel;	/* In main() source module of each program */
extern FILE *LogFd;	/* In main() source module of each program */

/*============================ LOGGER =================================*/
/*
 | Write a logging line in our logfile. If the loglevel is 1, close the file
 | after writing, so we can look in it at any time.
 */
void
logger(int lvl, ...)
{
	char	*local_time();
	char *fmt;
	va_list pvar;

	va_start(pvar, lvl);
/*	lvl = va_arg(pvar,int); */
	va_end(pvar);
	
	/* Do we have to log it at all ? */
	if (lvl > LogLevel) {
	  return;
	}

	/* Open the log file */
	if (LogFd == 0) {	/* Not opened before */
	  if (strcmp(LOG_FILE,"-")==0)  LogFd = stderr;
	  else if ((LogFd = fopen(LOG_FILE, "a")) == NULL) {
	    LogFd = NULL;
	    return;
	  }
	}

	va_start(pvar, lvl);
/*	lvl = va_arg(pvar,int); */
	fmt = va_arg(pvar,char*);

	fprintf(LogFd, "%s, ", local_time());
	vfprintf(LogFd, fmt, pvar);
	va_end(pvar);
	fflush(LogFd);

	if (LogLevel == 1) {	/* Normal run - close file after loging */
	  if (LogFd != stdout)
	    fclose(LogFd);
	  LogFd = 0;
	}
}


/*
 | Return the time in a printable format; to be used by Bug-Check and Logger.
 */
char *
local_time()
{
	static	char	TimeBuff[80];
	struct	tm	*tm, *localtime();
	time_t	clock;

	time(&clock);		/* Get the current time */
	tm = localtime(&clock);
	sprintf(TimeBuff, "%04d-%02d-%02d %02d:%02d:%02d",
		tm->tm_year+1900, (tm->tm_mon + 1), tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
	return TimeBuff;
}


/*
 | Write a hex dump of the buffer passed. Do it only if the level associated
 | with it is lower or equal to the current one. After the dump write the
 | text of the message in ASCII. WE always try to convert from EBCDIC to
 | ASCII as most traces are done at the network level.
 */
#define ADD_TEXT {	/* Add the printable text */ \
	NextLinePosition = &line[count * 3]; \
	*NextLinePosition++ = ' '; \
	*NextLinePosition++ = '|'; \
	*NextLinePosition++ = ' '; \
	while(q <= (unsigned char *)p) { \
		c = EBCDIC_ASCII[*q++]; \
		if((c >= 32) && (c <= 126))	/* Printable */ \
			*NextLinePosition++ = c; \
		else \
			*NextLinePosition++ = '.'; \
	} \
	*NextLinePosition = '\0'; \
}

void
trace(p, n, lvl)
const void *p;
const int	n, lvl;
{
	register int	count, i;
	char	line[LINESIZE];
	unsigned char	c;	/* Point to the beginning of this buffer */
	char *line2;

	if (lvl > LogLevel) return;
	logger(lvl, "Trace called with data size=%d\n", n);

	count = 0;	/* save beginning of buffer */
	i = 0;
	while(i < n) {
	  for (count = 0; count < 12; ++count) {
	    if (count+i < n)
	      sprintf(line+count*3, "%02x ", *(((unsigned char *)p)+i+count));
	    else
	      strcpy(line+count*3, "   ");
	  }
	  strcat(line, " | ");
	  line2 = strlen(line)+line;
	  for (count = 0; count < 12; ++count) {
	    if (count+i < n) {
	      c = EBCDIC_ASCII[*(((unsigned char *)p)+i+count)];
	      if ((c >= 32) && (c <= 126))
		line2[count] = c;
	      else
		line2[count] = '.';
	    } else
	      line2[count] = 0;
	  }
	  line2[12] = 0;
	  logger(lvl, "%3d: %s\n", i, line);
	  i += 12;
	}
}

