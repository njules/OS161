#include <types.h> // types (userptr_t, size_t, ...)
#include <synch.h> // synchronization (lock)

#ifndef _FILE_SYSCALLS_H_
#define _FILE_SYSCALLS_H_

#include "opt-shell.h"
#if OPT_SHELL

/* File handle struct */
struct fhandle
{
	struct vnode *vn;
	off_t offset;
	int flags;
	unsigned int ref_count;
	struct lock *lock;
};

/* file syscalls */
int sys_open(userptr_t filename, int flags, int *retval);
int sys_dup2(int oldfd, int newfd, int *retval);
int sys_close(int fd);
int sys_read(int fd, userptr_t buf, size_t size, ssize_t *retval);
int sys_write(int fd, userptr_t buf, size_t size, ssize_t *retval);
int sys_lseek(int fd, off_t pos, int whence, off_t *retval);
int sys_chdir(const char *path, int32_t *retval);
int sys___getcwd(userptr_t buf, size_t buflen, int *retval);

/* convenience functions */
int create_fhandle_struct(
	char* path, int flags, int mode, off_t offset, struct fhandle** retval
);
int open_console(struct fhandle *fdtable[]);

#endif

#endif
