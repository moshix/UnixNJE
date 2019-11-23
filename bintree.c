/* Balanced-Bin-Tree -- an in-core database structure containing
   an as-much-as-possible balanced binary tree, WITH AN ONLINE
   DISK COPY OF ITS CONTENTS.

   This is ok for databases that are
     - fairly small
     - built once (or rarely changed in mid-life)
     - are usually read-only
     - need to survive application program restart
       (but might not survive a bug/crash during database update
        to the disk!)

   This builds up an ARRAY of elements, which has a backing-store.
   Within that array all elements are in order, and re-inserting same
   key just adds the other one in.  However FINDING all of duplicate
   entries isn't easy..  Suggestion, NEVER insert duplicates.

   Needed storage (and disk space) amounts   eltsize * n

   Used "datum"s must contain everything -- key, and data in one parcel.
   (Pointers to external storage from within the datum have no meaning on
    disk copy of the element.  Of course, if they don't need either, it is
    up to the users to do as they wish..)

   A "eltcompare" -routine pointer to be provided at  bintree_open()
   knows the internals of the data elements, as the user does too.

   Execution complexity:
     - Open    O(n)  (More like O(1) - just malloc(), and disk-read)
     - Insert  O(n)
     - Delete  O(n)
     - Find    O(log n) (no disk-io)
     - post-find update O(1)


   by Matti Aarnio  <mea@nic.funet.fi> on  15-16 Feb 1994
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#ifndef __STDC__
#define const
#define void char
#endif

extern void *malloc();
extern void *realloc();
extern void free();
extern void *memset();
extern char *strrchr();

struct Bintree {
	FILE *treefile;
	int   last;		/* number of elements in store		*/
	int   heapsize;		/* Allocated size of the system		*/
	int   eltsize;		/* Single element size			*/
	int   need_rebalance;	/* Flag a need for rebalance		*/
	int (*eltcompare)();	/* Routine doing the compare		*/
	void *datastore;	/* Arbitary size data storage		*/
	void *tmpstore;		/* Single entry store for swaps		*/
};
struct BTheader {
	int	last;
	int	heapsize;
	int	eltsize;
	int	need_rebalance;
#define BTHSIGN 0x12345678
	int	bytesex_signature; /* 0x12345678 - for checkups */
	char	dbname[16];	   /* Name of the DB - for checkups */
};

#define BT_ENTRY(index) (void*)(((char*)(BT->datastore))+((index)*BT->eltsize))
#define BT_ENTRYO(offset) (void*)(((char*)(BT->datastore))+(offset))


/* Routines:

   -- Open by telling filename, element size (checked, if data
      exists, and compare routine returning <0 / 0 / >0

   struct Bintree *bintree_open(char *filename, short eltsize,
   				  int (*eltcompare)());
   -- The usual thing..

   int bintree_close(struct Bintree *BT);


   -- Feed in the new entry

   int bintree_insert(struct Bintree *BT, void *datum);

   -- Find an entry, and delete it

   int bintree_delete(struct Bintree *BT, void *datum);


   -- Find an entry, and return pointer to it

   void *bintree_find(struct Bintree *BT, void *datum);

   -- With found pointer, had the data updated, and now
      tell the system to update it also on the disk

   int bintree_update(struct Bintree *BT, void *datumptr);

 */

struct Bintree *
bintree_open(filename,eltsize,eltcompare)
const char *filename;
const short eltsize;
int (*eltcompare)();
{
	struct Bintree *BT = NULL;
	struct BTheader BTH;
	struct stat statbuf;
	int fd = open(filename,O_RDWR|O_CREAT,0644);
	char *db_basename = strrchr(filename,'/');

	if (db_basename == NULL)
	  db_basename = filename;
	else
	  ++db_basename;

	if (fd < 0) return NULL;

	BT = (struct Bintree *)malloc(sizeof (struct Bintree));
	if (!BT) return NULL; /* malloc() problem */

	BT->treefile = fdopen(fd,"r+"); /* This should not fail.. */
	if (BT->treefile == NULL) {
	  free(BT);
	  return NULL;
	}
	fstat(fd,&statbuf); /* File is open, query its data */
	
	if (statbuf.st_size == 0) { /* Fresh baby :) */
	  BT->last     = 0;
	  BT->heapsize = 8;
	  BT->eltsize  = eltsize;
	  BT->need_rebalance = 0;

	  BT->datastore = malloc(BT->heapsize * BT->eltsize);
	  if (BT->datastore == NULL) {
	    fclose(BT->treefile);
	    free(BT);
	    return NULL;
	  }
	  BTH.last           = BT->last;
	  BTH.heapsize       = BT->heapsize;
	  BTH.eltsize        = BT->eltsize;
	  BTH.need_rebalance = BT->need_rebalance;
	  BTH.bytesex_signature = BTHSIGN;
	  strncpy(BTH.dbname,db_basename,sizeof(BTH.dbname));

	  fseek(BT->treefile,0,0);
	  fwrite(&BTH,sizeof(BTH),1,BT->treefile);

	  memset(BT->datastore,0, BT->heapsize * BT->eltsize);
	  fwrite(BT->datastore, BT->heapsize * BT->eltsize, 1, BT->treefile);
	  fflush(BT->treefile);

	} else {
	  fread(&BTH,sizeof(BTH),1,BT->treefile);
	  BT->last     = BTH.last;
	  BT->heapsize = BTH.heapsize;
	  BT->eltsize  = BTH.eltsize;
	  /* XX:  Check 'need_rebalance'!  */

	  if (BT->eltsize != eltsize ||
	      strncmp(BTH.dbname,db_basename,sizeof(BTH.dbname)) != 0 ||
	      BTH.bytesex_signature != BTHSIGN) {
	    /* Mismatch! */
	    fclose(BT->treefile);
	    free(BT);
	    return NULL;
	  }

	  BT->datastore = malloc(BT->heapsize * BT->eltsize);
	  if (BT->datastore == NULL) {
	    fclose(BT->treefile);
	    free(BT);
	    return NULL;
	  }
	  fread(BT->datastore, BT->heapsize * BT->eltsize, 1, BT->treefile);
	}
	BT->eltcompare = eltcompare; /* Comparison routine */
	BT->tmpstore = malloc(BT->eltsize);
	/* XX: malloc() failure check! */
	return BT;
}


