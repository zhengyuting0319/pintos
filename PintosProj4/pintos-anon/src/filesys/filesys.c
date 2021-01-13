#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
// #define DEBUG

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  buffer_cache_init ();
  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  buffer_cache_close ();
}


/* Creates a file or directory (set by `is_dir`) of
   full path `path` with the given `initial_size`.
   The path to file consists of two parts: path directory and filename.

   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *path, off_t initial_size, bool is_dir)
{
  #ifdef DEBUG
    printf("in file_create\n");
  #endif
  block_sector_t inode_sector = 0;

  struct dir *dir;
  char *file_name;
  parse_to_get_dir_filename(path, &dir, &file_name);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, file_name, inode_sector, is_dir));

  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}


void parse_to_get_dir_filename(const char *pathname, struct dir **dir, char **filename) {
  *dir = NULL;
  *filename = NULL;

  size_t len = strlen(pathname);
  char *s = malloc(len + 1);
  memcpy(s, pathname, len + 1);

  
  // extract filename
  char *last = s + len;
  *last = '\0';
  --last;    // should still >= path_buf
  while (last >= s && *last != '/')
      --last;
  ++last;

  // the length of filename should be positive
  size_t len_filename = strlen(last);
  *filename = malloc(len_filename + 1);

  memcpy(*filename, last, len_filename + 1);


  bool abs = (len > 0 && *s == '/');    // absolute path starts with '/'
  struct dir *curr;
  if (abs) {
    curr = dir_open_root();
  }
  else {
    struct thread *t = thread_current();
    if (t->cwd == NULL) // may happen for non-process threads (e.g. main)
      curr = dir_open_root();
    else {
      curr = dir_reopen( t->cwd );
    }
  }
  char *token, *p;
  for (token = strtok_r(s, "/", &p); 
       token != last && token != NULL;
       token = strtok_r(NULL, "/", &p))
  {
    struct inode *inode = NULL;
    if(! dir_lookup(curr, token, &inode)) {
      dir_close(curr); 
      free(s);
      return; // such directory not exist
    }
    struct dir *next = dir_open(inode);
    if(next == NULL) {
      dir_close(curr);
      free(s);
      return NULL;
    }
    dir_close(curr);
    curr = next;
  }
  free(s);

  /* check if the inode has been removed to pass dir-rm-cwd */
  if (inode_is_removed (dir_get_inode(curr))) {
    dir_close(curr);
    return NULL;
  }

  *dir = curr;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  int l = strlen(name);
  if (l == 0) return NULL;

  struct dir *dir;
  char *file_name;
  parse_to_get_dir_filename(name, &dir, &file_name);

  struct inode *inode = NULL;

  // removed directory handling
  #ifdef DEBUG
    printf("dir == NULL %d\n",dir == NULL);
  #endif
  if (dir == NULL) return NULL;
  #ifdef DEBUG
    printf("strlen(file_name) %d\n",strlen(file_name));
  #endif
  if (strlen(file_name) > 0) {
    dir_lookup (dir, file_name, &inode);
    dir_close (dir);
  }
  else { // empty filename : just return the directory
    inode = dir_get_inode (dir);
  }

  // removed file handling
  #ifdef DEBUG
    printf("inode == NULL %d\n",inode == NULL);
    printf("inode_is_removed (inode) %d\n", inode_is_removed (inode));
  #endif
  if (inode == NULL || inode_is_removed (inode))
    return NULL;

  return file_open (inode);
}
/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  struct dir *dir;
  char *file_name;
  parse_to_get_dir_filename(name, &dir, &file_name);

  bool success = (dir != NULL && dir_remove (dir, file_name));
  dir_close (dir);

  return success;
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/* Change CWD for the current thread. */
bool
filesys_chdir (const char *name)
{
  // add '/' to name so that I can utilize the function parse_to_get_dir_filename with file_name = ''
  int l = strlen(name);
  char *s = (char*) malloc( sizeof(char) * (l + 2) );
  memcpy (s, name, sizeof(char) * (l + 1));
  s[l] = '/';
  s[l+1] = '\0';
  struct dir *dir;
  char *file_name;
  parse_to_get_dir_filename(s, &dir, &file_name);

  if(dir == NULL) {
    return false;
  }

  // switch CWD
  dir_close (thread_current()->cwd);
  thread_current()->cwd = dir;
  return true;
}
