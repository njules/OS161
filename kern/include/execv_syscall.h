#include <types.h>  // types (userptr_t, size_t, ...)

#ifndef _EXECV_SYSCALL_H_
#define _EXECV_SYSCALL_H_

#include "opt-procsc.h"
#if OPT_PROCSC

int sys_execv(userptr_t program, userptr_t args);

#endif

#endif

