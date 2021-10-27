#include <types.h>  // types (userptr_t, size_t, ...)

#ifndef _SYSCALLS_H_
#define _SYSCALLS_H_

#include "opt-syscalls.h"
#if OPT_SYSCALLS
int sys_read(int fd, userptr_t buf_ptr, size_t size);
#endif

#endif
