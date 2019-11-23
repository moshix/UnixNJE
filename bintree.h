/* bintree.h -- see bintree.c */


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

#ifndef	__BINTREE_H_
# define __BINTREE_H_

#ifndef	__
# ifdef __STDC__
#  define __(x) x
# else
#  define __(x) ()
# endif
#endif

struct Bintree { void *dummy; };

extern struct Bintree *bintree_open __((const char *filename, const int eltsize, int (*eltcompare)()));
extern int	bintree_close __((struct Bintree *BT));
extern int	bintree_insert __((struct Bintree *BT, const void *datum));
extern int	bintree_delete __((struct Bintree *BT, const void *datum));
extern void    *bintree_find __((struct Bintree *BT, const void *datum));
extern int	bintree_update __((struct Bintree *BT, const void *datum));

#endif /* __BINTREE_H_ */
