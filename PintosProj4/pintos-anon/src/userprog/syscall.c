#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"
#include "userprog/pagedir.h"

#ifdef VM
#include "vm/page.h"
#endif

// #define FILESYS
// #define FILESYSDebug


// #define DEBUG
// #define DEBUG2
struct lock sys_lock;
static uint32_t *esp;
static int fd_current = 3;

static void syscall_handler (struct intr_frame *);

void halt(void);
void exit(int status);
pid_t exec(const char* cmd_line);
int wait(pid_t pid);
bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int open(const char* file);
int filesize(int fd);
int read(int fd, void* buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
enum fd_search_filter { FD_FILE = 1, FD_DIRECTORY = 2 };


#ifdef VM
mmapid_t mmap(int fd, void *);
bool munmap(mmapid_t);

static struct mmap_desc* find_mmap_desc(struct thread *, mmapid_t fd);

void preload_and_pin_pages(const void *, size_t);
void unpin_preloaded_pages(const void *, size_t);
#endif

#ifdef FILESYS
bool chdir(const char *filename);
bool mkdir(const char *filename);
bool readdir(int fd, char *filename);
bool isdir(int fd);
int inumber(int fd);
#endif

// open file function
struct file_descriptor *
get_open_file (int fd)
{
  struct list_elem *e;
  struct file_descriptor *fd_struct; 
  e = list_tail (&open_files);
  while ((e = list_prev (e)) != list_head (&open_files)) 
    {
      fd_struct = list_entry (e, struct file_descriptor, elem);
      if (fd_struct->fd_num == fd)
	return fd_struct;
    }
  return NULL;
}



void
close_open_file (int fd)
{
  struct list_elem *e;
  struct list_elem *prev;
  struct file_descriptor *fd_struct; 
  e = list_end (&open_files);
  while (e != list_head (&open_files)) 
  {
    prev = list_prev (e);
    fd_struct = list_entry (e, struct file_descriptor, elem);
    if (fd_struct->fd_num == fd)
    {
      list_remove (e);
            file_close (fd_struct->file_struct);
      free (fd_struct);
      return ;
    }
    e = prev;
  }
}



//if the program cannot load or run for any reason
bool 
is_good_ptr(const void *usr_ptr)
{
  // printf("in is_good_ptr \n");
  struct thread *cur = thread_current ();
  if (usr_ptr != NULL && is_user_vaddr (usr_ptr))
    {
      // printf("%d\n",(pagedir_get_page (cur->pagedir, usr_ptr)) != NULL);
      return (pagedir_get_page (cur->pagedir, usr_ptr)) != NULL;
    }
  // printf("0\n");
  return false;
}

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
  list_init (&open_files);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


static int32_t
get_user (const uint8_t *uaddr) {
  // check that a user pointer `uaddr` points below PHYS_BASE
  if (! ((void*)uaddr < PHYS_BASE)) {
    return -1;
  }

  // as suggested in the reference manual, see (3.1.5)
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
  return result;
}

// in case of invalid memory access, fail and exit.
static void fail_invalid_access(void) {
  if (lock_held_by_current_thread(&filesys_lock))
    lock_release (&filesys_lock);

  exit (-1);
  NOT_REACHED();
}

static int
memread_user (void *src, void *dst, size_t bytes)
{
  int32_t value;
  size_t i;
  for(i=0; i<bytes; i++) {
    value = get_user(src + i);
    if(value == -1) // segfault or invalid memory access
      fail_invalid_access();

    *(char*)(dst + i) = value & 0xff;
  }
  return (int)bytes;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  esp = f->esp;
  thread_current()->current_esp = f->esp;
  #ifdef DEBUG
  printf ("system call!\n");
  #endif
  if (!is_good_ptr (esp) || !is_good_ptr (esp + 1) ||
    !is_good_ptr (esp + 2) || !is_good_ptr (esp + 3))
  {
    exit (-1);
  }
  else
  {
    int syscall_number = *(int *)(esp);
    #ifdef FILESYSDebug
      printf("sys num is %d \n",syscall_number);
    #endif
    switch (syscall_number)
    {
      case SYS_HALT:  {// 0
        halt();
        break;    
      }              
      case SYS_EXIT: {// 1
        // printf("exit num %d\n",(int)(*(esp + 1)));
        exit (*(esp + 1));
        break;     
      }        
      case SYS_EXEC: {// 2
        // printf("exec char %s\n",(char *) *(esp + 1));
        f->eax = exec ((char *) *(esp + 1));
        break;    
      }                   
      case SYS_WAIT: {// 3
        f->eax = wait (*(esp + 1));
        break;    
      }                    
      case SYS_CREATE: {// 4
        // printf("create num %s\n",(char *) *(esp + 1));
        f->eax = create ((char *) *(esp + 1), *(esp + 2));
        break;    
      }                    
      case SYS_REMOVE: {// 5
        f->eax = remove ((char *) *(esp + 1));
        break;    
      }              
      case SYS_OPEN: {// 6
        // printf("open num %s\n",(char *) *(esp + 1));
        f->eax = open ((char *) *(esp + 1));
        break;    
      }              
      case SYS_FILESIZE: {// 7
        f->eax = filesize (*(esp + 1));
        break;    
      }              
      case SYS_READ: {// 8
        // printf("read num1 %d\n",(int *) *(esp + 1));
        // printf("read num2 %d\n",(int *) *(esp + 2));
        // printf("read num3 %d\n",(int *) *(esp + 3));

        f->eax = read (*(esp + 1), (void *) *(esp + 2), *(esp + 3));
        break;    
      }              
      case SYS_WRITE: {// 9
      #ifdef DEBUG2
        printf("esp %p\n", *(esp));
        printf("write num1 %p\n",(int *) *(esp + 1));
        printf("write num2 %p\n",(int *) *(esp + 2));
        printf("write num3 %p\n",(int *) *(esp + 3));
      #endif
        f->eax = write (*(esp + 1), (void *) *(esp + 2), *(esp + 3));
        break;      
      }
      case SYS_SEEK: {// 10
        seek (*(esp + 1), *(esp + 2));
        break;    
      }              
      case SYS_TELL: {// 11
        f->eax = tell (*(esp + 1));
        break;    
      }              
      case SYS_CLOSE: {// 12
        // printf("close tid num %d\n",(int *) *(esp + 1));
        close (*(esp + 1));
        break;    
      }              
      case SYS_MMAP:{ //13 This is the mmap handler
	      f->eax = mmap (*(esp + 1), (void *) *(esp + 2));
        break;
      }           
      case SYS_MUNMAP:{ //14 This is the munmap handler
	      f->eax = munmap (*(esp + 1));
        break;
      }
  #ifdef FILESYS
    case SYS_CHDIR: // 15
      {
        f->eax = chdir(*(esp + 1));
        break;
      }

    case SYS_MKDIR: // 16
      {
        f->eax = mkdir(*(esp + 1));
        break;
      }

    case SYS_READDIR: // 17
      {
        f->eax = readdir(*(esp + 1), *(esp + 2));
        break;
      }

    case SYS_ISDIR: // 18
      {
        f->eax = isdir(*(esp + 1));
        break;
      }

    case SYS_INUMBER: // 19
      {
        f->eax = inumber(*(esp + 1));
        break;
      }

  #endif






      default: {
        printf("[ERROR] system call %d is unimplemented!\n", syscall_number);
        break;    
      }
    }              
  }
}


void 
halt(void) {
  shutdown_power_off();
}


void 
exit(int status) {
  #ifdef DEBUG
  printf("Syscall exit \n");
  #endif
  struct child_thread_status *child;
  struct thread *cur = thread_current ();


  printf ("%s: exit(%d)\n", thread_current()->name, status);
  struct thread *parent = cur->parent;


  if (parent != NULL) 
    {
      struct list_elem *e = list_tail(&parent->children);
      while ((e = list_prev (e)) != list_head (&parent->children))
        {
          child = list_entry (e, struct child_thread_status, elem_child_status);
          if (child->child_id == cur->tid)
          { 
            child->child_exit_status = status;
          }
        }
    }

  thread_exit();
}

pid_t
exec (const char *cmd_line)
{
  /* a thread's id. When there is a user process within a kernel thread, we
   * use one-to-one mapping from tid to pid, which means pid = tid
   */
  tid_t tid;
  struct thread *cur;

  /* check if the user pinter is valid */
  if (!is_good_ptr (cmd_line))
  {
    exit (-1);
  }

  cur = thread_current ();
  lock_acquire (&filesys_lock); // load() uses filesystem
  tid = process_execute (cmd_line);
  lock_release (&filesys_lock);
  /* check if load fail */
  if (cur->child_load_status == -1)
    tid = -1;
  return tid;
}

int wait(pid_t pid) {
  return process_wait(pid);
}

bool
create (const char *file_name, unsigned size)
{
  bool status;

  if (!is_good_ptr (file_name))
    exit (-1);

  lock_acquire (&filesys_lock);
  status = filesys_create(file_name, size,false);  
  lock_release (&filesys_lock);
  return status;
}


bool 
remove (const char *file_name)
{
  bool status;
  if (!is_good_ptr (file_name))
    exit (-1);

  lock_acquire (&filesys_lock);  
  status = filesys_remove (file_name);
  lock_release (&filesys_lock);
  #ifdef  FILESYSDebug
    printf("state of remove %d\n",status);
  #endif
  return status;
}

int
open (const char *file_name)
{
  struct file *f;
  struct file_descriptor *fd;
  int status = -1;
  #ifdef  FILESYSDebug
    printf("file_name %s\n",file_name);
  #endif
  if (!is_good_ptr(file_name))
  {
    exit (-1);
  }

  lock_acquire (&filesys_lock); 
 
  f = filesys_open (file_name);
  #ifdef  FILESYSDebug
    printf("f != NULL %d\n",f != NULL);
  #endif      
  if (f != NULL)
    {
      fd = calloc (1, sizeof *fd);
      fd->fd_num =  ++fd_current;
      fd->owner = thread_current ()->tid;
      fd->file_struct = f;
      status = fd->fd_num;
    



    // directory handling
      struct inode *inode = file_get_inode(fd->file_struct);
      if(inode != NULL && inode_is_directory(inode)) {

        fd->dir = dir_open( inode_reopen(inode) );

      #ifdef FILESYSDebug
        printf("================================\n");
        printf("fd->dir %d\n",fd->dir != NULL);
        printf("fd_open is %d\n",fd->fd_num);
        printf("================================\n");  
      #endif
        
      }
      else
      {
        fd->dir = NULL;
      }
  
#ifdef FILESYSDebug
  printf("statue is:%d\n",status);
#endif

  list_push_back (&open_files, &fd->elem);

  
  }
  lock_release (&filesys_lock);
  return status;
}

int
filesize (int fd)
{
  struct file_descriptor *fd_struct;
  lock_acquire (&filesys_lock); 
  int status = -1;
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_length (fd_struct->file_struct);
  lock_release (&filesys_lock);
  return status;
}


int read(int fd, void* buffer, unsigned size) {
  struct file_descriptor *fd_struct;
  int status = 0;
  struct thread *t = thread_current ();

  unsigned buffer_size = size;
  void * buffer_tmp = buffer;
  // whether buffer good 
  if (!(buffer_tmp != NULL && is_user_vaddr (buffer_tmp)))
    {
      exit (-1);
    }

  lock_acquire (&filesys_lock);   
  if (fd == STDOUT_FILENO)
      status = -1;
  else if (fd == STDIN_FILENO)
    {
      uint8_t c;
      unsigned counter = size;
      uint8_t *buf = buffer;
      while (counter > 1 && (c = input_getc()) != 0)
        {
          *buf = c;
          buffer++;
          counter--; 
        }
      *buf = 0;
      status = size - counter;
    }
  else 
    {
      fd_struct = get_open_file (fd);
      if (fd_struct != NULL) {
#ifdef VM
      preload_and_pin_pages(buffer, size);
#endif

      status = file_read (fd_struct->file_struct, buffer, size);

#ifdef VM
      unpin_preloaded_pages(buffer, size);
#endif
      }
	      
    }
  lock_release (&filesys_lock);
  return status;
}


int
write (int fd, const void *buffer, unsigned size)
{
  // printf("in write \n");
  struct file_descriptor *fd_struct;  
  int status = -1;
#ifdef FILESYSDebug
  printf("in write \n");
  printf("%d\n",buffer);
  printf("%d\n",fd);
  printf("%d\n",size);
  
#endif



  lock_acquire (&filesys_lock); 
  if (fd == STDIN_FILENO)
    {
      status = -1;
    }
  else if (fd ==  STDOUT_FILENO)
    {
      putbuf (buffer, size);;
      status = size;
    }
  else 
    {
      fd_struct = get_open_file (fd);
      if (fd_struct->dir != NULL) {
        fd_struct = NULL;
      }
        #ifdef FILESYSDebug
          printf("fd_struct != NULL: %d\n",fd_struct != NULL);
        #endif
      if (fd_struct != NULL) {
#ifdef VM
        preload_and_pin_pages(buffer, size);
#endif

      status = file_write (fd_struct->file_struct, buffer, size);

#ifdef VM
        unpin_preloaded_pages(buffer, size);
#endif
      }
    }
  lock_release (&filesys_lock);
  // printf("end write\n");

  return status;
}



void 
seek (int fd, unsigned position)
{
  lock_acquire (&filesys_lock); 
  struct file_descriptor *fd_struct;
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    file_seek (fd_struct->file_struct, position);
  lock_release (&filesys_lock);
}

unsigned 
tell (int fd)
{
  lock_acquire (&filesys_lock); 
  struct file_descriptor *fd_struct;
  int status = -1;
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_tell (fd_struct->file_struct);
  lock_release (&filesys_lock);
  return status;
}


void 
close (int fd)
{
  lock_acquire (&filesys_lock); 
  struct file_descriptor *fd_struct;
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL && fd_struct->owner == thread_current ()->tid)
    close_open_file (fd);
  lock_release (&filesys_lock);
}

