#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include "userprog/process.h"
typedef int pid_t;



struct file_descriptor
{
  int fd_num;
  tid_t owner;
  struct file *file_struct;
  struct list_elem elem;
};

struct list open_files; 
struct lock filesys_lock;


void syscall_init (void);

#endif /* userprog/syscall.h */
