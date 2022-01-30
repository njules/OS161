#include <types.h> // types (userptr_t, size_t, ...)
#include <synch.h> // synchronization (lock)
#include <mips/trapframe.h>

#ifndef _PROC_SYSCALLS_H_
#define _PROC_SYSCALLS_H_

#include "opt-shell.h"
#include "opt-fork.h"
#if OPT_SHELL

int sys_getpid(int *retval);
int sys_waitpid(pid_t pid, int *retval, int options);
#if OPT_FORK
int sys_fork(struct trapframe *, int *retval );
#endif
void child_forkentry(void *data1, unsigned long data2);
void sys__exit(int exitcode);

#endif
#endif