#ifdef VM
mmapid_t mmap(int fd, void *upage) {
  // check arguments
#ifdef DEBUG
        printf("in mmap\n");
#endif
  if (upage == NULL || pg_ofs(upage) != 0) return -1;
  if (fd <= 1) return -1; // 0 and 1 are unmappable
  struct thread *curr = thread_current();

  lock_acquire (&filesys_lock);

  /* 1. Open file */
  struct file *f = NULL;
  struct file_descriptor* file_d = get_open_file (fd);
  if(file_d && file_d->file_struct) {
    // reopen file so that it doesn't interfere with process itself
    // it will be store in the mmap_desc struct (later closed on munmap)
    f = file_reopen (file_d->file_struct);
  }
  if(f == NULL) goto MMAP_FAIL;

  size_t file_size = file_length(f);
  if(file_size == 0) goto MMAP_FAIL;

  /* 2. Mapping memory pages */
  // First, ensure that all the page address is NON-EXIESENT.
  size_t offset;
  for (offset = 0; offset < file_size; offset += PGSIZE) {
    void *addr = upage + offset;
    if (vm_supt_has_entry(curr->supt, addr)) goto MMAP_FAIL;
  }

  // Now, map each page to filesystem
  for (offset = 0; offset < file_size; offset += PGSIZE) {
    void *addr = upage + offset;

    size_t read_bytes = (offset + PGSIZE < file_size ? PGSIZE : file_size - offset);
    size_t zero_bytes = PGSIZE - read_bytes;

    vm_supt_install_filesys(curr->supt, addr, f, offset, read_bytes, zero_bytes, true);
  }

  /* 3. Assign mmapid */
  mmapid_t mid;
  if (! list_empty(&curr->mmap_list)) {
    mid = list_entry(list_back(&curr->mmap_list), struct mmap_desc, elem)->id + 1;
  }
  else mid = 1;

  struct mmap_desc *mmap_d = (struct mmap_desc*) malloc(sizeof(struct mmap_desc));
  mmap_d->id = mid;
  mmap_d->file = f;
  mmap_d->addr = upage;
  mmap_d->size = file_size;
  list_push_back (&curr->mmap_list, &mmap_d->elem);

  // OK, release and return the mid
  lock_release (&filesys_lock);
  return mid;


MMAP_FAIL:
  // finally: release and return
  lock_release (&filesys_lock);
  #ifdef DEBUG
  printf("fail\n");
  #endif
  return -1;
}


