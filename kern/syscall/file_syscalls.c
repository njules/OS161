#include "file_syscalls.h"  // prototype for this file
#include <kern/errno.h>  // Errors (ENOSYS)
#include <types.h>  // types (userptr_t, size_t, ...)

int sys_read(int fd, userptr_t buf_ptr, size_t size) {

	// TODO: verify fd and retrieve file handle from file table
	// TODO: verify access allowed by flags
	// TODO: verify buf_ptr is legal address in user space
	// TODO: read using VOP_READ and copy to userspace using iovec and uio
	// TODO: advance file offset

	if (fd == 0 || buf_ptr == 0 || size == 0) return ENOSYS;  // placeholder to avoid compiler warning
	return ENOSYS;
}
