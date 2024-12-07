//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

pthread_mutex_t ram_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t active_swp_lock = PTHREAD_MUTEX_INITIALIZER;
/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct* rg_elmt)
{
  int vmaid = rg_elmt -> vmaid;
  struct vm_area_struct *cur_vma = get_vma_by_num(mm, vmaid);
  struct vm_rg_struct *rg_node = cur_vma->vm_freerg_list;
  
  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  cur_vma->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma= mm->mmap; /* @pvma: pointer virtual memory area */

  if(mm->mmap == NULL)
    return NULL;

  int vmait = 0;
  
  while (vmait < vmaid)
  {
    if(pvma == NULL)
	  return NULL;

    vmait++;
    pvma = pvma->vm_next;
  }

  return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;
  if (mm->symrgtbl[rgid].vmaid == 0 && 
      mm->symrgtbl[rgid].rg_start >= mm->symrgtbl[rgid].rg_end)
    return NULL;
  if (mm->symrgtbl[rgid].vmaid == 1 && 
      mm->symrgtbl[rgid].rg_start <= mm->symrgtbl[rgid].rg_end)
    return NULL;
  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  // printf("allocating %d bytes\n", size); // DEBUG
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;

  /* TODO: commit the vmaid */
  rgnode.vmaid = vmaid;

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    caller->mm->symrgtbl[rgid].vmaid = rgnode.vmaid;

    *alloc_addr = rgnode.rg_start;
    return 0;
  }

  /* TODO: get_free_vmrg_area FAILED handle the region management (Fig.6)*/

  /* TODO retrive current vma if needed, current comment out due to compiler redundant warning*/
  /*Attempt to increate limit to get space */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);


  // int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  int inc_limit_ret;

  /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
  int old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT
   * inc_vma_limit(caller, vmaid, inc_sz)
   */
  if (inc_vma_limit(caller, vmaid, size, &inc_limit_ret) != 0)
  {
    return -1;
  }

  /* TODO: commit the limit increment */
  if (vmaid == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
    caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size -1;
    caller->mm->symrgtbl[rgid].vmaid = vmaid;
    if (size < inc_limit_ret)
    {
      /*add free list*/
      struct vm_rg_struct *newrg = init_vm_rg(old_sbrk + size, old_sbrk + inc_limit_ret - 1, vmaid);
      enlist_vm_freerg_list(caller->mm, newrg);
    }
  }
  else if (vmaid == 1)
  {
    caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
    caller->mm->symrgtbl[rgid].rg_end = old_sbrk - size + 1;
    caller->mm->symrgtbl[rgid].vmaid = vmaid;
    if (size < inc_limit_ret)
    {
      /*add free list*/     
      struct vm_rg_struct *newrg = init_vm_rg(old_sbrk - size, old_sbrk - inc_limit_ret + 1,vmaid);
      enlist_vm_freerg_list(caller->mm, newrg);
    }
  }
  /* TODO: commit the allocation address */
  *alloc_addr = caller->mm->symrgtbl[rgid].rg_start;

  return 0;
}
/* clear_fifo_pgn_no - clear pgn node when free memory
 *@pgnlist: list of pgn
 *@pgn: pgn to be removed
*/
int clear_fifo_pgn_node(struct pgn_t **pgnlist, int pgn)
{
  struct pgn_t *pgn_node = *pgnlist;
  struct pgn_t *prev_pgn_node = NULL;

  while (pgn_node != NULL)
  {
    if (pgn_node->pgn == pgn)
    {
      if (prev_pgn_node == NULL)
      {
        *pgnlist = pgn_node->pg_next;
      }
      else
      {
        prev_pgn_node->pg_next = pgn_node->pg_next;
      }
      free(pgn_node);
      return 0;
    }
    prev_pgn_node = pgn_node;
    pgn_node = pgn_node->pg_next;
  }
  return -1;
}
/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __free(struct pcb_t *caller, int rgid)
{
  struct vm_rg_struct* rgnode;
  // printf("freeing %d\n", rgid); //DEBUG
  // Dummy initialization for avoding compiler dummay warning
  // in incompleted TODO code rgnode will overwrite through implementing
  // the manipulation of rgid later
  // rgnode.vmaid = 0;  //dummy initialization
  // rgnode.vmaid = 1;  //dummy initialization

  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1;

  rgnode = get_symrg_byid(caller->mm, rgid);
  if (rgnode == NULL)
  {
    printf("Invalid memory region identify \n");
    return -1;
  }
    
  /* TODO: Manage the collect freed region to freerg_list */
  
  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, rgnode);
  printf("Data segment:\n");
  print_list_rg(caller->mm->mmap->vm_freerg_list);
  printf("Heap segment:\n");
  print_list_rg(caller->mm->mmap->vm_next->vm_freerg_list);
  // DEBUG
  // print_pgtbl(caller, 0, -1);
  return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;
  printf("-------PID: %d - Instruction: ALLOC %d %d -------\n", proc->pid, size, reg_index); //DEBUG
  /* By default using vmaid = 0 */
  if (__alloc(proc, 0, reg_index, size, &addr) != 0)
  {
    printf("Cannot allocate memory\n");
    return -1;
  }
  printf("Allocated at region %d - rg_start: %d - rg_end: %d \n", 
        reg_index, proc->mm->symrgtbl[reg_index].rg_start, 
        proc->mm->symrgtbl[reg_index].rg_end);
  printf("Data segment\n");
  print_pgtbl(proc, proc->mm->mmap->vm_start, proc->mm->mmap->sbrk); 
  print_list_rg(proc->mm->mmap->vm_freerg_list);
  printf("Heap segment\n");
  print_pgtbl(proc, proc->mm->mmap->vm_next->sbrk, proc->mm->mmap->vm_next->vm_start+1);
  print_list_rg(proc->mm->mmap->vm_next->vm_freerg_list);
  return 0;
}

