#include "file_syscalls.h" // prototype for this file
#include <kern/errno.h>	   // Errors (EBADF, EFAULT, ..)
#include <types.h>		   // types (userptr_t, size_t, ..)
#include <limits.h>		   // OPEN_MAX
#include <current.h>	   // curproc
#include <proc.h>		   // proc struct (for curproc)
#include <kern/fcntl.h>	   // open flags (O_RDONLY, O_WRONLY, ..)
#include <uio.h>		   // for moving data (uio, iovec)
#include <vnode.h>		   // for moving data (VOP_OPEN, VOP_READ, ..)

int sys_read(int fd, userptr_t buf_ptr, size_t size)
{

	if (fd < 0 || fd >= OPEN_MAX)
	{ // if fd out of bounds of fdtable
		return EBADF;
	}

	struct fhandle *open_file;
	open_file = curproc->p_fdtable[fd]; // get file handle from file table

	if (open_file == NULL)
	{
		return EBADF;
	}

	if (!(open_file->flags && (O_RDONLY || O_RDWR)))
	{ // if file not opened with O_RDONLY or O_RDWR flag
		return EBADF;
	}

	lock_acquire(open_file->lock); // synchronize access to file handle during read

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

	int err = VOP_READ(open_file->vn, &u); // read from file
	if (err)
	{
		lock_release(open_file->lock); // release lock
		return err;
	}

	open_file->offset = u.uio_offset; // advance offset by amount read

	lock_release(open_file->lock); // release lock

	return size - u.uio_resid; // return number of bytes read
}

int sys_write(int fd, userptr_t buf_ptr, size_t size)
{

	if (fd < 0 || fd >= OPEN_MAX)
	{ // if fd out of bounds of fdtable
		return EBADF;
	}
	// should we lock the process first? or just the file
	struct fhandle *open_file;
	open_file = curproc->p_fdtable[fd]; // we get the file from table

	if (open_file == NULL)
	{
		return EBADF;
	}

	if (!(open_file->flags && (O_WRONLY || O_RDWR)))
	{
		return EBADF;
	}

	lock_acquire(open_file->lock); // synchronize access to file handle during write

	struct iovec iov;
	struct uio u;
	int result;

	iov.iov_ubase = buf_ptr; // we set a user pointer to the buffer , change to ubase
	iov.iov_len = size;
	u.uio_iov = &iov;					// we set the pointer to the buffer we want to transfer
	u.uio_iovcnt = 1;					// set quantity of buffers
	u.uio_offset = open_file->offset;	// set offset of file
	u.uio_resid = size;					//size of object to transfer
	u.uio_segflg = UIO_USERSPACE;		// We set the flag to user data
	u.uio_rw = UIO_WRITE;				// we set action to write
	u.uio_space = curproc->p_addrspace; // we set the addres space for the user pointer

	result = VOP_WRITE(open_file->vn, &u); // write to file, if not all the content was writtend, it returns -1

	if (result)
	{
		lock_release(open_file->lock); // we release the lock and return result
		return result;
	}
	// If not all the content has been written

	open_file->offset += ((off_t)size - u.uio_resid);

	lock_release(open_file->lock);

	return size - u.uio_resid;
}