bool munmap (mmapid_t mid)
{
#ifdef DEBUG
        printf("in munmap\n");
#endif
  struct thread *curr = thread_current();
  struct mmap_desc *mmap_d = find_mmap_desc(curr, mid);
#ifdef DEBUG
        // printf("in munmap %d \n",mmap_d->id);
#endif
  if(mmap_d == NULL) { // not found such mid
    return false; // or fail_invalid_access() ?
  }

  lock_acquire (&filesys_lock);
  {
    // Iterate through each page
    size_t offset, file_size = mmap_d->size;
    for(offset = 0; offset < file_size; offset += PGSIZE) {
      void *addr = mmap_d->addr + offset;
      size_t bytes = (offset + PGSIZE < file_size ? PGSIZE : file_size - offset);
#ifdef DEBUG
    printf("--------&&&&&&&&&&&&&&&&&&&&&------------\n");
    printf("thread_current ()->tid = %d\n",thread_current ()->tid);
    printf("thread_current ()t->name = %s\n",thread_current ()->name);

    printf("pd is %d \n",curr->pagedir == NULL);
    printf("-----------&&&&&&&&&&&&&&&&&&&&&&&&&&------------------------------- \n");
#endif
      vm_supt_mm_unmap (curr->supt, curr->pagedir, addr, mmap_d->file, offset, bytes);
    }

    // Free resources, and remove from the list
    list_remove(& mmap_d->elem);
    file_close(mmap_d->file);
    free(mmap_d);
  }
  lock_release (&filesys_lock);

  return true;
}


