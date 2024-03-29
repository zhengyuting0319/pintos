#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/frame.h"

// #define FILESYSDebug


// #define DEBUG

// #ifndef VM
// // alternative of vm-related functions introduced in Project 3
// #define vm_frame_allocate(x, y) palloc_get_page(x)
// #define vm_frame_free(x) palloc_free_page(x)
// #endif



struct thread* tmp_currnent;
static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy, *new_file_name;
  tid_t tid;

  struct child_thread_status *child = NULL;
  struct thread *cur;
  // printf("%d\n",&cur->children == NULL);


  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* use new_file_name to produce token (ignore arguments) */
  new_file_name = palloc_get_page (0);
  if (new_file_name == NULL) {
    return TID_ERROR;  
  } 
  strlcpy (new_file_name, file_name, PGSIZE);
  /*
  Example usage:
   char s[] = "  String to  tokenize. ";
   char *token, *save_ptr;  
   
   for (token = strtok_r (s, " ", &save_ptr); token != NULL;
        token = strtok_r (NULL, " ", &save_ptr))
     printf ("'%s'\n", token);
     
   outputs:
     'String'
     'to'
     'tokenize.'
  */ 
  char *token, *save_ptr;
  token = strtok_r(new_file_name, " ", &save_ptr);
  
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (token, PRI_DEFAULT, start_process, fn_copy);
  palloc_free_page (new_file_name); 
  
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);
  else 
    {  
      cur = thread_current ();
      child = calloc (1, sizeof *child);
      if (child != NULL) 
        {
        
          child->child_id = tid;
          list_push_back (&cur->children, &child->elem_child_status);
  }       
    }     
    // printf("end exec\n");
    
  cur->excute_subtract_wait += 1;
  
  sema_down(&cur->initial_sema);
  return tid;
} 


/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  int load_status;
  struct thread *cur = thread_current ();
  struct thread *parent;

  
  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  
  if (!success) 
    load_status = -1;
  else
    load_status = 1;
  
  /* check if load successfully, if not success, syscall_execute will return -1, that's why we need to start_process before excute finish */
  parent = cur->parent;
  
  parent->child_load_status = load_status;


  
      #ifdef FILESYSDebug
        printf("================================\n");
        printf("parent != NULL %d\n",parent != NULL);
        printf("parent->cwd != NULL %d\n",parent->cwd != NULL);
        printf("================================\n");  
      #endif

  /* Set up CWD */
  if (parent != NULL && parent->cwd != NULL) {
    // child process inherits the CWD
    cur->cwd = dir_reopen(parent->cwd);
  }
  else {
    cur->cwd = dir_open_root();
  }


  /* If load failed, quit. */
  palloc_free_page (file_name);

  sema_up(&parent->initial_sema);
  if (!success) 
    thread_exit ();
  
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  int status;
  struct thread *cur;
  struct child_thread_status *child = NULL;
  struct list_elem *e, *it;

  if (child_tid != TID_ERROR)
  {
    cur = thread_current();
    /* check if child valid and get child */
    e = list_tail (&cur->children);
    while ((e = list_prev (e)) != list_head (&cur->children))
    {
      child = list_entry(e, struct child_thread_status, elem_child_status);
      if (child->child_id == child_tid)
        break;
    }
    if (child == NULL)
      return -1;

    else {
      /* sema_down to wake child, also check if the child_thread indeed exist, avoid wait twice */
      while (thread_get_by_id (child_tid) != NULL)
        sema_down(&cur->wait_sema);

      /* back to parent, and child have alread exit, return child_exit_status */
      status = child->child_exit_status;

      /* avoid wait twice, excute_time must >= wait_time(child's time) */
      cur->excute_subtract_wait -= 1;
      if (cur->excute_subtract_wait < 0) {
        status = -1;
      }
    }
  }
  else
    status = TID_ERROR;

  return status;
}




/* Free the current process's resources. */
void
process_exit (void)
{
  
  struct thread *cur = thread_current ();
  uint32_t *pd;
  struct thread *parent;
  struct list_elem *e;
  struct list_elem *next;
  struct child_thread_status *child;

  /* Resources should be cleaned up */
  // 1. file descriptors
  struct list *fdlist = &cur->file_descriptors;
  while (!list_empty(fdlist)) {
    struct list_elem *e = list_pop_front (fdlist);
    struct file_descriptor *desc = list_entry(e, struct file_descriptor, elem);
    file_close(desc->file_struct);
    palloc_free_page(desc); // see sys_open()
  }
  
#ifdef VM
  // mmap descriptors
  struct list *mmlist = &cur->mmap_list;
  while (!list_empty(mmlist)) {
    struct list_elem *e = list_begin (mmlist);
    struct mmap_desc *desc = list_entry(e, struct mmap_desc, elem);

    // in sys_munmap(), the element is removed from the list
#ifdef DEBUG
    printf("------------------------------------------------\n");
    printf("exit\n");
    printf("thread_current ()->tid = %d\n",thread_current ()->tid);
    printf("thread_current ()t->name = %s\n",thread_current ()->name);
    printf("thread_current ()->pd = %d\n",thread_current ()->pagedir);

    printf("------------------------------------------------\n");

#endif
    ASSERT( munmap (desc->id) == true );
  }
  vm_supt_destroy (cur->supt);
  cur->supt = NULL;
#endif
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  #ifdef DEBUG
  printf("start\n");
  printf("pd is %d \n",cur->pagedir == NULL);
  #endif
  pd = cur->pagedir;
  
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      #ifdef DEBUG
      printf("in pd != NULL\n");
      printf("%d\n",thread_current ()->pagedir == NULL);
      #endif
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  // printf("start\n");
  #ifdef DEBUG
  printf("pd is %x \n",cur->pagedir == NULL);
  #endif
  /* free child */
  e = list_begin (&cur->children);
  while (e != list_tail(&cur->children))
  {
    next = list_next (e);
    child = list_entry (e, struct child_thread_status, elem_child_status);
    list_remove (e);
    free (child);
    e = next;
  }

  /* Close CWD */
  if(cur->cwd) 
  {
    dir_close (cur->cwd);
  }


  #ifdef DEBUG
  printf("pd is %d \n",cur->pagedir == NULL);
  #endif
  if (cur->exec_file != NULL)
  {
    file_allow_write (cur->exec_file);
  }

  /* close all file the process uses */
  struct file_descriptor *fd_struct; 
  e = list_begin (&open_files);
  while (e != list_tail (&open_files)) {
    next = list_next (e);
    fd_struct = list_entry (e, struct file_descriptor, elem);
    if (fd_struct->owner == cur->tid)
    {
      list_remove (e);
      file_close (fd_struct->file_struct);
      free (fd_struct);
    }
    e = next;
  }  

  /* when exit cur, awake his parent */
  parent = cur->parent;
  sema_up(&parent->wait_sema);
}




