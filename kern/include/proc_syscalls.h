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
void child_forkentry(void *data1, unsigned long data2);
#endif
int sys_execv(userptr_t program, userptr_t args);
void sys__exit(int exitcode);
int sys_waitpid(pid_t pid, int *retval, int options);
int sys_getpid(int *retval);

/* convenience functions for execv */
int align(int pointer, int align);

#endif
#endif