static struct mmap_desc*
find_mmap_desc(struct thread *t, mmapid_t mid)
{
  ASSERT (t != NULL);

  struct list_elem *e;

  if (! list_empty(&t->mmap_list)) {
    for(e = list_begin(&t->mmap_list);
        e != list_end(&t->mmap_list); e = list_next(e))
    {
      struct mmap_desc *desc = list_entry(e, struct mmap_desc, elem);
      if(desc->id == mid) {
        return desc;
      }
    }
  }

  return NULL; // not found
}

void preload_and_pin_pages(const void *buffer, size_t size)
{
  struct supplemental_page_table *supt = thread_current()->supt;
  uint32_t *pagedir = thread_current()->pagedir;

  void *upage;
  for(upage = pg_round_down(buffer); upage < buffer + size; upage += PGSIZE)
  {
    load_vm(esp, upage);
    vm_pin_page (supt, upage);
  }
}

void unpin_preloaded_pages(const void *buffer, size_t size)
{
  struct supplemental_page_table *supt = thread_current()->supt;

  void *upage;
  for(upage = pg_round_down(buffer); upage < buffer + size; upage += PGSIZE)
  {
    vm_unpin_page (supt, upage);
  }
}
#endif



#ifdef FILESYS
bool mkdir(const char *filename)
{
  bool return_code;

  lock_acquire (&filesys_lock);
  return_code = filesys_create(filename, 0, true);
  lock_release (&filesys_lock);
  #ifdef FILESYSDebug
    printf("mkdir success?: %d\n",return_code);
  #endif

  return return_code;
}