int
bintree_close(BT)
struct Bintree *BT;
{
	int rc;

	rc = fclose(BT->treefile);
	free(BT->datastore);
	free(BT->tmpstore);
	free(BT);
	return rc;
}


/* Find an index to the data */
static int
__bintree_find(BT, datum, nearby)
struct Bintree *BT;
void *datum;
const int nearby;	/* Accept any result, we need to find pivot point */
{
	int lo, hi, mid, rc = 0;

	/* We do the search by a BINARY search */
	/* The elements get into the array in ascending
	   order per used comparison routine */

	lo = 0; hi = BT->last-1;
	mid = lo;
	while (lo <= hi) {
	  mid = (lo + hi) >> 1;

	  rc = (BT->eltcompare)(datum,BT_ENTRY(mid));
	  if (rc == 0)
	    return mid;

	  if (rc < 0)
	    hi = mid-1;
	  else
	    lo = mid+1;
	}
	if (nearby)
	  if (rc > 0)
	    return mid+1;
	  else
	    return mid;
	return -1; /* Not found */
}

/* Feed in the new entry */
int
bintree_insert(BT, datum)
struct Bintree *BT;
void *datum;
{
	int rc = 0;
	struct BTheader BTH;
	int inspt;

	BT->last += 1;
	if (BT->last >= (BT->heapsize -1)) { /* It must grow at first */
	  int   newsize = BT->heapsize + 8;
	  void *newdata = realloc(BT->datastore, newsize * BT->eltsize);
	  if (newdata == NULL)
	    return -1;
	  BT->datastore = newdata;
	  BT->heapsize  = newsize;
	}

	if (BT->last > 1) {
	  int ins0;
	  /* Ok, one or more exist already */
	  inspt = __bintree_find(BT,datum,1);
	  /* It may go one up at the final compare.. */
	  if (inspt >= BT->last) inspt = BT->last-1;
	  /* Move rest of them up.. */
	  for (ins0 = BT->last-1; ins0 > inspt; --ins0)
	    memcpy(BT_ENTRY(ins0), BT_ENTRY(ins0-1), BT->eltsize);
	} else
	  /* The first one.. */
	  inspt = 0;
	memcpy(BT_ENTRY(inspt), datum, BT->eltsize);

	fseek(BT->treefile, sizeof(struct BTheader)+(inspt * BT->eltsize), 0);
	fwrite(BT_ENTRY(inspt), BT->eltsize, BT->last-1-inspt+1, BT->treefile);

	fseek(BT->treefile,0,0);
	fread(&BTH,sizeof(BTH),1,BT->treefile);

	BTH.last           = BT->last;
	BTH.heapsize       = BT->heapsize;
	BTH.eltsize        = BT->eltsize;
	BTH.need_rebalance = BT->need_rebalance;
	BTH.bytesex_signature = BTHSIGN;

	fseek(BT->treefile,0,0);
	if (fwrite(&BTH, sizeof(BTH), 1, BT->treefile) != 1)
	  rc = -1;
	if (fflush(BT->treefile) != 0)
	  rc = -1;

	return rc;
}


/* Find an entry, and delete it */
int
bintree_delete(BT, datum)
struct Bintree *BT;
void *datum;
{
	int index, count;
	char *ptr;
	long offset;
	int rc = 0;
	struct BTheader BTH;

	index = __bintree_find(BT,datum,0);
	if (index < 0) return -1; /* Not found */

	offset = BT->eltsize * index;
	ptr    = BT_ENTRYO(offset);
	count  = BT->last-1 - index; /* NOT +1 ! */

	memcpy(ptr,ptr + BT->eltsize, count*BT->eltsize);
	BT->last -= 1;

	fseek(BT->treefile,0,0);
	fread(&BTH,sizeof(BTH),1,BT->treefile);

	BTH.last           = BT->last;
	BTH.heapsize       = BT->heapsize;
	BTH.eltsize        = BT->eltsize;
	BTH.need_rebalance = BT->need_rebalance;
	BTH.bytesex_signature = BTHSIGN;

	fseek(BT->treefile, 0, 0);
	fwrite(&BTH, sizeof(BTH), 1, BT->treefile);

	fseek(BT->treefile, sizeof(struct BTheader)+offset, 0);
	fwrite(ptr, BT->eltsize, count, BT->treefile);
	fflush(BT->treefile);

	return rc;
}

/* Find an entry, and return pointer to it */
void *
bintree_find(BT, datum)
struct Bintree *BT;
void *datum;
{
	int index = __bintree_find(BT,datum,0);

	if (index < 0) return NULL; /* Not found */

	return BT_ENTRY(index);
}

/*  With found pointer, had the data updated, and now
    tell the system to update it also on the disk */
int
bintree_update(BT, datumptr)
struct Bintree *BT;
void *datumptr;
{
	long offset = sizeof(struct BTheader) + ((char*)datumptr - (char*)(BT->datastore));
	int rc = 0;

	fseek(BT->treefile,offset,0);
	if (fwrite(datumptr,BT->eltsize,1,BT->treefile) != 1)
	  rc = -1;
	if (fflush(BT->treefile) != 0)
	  rc = -1;
	return rc;
}
