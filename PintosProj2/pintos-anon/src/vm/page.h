#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "vm/swap.h"
#include <hash.h>
#include "filesys/off_t.h"

/**
 * Indicates a state of page.
 */
enum page_status {
  ALL_ZERO,         // All zeros
  ON_FRAME,         // Actively in memory
  ON_SWAP,          // Swapped (on swap slot)
  FROM_FILESYS      // from filesystem (or executable)
};

/**
 * Supplemental page table. The scope is per-process.
 */
struct supplemental_page_table
  {
    /* The hash table, page -> spte */
    struct hash page_map;
  };

struct supplemental_page_table_entry
  {
    void *upage;              /* Virtual address of the page (the key) */
    void *kpage;              /* Kernel page (frame) associated to it.
                                 Only effective when status == ON_FRAME.
                                 If the page is not on the frame, should be NULL. */
    struct hash_elem elem;

    enum page_status status;

    bool dirty;               /* Dirty bit. */

    // for ON_SWAP
    // swap_index_t swap_index;  /* Stores the swap index if the page is swapped out.
                                //  Only effective when status == ON_SWAP */

    // for FROM_FILESYS
    struct file *file;
    off_t file_offset;
    uint32_t read_bytes, zero_bytes;
    bool writable;
  };

#endif
