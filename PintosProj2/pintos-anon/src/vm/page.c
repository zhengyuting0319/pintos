#include <hash.h>
#include <string.h>
#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"

// #define DEBUG


// Hash Functions required for [frame_map]. Uses 'kaddr' as key.
static unsigned
spte_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct supplemental_page_table_entry *entry = hash_entry(elem, struct supplemental_page_table_entry, elem);
  return hash_int( (int)entry->upage );
}
static bool
spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct supplemental_page_table_entry *a_entry = hash_entry(a, struct supplemental_page_table_entry, elem);
  struct supplemental_page_table_entry *b_entry = hash_entry(b, struct supplemental_page_table_entry, elem);
  return a_entry->upage < b_entry->upage;
}

struct supplemental_page_table*
init_pt (struct hash page_table)
{
  hash_init (&page_table, spte_hash_func, spte_less_func, NULL);
}


/**
 * Install a page (specified by the starting address `upage`) which
 * is currently on the frame, in the supplemental page table.
 *
 * Returns true if successful, false otherwise.
 * (In case of failure, a proper handling is required later -- process.c)
 */
bool
vm_supt_install_frame (struct supplemental_page_table *supt, void *upage, void *kpage)
{
  struct supplemental_page_table_entry *spte;
  spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));

  spte->upage = upage;
  spte->kpage = kpage;
  spte->status = ON_FRAME;
  spte->dirty = false;
//   spte->swap_index = -1;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&supt->page_map, &spte->elem);
  if (prev_elem == NULL) {
    // successfully inserted into the supplemental page table.
    return true;
  }
  else {
    // failed. there is already an entry.
    free (spte);
    return false;
  }
}


/**
 * Install new a page (specified by the starting address `upage`)
 * on the supplemental page table. The page is of type ALL_ZERO,
 * indicates that all the bytes is (lazily) zero.
 */
bool
vm_supt_install_zeropage (struct supplemental_page_table *supt, void *upage)
{
      #ifdef DEBUG
      printf("vm_supt_install_zeropage\n");
      #endif
  struct supplemental_page_table_entry *spte;
  spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));

  spte->upage = upage;
  spte->kpage = NULL;
  spte->status = ALL_ZERO;
  spte->dirty = false;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&supt->page_map, &spte->elem);
  if (prev_elem == NULL) return true;

  // there is already an entry -- impossible state
  PANIC("Duplicated SUPT entry for zeropage");
  return false;
}



/**
 * Lookup the SUPT and find a SPTE object given the user page address.
 * returns NULL if no such entry is found.
 */
struct supplemental_page_table_entry*
vm_supt_lookup (struct supplemental_page_table *supt, void *page)
{
  // create a temporary object, just for looking up the hash table.
  struct supplemental_page_table_entry spte_temp;
  spte_temp.upage = page;

  struct hash_elem *elem = hash_find (&supt->page_map, &spte_temp.elem);
  if(elem == NULL) return NULL;
  return hash_entry(elem, struct supplemental_page_table_entry, elem);
}


/**
 * Returns if the SUPT contains an SPTE entry given the user page address.
 */
bool
vm_supt_has_entry (struct supplemental_page_table *supt, void *page)
{
  #ifdef DEBUG
      printf("vm_supt_has_entry\n");
  #endif
  /* Find the SUPT entry. If not found, it is an unmanaged page. */
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);
  if(spte == NULL) return false;

  return true;
}


/**
 * Load the page, specified by the address `upage`, back into the memory.
 */
bool
vm_load_page(struct supplemental_page_table *supt, uint32_t *pagedir, void *upage)
{
  /* see also userprog/exception.c */
  #ifdef DEBUG
    printf("in vm_load_page! \n");
  #endif

  // 1. Check if the memory reference is valid
  struct supplemental_page_table_entry *spte;
  spte = vm_supt_lookup(supt, upage);
  if(spte == NULL) {
    return false;
  }

  if(spte->status == ON_FRAME) {
    // already loaded
    return true;
  }

  // 2. Obtain a frame to store the page
  void *frame_page = vm_frame_allocate(PAL_USER, upage);
  if(frame_page == NULL) {
    return false;
  }

  // 3. Fetch the data into the frame
  bool writable = true;
  switch (spte->status)
  {
  case ALL_ZERO:
  #ifdef DEBUG
  printf("ALL Zero!\n");
  #endif
    memset (frame_page, 0, PGSIZE);
    break;

//   case ON_FRAME:
//     /* nothing to do */
//     break;

// //   case ON_SWAP:
// //     // Swap in: load the data from the swap disc
// //     vm_swap_in (spte->swap_index, frame_page);
// //     break;

//   case FROM_FILESYS:
//     if( vm_load_page_from_filesys(spte, frame_page) == false) {
//       vm_frame_free(frame_page);
//       return false;
//     }

//     writable = spte->writable;
//     break;

  default:
    PANIC ("unreachable state");
  }

  // 4. Point the page table entry for the faulting virtual address to the physical page.
  if(!pagedir_set_page (pagedir, upage, frame_page, writable)) {
    vm_frame_free(frame_page);
    return false;
  }

//   // Make SURE to mapped kpage is stored in the SPTE.
//   spte->kpage = frame_page;
//   spte->status = ON_FRAME;

//   pagedir_set_dirty (pagedir, frame_page, false);

//   // unpin frame
//   vm_frame_unpin(frame_page);

  return true;
}