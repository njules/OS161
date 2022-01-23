#include <types.h> // types (userptr_t, size_t, ...)
#include <synch.h> // synchronization (lock)

#ifndef _PROC_SYSCALLS_H_
#define _PROC_SYSCALLS_H_

#include "opt-shell.h"
#if OPT_SHELL

int sys_getpid(int32_t *retval);

#endif
#endif