/*pgmalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify vaiable in symbole table)
 */
int pgmalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;
  printf("-------PID: %d - Instruction: MALLOC %d %d ------- \n", proc->pid, size, reg_index); //DEBUG
  /* By default using vmaid = 1 */
  if (__alloc(proc, 1, reg_index, size, &addr) != 0)
  {
    printf("Cannot allocate memory\n");
    return -1;
  }
  printf("Allocated at region %d - rg_start:%d - rg_end:%d \n", 
        reg_index, proc->mm->symrgtbl[reg_index].rg_start, 
        proc->mm->symrgtbl[reg_index].rg_end);
  printf("Data segment\n");
  print_pgtbl(proc, proc->mm->mmap->vm_start, proc->mm->mmap->sbrk); 
  print_list_rg(proc->mm->mmap->vm_freerg_list);
  printf("Heap segment\n");
  print_pgtbl(proc, proc->mm->mmap->vm_next->sbrk, proc->mm->mmap->vm_next->vm_start+1);
  print_list_rg(proc->mm->mmap->vm_next->vm_freerg_list);
  return 0;
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
  printf("-------PID: %d - Instruction: FREE %d -------\n", proc->pid, reg_index); //DEBUG
  return __free(proc, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = mm->pgd[pgn];
  if (!PAGING_PTE_PAGE_PRESENT(pte))
  { /* Page is not online, make it actively living */
    int vicpgn; 
    int vicfpn;
    // printf("Page %d is not online\n", pgn); //DEBUG
    int tgtfpn = PAGING_PTE_SWP(pte);//the target frame storing our variable

    /* TODO: Play with your paging theory here */
    /* Find victim page */
    if (find_victim_page(caller->mm, &vicpgn) == -1)
      return -1;
    vicfpn = PAGING_PTE_FPN(caller->mm->pgd[vicpgn]);
    /* Do swap frame from MEMRAM to MEMSWP and vice versa*/
    /* Copy victim frame to swap */
    __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, tgtfpn, 1);
    /* Copy target frame from swap to mem */
    __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn, 0);
    /* Update page table */
    //pte_set_swap() &mm->pgd;
    pte_set_swap(&mm->pgd[vicpgn], 0, tgtfpn);
    /* Update its online status of the target page */
    //pte_set_fpn() & mm->pgd[pgn];
    pte_set_fpn(&caller->mm->pgd[pgn], vicfpn);

    enlist_pgn_node(&caller->mm->fifo_pgn,pgn);
  }

  *fpn = PAGING_PTE_FPN(mm->pgd[pgn]);

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  pthread_mutex_lock(&ram_lock);
  MEMPHY_read(caller->mram,phyaddr, data);
  pthread_mutex_unlock(&ram_lock);
  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  if (addr < 0 || addr >= caller -> vmemsz)
  {
    printf("Invalid address\n");
    return -1;
  }
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  pthread_mutex_lock(&ram_lock);
  MEMPHY_write(caller->mram,phyaddr, value);
  pthread_mutex_unlock(&ram_lock);
   return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __read(struct pcb_t *caller, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  if (currg == NULL)
  {
    printf("Invalid memory region identify\n");
    return -1;
  }
  int vmaid = currg->vmaid;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if(cur_vma == NULL) /* Invalid memory identify */
	{
    printf("Invalid memory area dentify\n");
    return -1;
  }

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}