/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();
#ifdef DEBUG
    printf("**********************************\n");
    printf("activate\n");
    printf("thread_current ()->tid = %d\n",thread_current ()->tid);
    printf("thread_current ()t->name = %s\n",thread_current ()->name);
    printf("thread_current ()->pd = %d\n",thread_current ()->pagedir);
    printf("**********************************\n");

#endif

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char *file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();

#ifdef VM
  t->supt = vm_supt_create ();
#endif

  if (t->pagedir == NULL) 
    goto done;
  process_activate ();


  char *whole_file_name, *save_ptr;
  whole_file_name = palloc_get_page (0);
  if (whole_file_name == NULL)
    return TID_ERROR;
  strlcpy (whole_file_name, file_name, PGSIZE);
  file_name = strtok_r(file_name, " ", &save_ptr);

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }


  t->exec_file = file; 
  file_deny_write (file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, whole_file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  // file_close (file);
  palloc_free_page(whole_file_name);
  return success;
}

/* load() helpers. */

bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

#ifdef VM
      // Lazy load
      struct thread *curr = thread_current ();
      ASSERT (pagedir_get_page(curr->pagedir, upage) == NULL); // no virtual page yet?
      // printf('%x\n', upage);
      if (! vm_supt_install_filesys(curr->supt, upage,
            file, ofs, page_read_bytes, page_zero_bytes, writable) ) {
        return false;
      }
#else
      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page(PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          #ifdef DEBUG
          printf("Add the page to the process's address space.\n");
          #endif
          palloc_free_page (kpage);
          return false; 
        }
#endif
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
#ifdef VM
      ofs += PGSIZE;
#endif
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, char *file_name) 
{
  uint8_t *kpage;
  bool success = false;

  // kpage = palloc_get_page (PAL_USER | PAL_ZERO);//vm_frame_allocate (PAL_USER | PAL_ZERO, PHYS_BASE - PGSIZE);
  // if (kpage != NULL) 
    //install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);//
  success = true;//stack_growth(((uint8_t *) PHYS_BASE) - PGSIZE);
  if (success) {
    // stack_growth(((uint8_t *) PHYS_BASE) - PGSIZE);
    *esp = PHYS_BASE;

    char *rec_tokens[100];
    int word_align_and_argv_argc = 0;
    int ip = 0;
    char *token, *save_ptr;
    for (token = strtok_r (file_name, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)) {
      rec_tokens[ip++] = token;
      word_align_and_argv_argc += strlen(token) + 1;
    }
    int argc = ip;
    for (; ip > 0; ip--) {
      *esp -= sizeof(char) * (strlen(rec_tokens[ip-1]) + 1);
      memcpy(*esp, rec_tokens[ip-1], sizeof(char) * (strlen(rec_tokens[ip-1]) + 1));
    }

    word_align_and_argv_argc = (4 - word_align_and_argv_argc % 4) % 4 + 4;
    *esp -= word_align_and_argv_argc;

    memset(*esp, 0, word_align_and_argv_argc);
    
    //hex_dump((uintptr_t)*esp, *esp, 16, true);

    uint32_t tmp_esp = PHYS_BASE;
    for (ip = argc; ip > 0; ip--) {
      *esp -= 4;
      tmp_esp = tmp_esp - strlen(rec_tokens[ip - 1]) - 1;
      memcpy(*esp, &tmp_esp, sizeof(uint32_t));
    }
    
    *esp -= 4;
    tmp_esp = *esp + 4;
    memcpy(*esp, &tmp_esp, sizeof(uint32_t));
    *esp -= 4;
    tmp_esp = argc;
    memcpy(*esp, &tmp_esp, sizeof(uint32_t));
    *esp -= 4;
    memset(*esp, 0, 4);
    #ifdef DEBUG
    hex_dump((uintptr_t)*esp, *esp, 80, true);
    #endif
      
  }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

