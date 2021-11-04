#include "file_syscalls.h"  // prototype for this file
#include <kern/errno.h>  // Errors (EBADF, EFAULT, ..)
#include <types.h>  // types (userptr_t, size_t, ..)
#include <limits.h>  // OPEN_MAX
#include <current.h>  // curproc
#include <proc.h>  // proc struct (for curproc)
#include <kern/fcntl.h>  // open flags (O_RDONLY, O_WRONLY, ..)
#include <uio.h>  // for moving data (uio, iovec)
#include <vnode.h>  // for moving data (VOP_OPEN, VOP_READ, ..)

int sys_read(int fd, userptr_t buf_ptr, size_t size) {

	if (fd < 0 || fd >= OPEN_MAX) {  // if fd out of bounds of fdtable
		return EBADF;
	}

	struct fhandle *open_file;
	open_file = curproc->p_fdtable[fd];  // get file handle from file table

	if (open_file == NULL) {
		return EBADF;
	}

	if (!(open_file->flags && (O_RDONLY || O_RDWR))) {  // if file not opened with O_RDONLY or O_RDWR flag
		return EBADF;
	}

	lock_acquire(open_file->lock);  // synchronize access to file handle during read

	// set up iovec and uio structs to read data from file with VOP_READ
	struct iovec iov;
	struct uio u;

	iov.iov_ubase = buf_ptr;
	iov.iov_len = size;
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_resid = size;
	u.uio_offset = open_file->offset;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = curproc->p_addrspace;

	int err = VOP_READ(open_file->vn, &u);  // read from file
	if (err) {
		lock_release(open_file->lock);  // release lock
		return err;
	}

	open_file->offset = u.uio_offset;  // advance offset by amount read

	lock_release(open_file->lock);  // release lock

	return size - u.uio_resid;  // return number of bytes read
}