/*pgwrite - PAGING-based read a region memory */
int pgread(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		uint32_t offset, // Source address = [source] + [offset]
		uint32_t destination) 
{
  BYTE data;
  printf("-------PID: %d - Instruction: READ %d %d %d -------\n", proc->pid, source, offset, destination); //DEBUG
  if (__read(proc, source, offset, &data) != 0)
    return -1;
  destination = (uint32_t) data;
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
    printf("Data segment\n");
    print_pgtbl(proc, proc->mm->mmap->vm_start, proc->mm->mmap->sbrk); 
    printf("Heap segment\n");
    print_pgtbl(proc, proc->mm->mmap->vm_next->sbrk, proc->mm->mmap->vm_next->vm_start+1);
#endif
  MEMPHY_dump(proc->mram);
#endif

  return 0;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __write(struct pcb_t *caller, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  if (currg == NULL)
  {
    printf("Invalid memory region identify\n");
    return -1;
  }
  int vmaid = currg->vmaid;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if(cur_vma == NULL) /* Invalid memory identify */
	{
    printf("Invalid memory area identify\n");
    return -1;
  }  
  pg_setval(caller->mm, currg->rg_start + offset, value, caller);
  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		uint32_t offset)
{
  printf("-------PID: %d - Instruction: WRITE %d %d %d -------\n", proc->pid, data, destination, offset); //DEBUG
  if (__write(proc, destination, offset, data) != 0){
    return -1;
  }
  #ifdef IODUMP
    printf("write region=%d offset=%d value=%d\n", destination, offset, data);
  #ifdef PAGETBL_DUMP
    printf("Data segment\n");
    print_pgtbl(proc, proc->mm->mmap->vm_start, proc->mm->mmap->sbrk); 
    printf("Heap segment\n");
    print_pgtbl(proc, proc->mm->mmap->vm_next->sbrk, proc->mm->mmap->vm_next->vm_start+1);
  #endif
    MEMPHY_dump(proc->mram);
  #endif
  return 0;

}


/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PTE_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);    
    }
  }

  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct* get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct * newrg;
  /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  newrg = malloc(sizeof(struct vm_rg_struct));

  /* TODO: update the newrg boundary */
  if (vmaid == 0)
  {
    newrg->rg_start = cur_vma->sbrk;
    newrg->rg_end = cur_vma->sbrk + alignedsz - 1;
  }
  else if (vmaid == 1)
  {
    newrg->rg_start = cur_vma->sbrk;
    newrg->rg_end = cur_vma->sbrk - alignedsz + 1;
  }
  else
  {
    newrg->rg_start = newrg->rg_end = -1;
  }

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma start
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  struct vm_area_struct *vma;

  /* TODO validate the planned memory area is not overlapped */
  if (vmaid == 0)
  {
    vma = get_vma_by_num(caller->mm, 1);
    if (OVERLAP(vmastart, vmaend, vma->sbrk, vma->vm_start) || vma == NULL)
    {
      return -1; /* Overlap and failed allocation */
    }   
  }
  else if (vmaid == 1)
  {
    vma = get_vma_by_num(caller->mm, 0);
    if (OVERLAP(vma->vm_start, vma->sbrk - 1, vmaend, vmastart) || vma == NULL)
    {
      return -1; /* Overlap and failed allocation */    
    }
  }

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size 
 *@inc_limit_ret: increment limit return
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz, int* inc_limit_ret)
{
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage =  inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  int old_end = cur_vma->vm_end;
  /*Validate overlap of obtained region */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
  {
    return -1; /*Overlap and failed allocation */
  }
    
  /* TODO: Obtain the new vm area based on vmaid */

  if (vm_map_ram(caller, vmaid ,area->rg_start, area->rg_end, 
                    old_end, incnumpage , newrg) < 0)
    return -1; /* Map the memory to MEMRAM */

  if (vmaid == 0)
  {
    cur_vma->sbrk += inc_amt;
  }
  else if (vmaid == 1)
  { 
    cur_vma->sbrk -= inc_amt;
  }
  else
  {
    return -1;
  }
  *inc_limit_ret = inc_amt;

  return 0;

}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn) 
{
  struct pgn_t *pg = mm->fifo_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if (pg == NULL)
    return -1;
  struct pgn_t * pre_pg;
  
  while (pg->pg_next != NULL)
  {
    pre_pg = pg;
    pg = pg->pg_next;
  }
  *retpgn = pg->pgn;
  pre_pg->pg_next = NULL;
  free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size 
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL && rgit->vmaid == vmaid)
  {
    if ((vmaid == 0 && rgit->rg_start + size - 1 <= rgit->rg_end) || 
        (vmaid == 1 && rgit->rg_start - size + 1 >= rgit->rg_end))
    { 
      /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = (vmaid == 0) ? rgit->rg_start + size - 1 : rgit->rg_start - size + 1;

      /* Update left space in chosen region */
      if ((vmaid == 0 && rgit->rg_start + size < rgit->rg_end) || 
          (vmaid == 1 && rgit->rg_start - size > rgit->rg_end))
      {
        rgit->rg_start = (vmaid == 0) ? rgit->rg_start + size : rgit->rg_start - size;
      }
      else
      { 
        /* Use up all space, remove current node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;
          rgit->rg_next = nextrg->rg_next;
          free(nextrg);
        }
        else
        { 
          /* End of free list */
          rgit->rg_start = rgit->rg_end; // dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      break;
    }
    rgit = rgit->rg_next; // Traverse next rg
  }

  if (newrg->rg_start == -1) // new region not found
    return -1;

  return 0;
}

//#endif
