/* namesparse -- parse IBM VM/SP "NAMES DATA" entries.

   :<tag>.<info> <SP> :<tag>.<info> <SP> ..
   <SP> :<tag>.info...

   Feed this routine:
       FILE *namesfile;
       char *picktags[]; (NULL means EVERYTHING, "tag." !)
       char *selector; (A ":<tag>.<value>" which identifies records to accept,
                        a NULL means ANY.)

   returns:
       NULL  -- end of file, no data found
       char *results[]; (A NULL terminated array of pointers to strings)

   By Matti Aarnio <mea@nic.funet.fi>  14-Feb-1994 */

#include <stdio.h>
extern char *malloc();
extern char *realloc();
extern void  free();
extern char *strchr();

char *MEMFAIL[] = { ":failure.MALLOC FAILED",NULL };


char **
namesparse(namesfile,picktags,selector)
FILE *namesfile;
char *picktags[];
char *selector;
{
	int   linesize = 256;	/* Our advanced knowledge.. */
	char *linebuf = NULL;
	char *s, *s1, *s2;
	char **results = NULL;
	int   resultspc = 0;
	int   resultcnt = 0;
	int   first_char;
	int   pick_this;
	int   comp_len;
	int   select_len = 0;
	int   select_taglen = 0;
	int   selector_matched = (selector == NULL); /* No selector == match */
	int   pick_len;
	int   begin_found = 0; /* Found a line starting with ':' */

	linebuf = malloc(linesize);
	if (!linebuf) return NULL;

	if (selector) {
	  select_len = strlen(selector);
	  s1 = selector + 1;
	  while (*s1 != 0 && *s1 != '.') ++s1;
	  select_taglen = (s1 - selector);
	}

	while (1) {
	  /* Look for them... */
	  if (feof(namesfile) || ferror(namesfile)) {
	    free(linebuf);
	    return results;
	  }

	  /* First begin, or continuation of more ? */
	  first_char = getc(namesfile);
	  ungetc(first_char,namesfile);
	  if (first_char == ':' && begin_found) {
	    if (selector_matched && results != NULL)
	      return results;
	    /* Didn't match the select, but MAY have data in the results[],
	       scratch it.. */
	    if (results != NULL) {
	      while (resultcnt > 0)
		free(results[--resultcnt]);
	      results[0] = NULL;
	    }
	    resultcnt = 0;
	  }
	  if (first_char == ':')	/* Ok, found it */
	    begin_found = 1;

	  s = linebuf;
	  if (fgets(linebuf,linesize-1,namesfile) == NULL) {
	    free(linebuf);
	    return results;
	  }
	  while ((s = strchr(linebuf+linesize-256,'\n')) == NULL) {
	    char *newbuf;
	    if (feof(namesfile) || ferror(namesfile))
	      break;		/* EOF w/o newline at the end ? */
	    linesize += 256;
	    newbuf = realloc(linebuf,linesize);
	    if (!newbuf) {
	      free(linebuf);
	      return results;
	    }
	    linebuf = newbuf;
	    if (fgets(linebuf+linesize-256-1,256,namesfile) == NULL)
	      break;		/* EOF while scanning it in */
	  }
	  if (s) *s = 0;	/* We have valid 's', when we have the
				   terminating newline.  Zap the  newline. */

	  /* Ok, we have an input line, now get it split.. */
	  s = linebuf;
	  while (*s == ' ') ++s; /* Skip the initial blanks */

	  while (*s != 0) {
	    int tag_len;
	    char *t;
	    int prev_char;

	    s1 = s; /* This position SHOULD contain a colon.. we don't
		       check it, though.. */
	    prev_char = *s++;
	    /* Scan until next colon, or end of string */
	    while (*s != 0 && !(*s == ':' && prev_char == ' '))
	      prev_char = *s++;

	    /* Chop trailing spaces */
	    s2 = s-1;
	    while (s2 > s1 && *s2 == ' ') *s2-- = 0;

	    /* Total component length */
	    comp_len = (s2 - s1) +1;

	    /* Size of the tag in it;  ":<tag>.<data>" */
	    t = s1 + 1;
	    while (*t != ' ' && *t != 0 && *t != '.') ++t;
	    /*  :<tag>.  -> *s1 == ':', *t == '.' */
	    tag_len = (t - s1); /* INCLUDE the terminating "." ! */

	    if (selector != NULL &&
		tag_len == select_taglen && comp_len == select_len) {
	      if (strncmp(selector,s1,comp_len)==0)
		selector_matched = 1;
	    }
	    
	    if (picktags) { /* See if we take only some entries */
	      char **picks = picktags;
	      pick_this = 0;
	      while (*picks != NULL) {
		if (strncmp(s1+1,*picks,tag_len)==0) {
		  pick_this = 1;
		  break;
		}
		++picks;
	      }
	    } else
	      pick_this = 1;

	    /* If we pick it, put it into  "results" */
	    if (pick_this) {
	      if (results == NULL) {
		resultspc = 8;
		results = (char**)malloc(sizeof(char*)*(resultspc+1));
	      }
	      if (results == NULL) return MEMFAIL;
	      if (resultcnt >= resultspc) {
		resultspc += 8;
		results = (char**)realloc(results,sizeof(char*)*(resultspc+1));
	      }
	      if (results == NULL) return MEMFAIL;
	      results[resultcnt] = malloc(comp_len+1);
	      if (results[resultcnt] == NULL) return MEMFAIL;
	      memcpy(results[resultcnt],s1,comp_len+1);
	      ++resultcnt;
	      results[resultcnt] = NULL;
	    }
	  } /* .. while on line */
	} /* .. While (1) on record */
}


#ifdef	TEST
int
main(argc,argv)
int argc;
char *argv[];
{
	char *picklist[] =  {
	  "node.","internet.","servers1.","servers2.",NULL
	};
	char **results;
	int i;

	while (!feof(stdin) && !ferror(stdin)) {
	  results = namesparse(stdin,argc > 1 ? NULL : picklist,NULL);
	  if (results == NULL)
	    printf("RET=NULL\n");
	  else {
	    printf("%s\n",results[0]);
	    for (i=1; results[i] != NULL; ++i)
	      printf("\t%s\n",results[i]);
	    for (i=0; results[i] != NULL; ++i)
	      free(results[i]);
	    free(results);
	  }
	}
	return 0;
}
#endif