bool chdir(const char *filename)
{
  bool return_code;
  lock_acquire (&filesys_lock);
  return_code = filesys_chdir(filename);
  lock_release (&filesys_lock);
  #ifdef FILESYSDebug
    printf("chdir success?: %d\n",return_code);
  #endif
  return return_code;
}

bool readdir(int fd, char *name)
{
  struct file_descriptor* file_d;
  bool ret = false;

  lock_acquire (&filesys_lock);
  file_d = get_open_file (fd);
  if (file_d == NULL) {
    lock_release (&filesys_lock);
    return false;
  }
  struct inode *inode;
  inode = file_get_inode(file_d->file_struct); // file descriptor -> inode
  if(inode == NULL) {
    lock_release (&filesys_lock);
    return false;
  }
  // check whether it is a valid directory
  if(! inode_is_directory(inode)) {
    lock_release (&filesys_lock);
    return false;
  }
  struct file *file = file_d->file_struct;
  off_t pos = file_tell(file);
  struct dir *dir = dir_open(inode);
  dir_seek(dir, pos);
  ret = dir_readdir (file_d->dir, name);
  file_seek(file, dir_tell(dir));
  free(dir);
  lock_release (&filesys_lock);
  return ret;
}

bool isdir(int fd)
{
  lock_acquire (&filesys_lock);

  struct file_descriptor* file_d = get_open_file (fd);
  if (!file_d)
    return false;
  bool ret = inode_is_directory (file_get_inode(file_d->file_struct));

  lock_release (&filesys_lock);
  return ret;
}

int inumber(int fd)
{
  lock_acquire (&filesys_lock);

  struct file_descriptor* file_d = get_open_file (fd);
  if (!file_d)
    return -1;
  int ret = (int) inode_get_inumber (file_get_inode(file_d->file_struct));

  lock_release (&filesys_lock);
  return ret;
}


#endif