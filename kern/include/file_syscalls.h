#include <types.h> // types (userptr_t, size_t, ...)
#include <synch.h> // synchronization (lock)

#ifndef _FILE_SYSCALLS_H_
#define _FILE_SYSCALLS_H_

#include "opt-filesc.h"
#if OPT_FILESC

struct fhandle
{
	struct vnode *vn;
	off_t offset;
	int flags;
	unsigned int ref_count;
	struct lock *lock;
};

int sys_open(userptr_t filename, int flags, int *retval);
int sys_read(int fd, userptr_t buf, size_t size, ssize_t *retval);
int sys_lseek(int fd, off_t pos, int whence, off_t *retval);
int sys___getcwd(userptr_t buf, size_t buflen, int *retval);
int sys_write(int fd, userptr_t buf_ptr, size_t size, ssize_t *retval);
int sys_close(int fd);

#endif

#endif
