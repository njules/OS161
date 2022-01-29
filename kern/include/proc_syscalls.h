#include <types.h> // types (userptr_t, size_t, ...)
#include <synch.h> // synchronization (lock)

#ifndef _PROC_SYSCALLS_H_
#define _PROC_SYSCALLS_H_

#include "opt-shell.h"
#if OPT_SHELL

int sys_execv(userptr_t program, userptr_t args);
void sys__exit(int exitcode);
int sys_waitpid(pid_t pid, int *retval, int options);
int sys_getpid(int *retval);

/* convenience functions for execv */
int align(int pointer, int align);
int copy_args_to_stack(int argc, char** argv, userptr_t* stackptr);

#endif
#endif
