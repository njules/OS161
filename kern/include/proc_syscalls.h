#include <types.h> // types (userptr_t, size_t, ...)
#include <synch.h> // synchronization (lock)

#ifndef _PROC_SYSCALLS_H_
#define _PROC_SYSCALLS_H_

#include "opt-shell.h"
#if OPT_SHELL

int sys_getpid(int *retval);
int sys_waitpid(pid_t pid, int *retval, int options);
int sys_fork(struct trapframe *tf, int *retval );
void child_forkentry(void *data1, unsigned long data2);
void sys__exit(int exitcode);

#endif
#endif