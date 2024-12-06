//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

#include "mm.h"
#include <stdlib.h>

/*
 *  MEMPHY_mv_csr - move MEMPHY cursor
 *  @mp: memphy struct
 *  @offset: offset
 */
int MEMPHY_mv_csr(struct memphy_struct *mp, int offset)
{
   int numstep = 0;
   // pthread_mutex_lock(&mp->lock);
   mp->cursor = 0;
   while(numstep < offset && numstep < mp->maxsz){
     /* Traverse sequentially */
     mp->cursor = (mp->cursor + 1) % mp->maxsz;
     numstep++;
   }
   // pthread_mutex_unlock(&mp->lock);

   return 0;
}

/*
 *  MEMPHY_seq_read - read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_seq_read(struct memphy_struct *mp, int addr, BYTE *value)
{
   if (mp == NULL)
     return -1;

   if (!mp->rdmflg)
     return -1; /* Not compatible mode for sequential read */

   MEMPHY_mv_csr(mp, addr);
   *value = (BYTE) mp->storage[addr];

   return 0;
}

/*
 *  MEMPHY_read read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_read(struct memphy_struct * mp, int addr, BYTE *value)
{
   if (mp == NULL)
     return -1;

   if (mp->rdmflg)
      *value = mp->storage[addr];
   else /* Sequential access device */
      return MEMPHY_seq_read(mp, addr, value);

   return 0;
}

/*
 *  MEMPHY_seq_write - write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_seq_write(struct memphy_struct * mp, int addr, BYTE value)
{

   if (mp == NULL)
     return -1;

   if (!mp->rdmflg)
     return -1; /* Not compatible mode for sequential read */

   MEMPHY_mv_csr(mp, addr);
   // pthread_mutex_lock(&mp->lock);
   mp->storage[addr] = value;
   // pthread_mutex_unlock(&mp->lock);

   return 0;
}

/*
 *  MEMPHY_write-write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_write(struct memphy_struct * mp, int addr, BYTE data)
{
   if (mp == NULL)
     return -1;

   if (mp->rdmflg)
   {
      // pthread_mutex_lock(&mp->lock);
      mp->storage[addr] = data;
      // pthread_mutex_unlock(&mp->lock);
   }
   else /* Sequential access device */
      return MEMPHY_seq_write(mp, addr, data);

   return 0;
}

/*
 *  MEMPHY_format-format MEMPHY device
 *  @mp: memphy struct
 */
int MEMPHY_format(struct memphy_struct *mp, int pagesz)
{
    /* This setting come with fixed constant PAGESZ */
    int numfp = mp->maxsz / pagesz;
    struct framephy_struct *newfst, *fst;
    int iter = 0;

    if (numfp <= 0)
      return -1;

    /* Init head of free framephy list */ 
    fst = malloc(sizeof(struct framephy_struct));
    fst->fpn = iter;
    mp->free_fp_list = fst;

    /* We have list with first element, fill in the rest num-1 element member*/
    for (iter = 1; iter < numfp ; iter++)
    {
       newfst =  malloc(sizeof(struct framephy_struct));
       newfst->fpn = iter;
       newfst->fp_next = NULL;
       fst->fp_next = newfst;
       fst = newfst;
    }

    return 0;
}

int MEMPHY_get_freefp(struct memphy_struct *mp, int *retfpn)
{
   struct framephy_struct *fp = mp->free_fp_list;
   // printf("MEMPHY_get_freefp\n");  //DEBUG
   if (fp == NULL)
   {
   //   printf("No free frame available\n");  //DEBUG
      return -1;
   }
   // pthread_mutex_lock(&mp->lock);
   *retfpn = fp->fpn;
   mp->free_fp_list = fp->fp_next;

   /* MEMPHY is iteratively used up until its exhausted
    * No garbage collector acting then it not been released
    */
   free(fp);
   // pthread_mutex_unlock(&mp->lock);

   return 0;
}

int MEMPHY_dump(struct memphy_struct * mp)
{
    /*TODO dump memphy contnt mp->storage 
     *     for tracing the memory content
     */
    for (int i = 0; i < mp -> maxsz; i++)
    {
      if (i % 32 == 0)
        printf("\n");
      printf("%d", mp -> storage[i]);
    }
      
    return 0;
}

int MEMPHY_put_freefp(struct memphy_struct *mp, int fpn)
{
   struct framephy_struct *fp = mp->free_fp_list;
   struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));

   /* Create new node with value fpn */
   // pthread_mutex_lock(&mp->lock);
   newnode->fpn = fpn;
   newnode->fp_next = fp;
   mp->free_fp_list = newnode;
   // pthread_mutex_unlock(&mp->lock);

   return 0;
}


/*
 *  Init MEMPHY struct
 */
int init_memphy(struct memphy_struct *mp, int max_size, int randomflg)
{
   mp->storage = (BYTE *)malloc(max_size*sizeof(BYTE));
   mp->maxsz = max_size;
   //pthread_mutex_init(&mp->lock, NULL);
   //pthread_mutex_lock(&mp->lock);
   MEMPHY_format(mp,PAGING_PAGESZ);

   mp->rdmflg = (randomflg != 0)?1:0;

   if (!mp->rdmflg )   /* Not Ramdom acess device, then it serial device*/
      mp->cursor = 0;
   //pthread_mutex_unlock(&mp->lock);
   return 0;
}

//#endif
