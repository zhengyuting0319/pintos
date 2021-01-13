#include <debug.h>
#include <string.h>
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "filesys/cache.h"

// #define DEBUG

#define BUFFER_CACHE_SIZE 64

struct buffer_cache_entry_t {
  bool occupied;  // true only if this entry is valid cache entry

  block_sector_t disk_sector;
  uint8_t buffer[BLOCK_SECTOR_SIZE];

  bool dirty;     // dirty bit
  bool access;    // reference bit, for clock algorithm
};

/* Buffer cache entries. */
static struct buffer_cache_entry_t cache[BUFFER_CACHE_SIZE];

/* A global lock for synchronizing buffer cache operations. */
static struct lock buffer_cache_lock;

/* initialize buffer_cache */
void
buffer_cache_init (void)
{
  lock_init (&buffer_cache_lock);

  // initialize entries
  size_t i;
  for (i = 0; i < BUFFER_CACHE_SIZE; ++ i)
  {
    cache[i].occupied = false;
  }
}

void
buffer_cache_close (void)
{
  // flush buffer cache entries
  lock_acquire (&buffer_cache_lock);

  size_t i;
  for (i = 0; i < BUFFER_CACHE_SIZE; ++ i)
  {
    if (cache[i].occupied == false) continue;
    // buffer_cache_flush( &(cache[i]) );
    if (cache[i].dirty) {
      block_write (fs_device, cache[i].disk_sector, cache[i].buffer);
      cache[i].dirty = false;
    }
  }

  lock_release (&buffer_cache_lock);
}


/**
 * Lookup the cache entry, and returns the pointer of buffer_cache_entry_t,
 * or NULL in case of cache miss. (simply traverse the cache entries)
 */
static struct buffer_cache_entry_t*
buffer_cache_lookup (block_sector_t sector)
{
  #ifdef DEBUG
    printf("buffer_cache_lookup\n");
  #endif
  size_t i;
  for (i = 0; i < BUFFER_CACHE_SIZE; ++ i)
  {
    if (cache[i].occupied == false) continue;
    if (cache[i].disk_sector == sector) {
      // cache hit.
      #ifdef DEBUG
        printf("end buffer_cache_lookup\n");
      #endif
      return &(cache[i]);
    }
  }
  return NULL; // cache miss
}

/* Obtain a free cache entry slot.
   If there is an unoccupied slot already, return it.
   Otherwise, slot->occupied will be set to false by the clock algorithm. */
static struct buffer_cache_entry_t*
buffer_cache_evict (void)
{
  ASSERT (lock_held_by_current_thread(&buffer_cache_lock));

  // clock algorithm
  static size_t clock = 0;
  while (true) {
    if (cache[clock].occupied == false) {
      // found an empty slot -- use it
      return &(cache[clock]);
    }
    if (cache[clock].access) {
      // give a second chance
      cache[clock].access = false;
    }
    else {
      // evict 
      // lock_acquire (&buffer_cache_lock);
      struct buffer_cache_entry_t *slot = &cache[clock];
      if (slot->dirty) {
        // write back into disk
        block_write (fs_device, slot->disk_sector, slot->buffer);
        slot->dirty = false;
      }
      slot->occupied = false;
      // lock_release (&buffer_cache_lock);
      return slot;
    }
    // loop
    clock ++;
    clock %= BUFFER_CACHE_SIZE;
  }
  return NULL;
}

/* read sector and store on buffer, and copy it to target */
void
buffer_cache_read (block_sector_t sector, void *target)
{
  lock_acquire (&buffer_cache_lock);

  struct buffer_cache_entry_t *slot = buffer_cache_lookup (sector);
  if (slot == NULL) {
    // cache miss: need eviction.
    slot = buffer_cache_evict ();
    ASSERT (slot != NULL && slot->occupied == false);

    // fill in the cache entry.
    slot->occupied = true;
    slot->disk_sector = sector;
    slot->dirty = false;
    block_read (fs_device, sector, slot->buffer);
  }

  // copy the buffer data into memory.
  slot->access = true;
  memcpy (target, slot->buffer, BLOCK_SECTOR_SIZE);

  lock_release (&buffer_cache_lock);
}

/* read sector and write source to cache_buffer */
void
buffer_cache_write (block_sector_t sector, const void *source)
{
  #ifdef DEBUG
    printf("in buffer_cache_write\n");
  #endif
  lock_acquire (&buffer_cache_lock);

  struct buffer_cache_entry_t *slot = buffer_cache_lookup (sector);
  #ifdef DEBUG
    printf("tem3\n");
  #endif
  if (slot == NULL) {
    // cache miss: need eviction.
    slot = buffer_cache_evict ();
    // #ifdef DEBUG
    //   printf("tem3\n");
    // #endif
    ASSERT (slot != NULL && slot->occupied == false);

    // fill in the cache entry.
    slot->occupied = true;
    slot->disk_sector = sector;
    slot->dirty = false;
  }

  // copy the data form memory into the buffer cache.
  slot->access = true;
  slot->dirty = true;
  memcpy (slot->buffer, source, BLOCK_SECTOR_SIZE);

  lock_release (&buffer_cache_lock);
  #ifdef DEBUG
    printf("end buffer_cache_write\n");
  #endif
}
