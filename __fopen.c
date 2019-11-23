#include <stdio.h>
#include <errno.h>

#ifndef	DEBUG_FOPEN

FILE *_nje_fopen(filename, type)
     char *filename, *type;
{
	return fopen(filename, type);
}

FILE *_nje_freopen(filename,type,stream)
     char *filename, *type;
     FILE *stream;
{
	return freopen(filename,type,stream);
}

FILE *_nje_fdopen(fd,type)
     int fd;
     char *type;
{
	return fdopen(fd,type);
}

int _nje_fclose(stream)
     FILE *stream;
{
	return fclose(stream);
}


#else


static FILE *fileptrs[512];
static int topfp  = 0;
int calls_fopen   = 0;
int calls_fdopen  = 0;
int calls_freopen = 0;
int calls_fclose  = 0;
static int initialized = 0;
static FILE *fopenlog = NULL;

extern int errno;

static void fopen_init()
{
  fopenlog = stderr;
  initialized = 1;
}

static void fopenerr(name,msg,dumpfps,oerrno)
char *name, *msg;
int dumpfps, oerrno;
{
	extern char *sys_errlist[];

	if (!fopenlog) abort();

	fprintf(fopenlog,"_nje_%s() : %s",name,msg);

	if (oerrno != 0)
	  fprintf(fopenlog,", errno=%d (%s)",oerrno,sys_errlist[oerrno]);

	if (dumpfps) {
	  int i;
	  fprintf(fopenlog," fps in use:");
	  for (i = 0; i < topfp; ++i)
	    if (fileptrs[i] != NULL)
	      fprintf(fopenlog," %d",i);
	}
	fprintf(fopenlog,"\n");
}


FILE *_nje_fopen(filename, type)
     char *filename, *type;
{
	FILE *fp = fopen(filename, type);
	int i;
	int oerrno = errno;

	if (!initialized) fopen_init();
	++calls_fopen;

	if (fp) {
	  int freefp = -1;
	  for (i=0; i < topfp; ++i)
	    if (freefp < 0 && fileptrs[i] == NULL) freefp = i;
	    if (fileptrs[i] == fp) {
	      fopenerr("fopen","reissued fp",0,0);
	      return fp;
	    }
	  if (freefp < 0) freefp = topfp++;
	  fileptrs[freefp] = fp;
	} else
	  fopenerr("fopen", "failed", 1, oerrno);

	errno = oerrno;
	return fp;
}

FILE *_nje_freopen(filename,type,stream)
     char *filename, *type;
     FILE *stream;
{
	FILE *fp = freopen(filename, type, stream);
	int i;
	int oerrno = errno;

	if (!initialized) fopen_init();
	++calls_freopen;

	if (fp) {
	  int freefp = -1;
	  for (i=0; i < topfp; ++i)
	    if (freefp < 0 && fileptrs[i] == NULL) freefp = i;
	    if (fileptrs[i] == fp) {
	      fopenerr("freopen","reissued fp",0,0);
	      return fp;
	    }
	  if (freefp < 0) freefp = topfp++;
	  fileptrs[freefp] = fp;
	} else
	  fopenerr("freopen", "failed", 1, oerrno);

	errno = oerrno;
	return fp;
}

FILE *_nje_fdopen(fd,type)
     int fd;
     char *type;
{
	FILE *fp = fdopen(fd,type);
	int i;
	int oerrno = errno;

	if (!initialized) fopen_init();
	++calls_fdopen;

	if (fp) {
	  int freefp = -1;
	  for (i=0; i < topfp; ++i)
	    if (freefp < 0 && fileptrs[i] == NULL) freefp = i;
	    if (fileptrs[i] == fp) {
	      fopenerr("fdopen","reissued fp",0,0);
	      return fp;
	    }
	  if (freefp < 0) freefp = topfp++;
	  fileptrs[freefp] = fp;
	} else
	  fopenerr("fdopen", "failed", 1, oerrno);

	errno = oerrno;
	return fp;
}

int _nje_fclose(stream)
     FILE *stream;
{
	int i;

	if (!initialized) fopen_init();
	++calls_fclose;

	if (stream)
	  for (i = 0; i < topfp; ++i)
	    if (stream == fileptrs[i]) {
	      fileptrs[i] = NULL;
	      return fclose(stream);
	    }

	fopenerr("fclose","unknown stream", 1, 0);

	return fclose(stream);
}



#endif
