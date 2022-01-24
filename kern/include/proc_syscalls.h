#include <types.h>  // types (userptr_t, size_t, ...)

#ifndef _EXECV_SYSCALL_H_
#define _EXECV_SYSCALL_H_

#include "opt-shell.h"
#if OPT_SHELL

int sys_execv(userptr_t program, userptr_t args);

/* convenience functions for execv */
int align(int pointer, int align);
void destroy_current_as();
void free_kargs(char** argv, int n);

#endif

#endif
