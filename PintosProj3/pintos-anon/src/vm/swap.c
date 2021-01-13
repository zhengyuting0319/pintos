#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "vm/swap.h"

#define DEBUG

/* A block device. */
struct block
  {
    struct list_elem list_elem;         /* Element in all_blocks. */

    char name[16];                      /* Block device name. */
    enum block_type type;                /* Type of block device. */
    block_sector_t size;                 /* Size in sectors. */

    const struct block_operations *ops;  /* Driver operations. */
    void *aux;                          /* Extra data owned by driver. */

    unsigned long long read_cnt;        /* Number of sectors read. */
    unsigned long long write_cnt;       /* Number of sectors written. */
  };

/* An ATA device. */
struct ata_disk
  {
    char name[8];               /* Name, e.g. "hda". */
    struct channel *channel;    /* Channel that disk is attached to. */
    int dev_no;                 /* Device 0 or 1 for master or slave. */
    bool is_ata;                /* Is device an ATA disk? */
  };

/* An ATA channel (aka controller).
   Each channel can control up to two disks. */
struct channel
  {
    char name[8];               /* Name, e.g. "ide0". */
    uint16_t reg_base;          /* Base I/O port. */
    uint8_t irq;                /* Interrupt in use. */

    struct lock lock;           /* Must acquire to access the controller. */
    bool expecting_interrupt;   /* True if an interrupt is expected, false if
                                   any interrupt would be spurious. */
    struct semaphore completion_wait;   /* Up'd by interrupt handler. */

    struct ata_disk devices[2];     /* The devices on this channel. */
  };

static struct block *swap_block;
static struct bitmap *swap_available;
static struct lock lock;

static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

// the number of possible (swapped) pages.
static size_t swap_size;

void
vm_swap_init ()
{
  ASSERT (SECTORS_PER_PAGE > 0); // 4096/512 = 8?
  lock_init (&lock);
  // Initialize the swap disk
  swap_block = block_get_role(BLOCK_SWAP);
  if(swap_block == NULL) {
    PANIC ("Error: Can't initialize swap block");
    NOT_REACHED ();
  }

  // Initialize swap_available, with all entry true
  // each single bit of `swap_available` corresponds to a block region,
  // which consists of contiguous [SECTORS_PER_PAGE] sectors,
  // their total size being equal to PGSIZE.
  swap_size = block_size(swap_block) / SECTORS_PER_PAGE;
  swap_available = bitmap_create(swap_size);
  bitmap_set_all(swap_available, true);
}


swap_index_t vm_swap_out (void *page)
{
  
  lock_acquire(&lock);
  // Ensure that the page is on user's virtual memory.
  ASSERT (page >= PHYS_BASE);

  // Find an available block region to use
  size_t swap_index = bitmap_scan (swap_available, /*start*/0, /*cnt*/1, true);

  struct channel* c = swap_block->aux;

  size_t i;
  for (i = 0; i < SECTORS_PER_PAGE; ++ i) {
    #ifdef DEBUG
    // printf("%p\n",swap_block);
    #endif
      if (lock_held_by_current_thread(&c->lock))
      {
        printf("in\n");
        lock_release(&c->lock);
      }
    block_write(swap_block,
        /* sector number */  swap_index * SECTORS_PER_PAGE + i,
        /* target address */ page + (BLOCK_SECTOR_SIZE * i)
        );
  }

  // occupy the slot: available becomes false
  bitmap_set(swap_available, swap_index, false);
  lock_release(&lock);

  return swap_index;
}


void vm_swap_in (swap_index_t swap_index, void *page)
{
  lock_acquire(&lock);

  // Ensure that the page is on user's virtual memory.
  ASSERT (page >= PHYS_BASE);

  // check the swap region
  ASSERT (swap_index < swap_size);
  if (bitmap_test(swap_available, swap_index) == true) {
    // still available slot, error
    PANIC ("Error, invalid read access to unassigned swap block");
  }

  size_t i;
  for (i = 0; i < SECTORS_PER_PAGE; ++ i) {
    block_read (swap_block,
        /* sector number */  swap_index * SECTORS_PER_PAGE + i,
        /* target address */ page + (BLOCK_SECTOR_SIZE * i)
        );
  }

  bitmap_set(swap_available, swap_index, true);
  lock_release(&lock);

}

void
vm_swap_free (swap_index_t swap_index)
{
  lock_acquire(&lock);
  // check the swap region
  ASSERT (swap_index < swap_size);
  if (bitmap_test(swap_available, swap_index) == true) {
    PANIC ("Error, invalid free request to unassigned swap block");
  }
  bitmap_set(swap_available, swap_index, true);
  lock_release(&lock);
}
