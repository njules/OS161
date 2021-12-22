#include "file_syscalls.h" // prototype for this file
#include <kern/errno.h>	   // Errors (EBADF, EFAULT, ..)
#include <types.h>		   // types (userptr_t, size_t, ..)
#include <limits.h>		   // OPEN_MAX
#include <current.h>	   // curproc
#include <proc.h>		   // proc struct (for curproc)
#include <kern/fcntl.h>	   // open flags (O_RDONLY, O_WRONLY, ..)
#include <uio.h>		   // for moving data (uio, iovec)
#include <vnode.h>		   // for moving data (VOP_OPEN, VOP_READ, ..)
#include <vfs.h>		   // for vfs functions (vfs_open, vfs_close)
#include <copyinout.h>	   // for moving data (copyinstr)
#include <kern/seek.h>	   // for seek constants (SEEK_SET, SEEK_CUR, ..)
#include <kern/stat.h>	   // for getting file info via VOP_STAT (stat)

int sys_open(userptr_t filename, int flags, int *retval)
{

	// check flags are compatible with access type (r, w, rw)
	if (flags & O_RDONLY)
	{
		if (flags & !(O_RDONLY | O_CREAT | O_EXCL))
		{
			return EINVAL;
		}
	}
	else if (flags & O_WRONLY)
	{
		if (flags & !(O_WRONLY | O_CREAT | O_EXCL | O_TRUNC | O_APPEND))
		{
			return EINVAL;
		}
	}
	else if (flags & O_RDWR)
	{
		if (flags & !(O_RDWR | O_CREAT | O_EXCL | O_TRUNC))
		{
			return EINVAL;
		}
	}
	else
	{ // if none of O_RDONLY, O_WRONLY, O_RDWR are specified
		return EINVAL;
	}

	// find first available slot in file table
	int fd = -1;
	for (int i = 3; i < OPEN_MAX; i++)
	{
		if (curproc->p_fdtable[i] == NULL)
		{
			fd = i;
			break;
		}
	}
	if (fd == -1)
	{ // if no free slot found
		return EMFILE;
	}

	// copy filename from userpointer into kernel buffer
	char path[PATH_MAX + 1];
	size_t pathlen;
	int err = copyinstr(filename, path, sizeof(path) - 1, &pathlen);
	if (err)
	{
		return err;
	}

	// open file
	struct vnode *vn;
	err = vfs_open(path, flags, 0, &vn);
	if (err)
	{
		return err;
	}

	// compute file offset (EOF if O_APPEND is specified, 0 else)
	off_t offset = 0;
	if (flags | O_APPEND)
	{
		struct stat *file_stat = NULL;
		err = VOP_STAT(vn, file_stat);
		if (err)
		{
			return err;
		}
		offset = file_stat->st_size;
	}

	// initialize fhandle struct
	struct fhandle *open_file;
	open_file = kmalloc(sizeof(struct fhandle));

	open_file->vn = vn;
	open_file->offset = offset;
	open_file->flags = flags;
	open_file->ref_count = 1;
	open_file->lock = lock_create(path);

	lock_acquire(open_file->lock); // synchronize access to file table during open

	// add file handle to fdtable
	curproc->p_fdtable[fd] = open_file;

	lock_release(open_file->lock); // release lock

	*retval = fd;
	return 0;
}

int sys_read(int fd, userptr_t buf, size_t size, ssize_t *retval)
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

	if (!(open_file->flags & (O_RDONLY | O_RDWR)))
	{ // if file not opened with O_RDONLY or O_RDWR flag
		return EBADF;
	}

	lock_acquire(open_file->lock); // synchronize access to file handle during read

	// set up iovec and uio structs to read data from file with VOP_READ
	struct iovec iov;
	struct uio u;

	iov.iov_ubase = buf;
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

	*retval = size - u.uio_resid; // return number of bytes read on success
	return 0;
}

int sys_write(int fd, userptr_t buf_ptr, size_t size, ssize_t *retval)
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

	*retval = size - u.uio_resid; // return number of bytes read on success
	return 0;
}

int sys_lseek(int fd, off_t pos, int whence, off_t *retval)
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

	lock_acquire(open_file->lock); // synchronize access to file handle during seek

	if (!VOP_ISSEEKABLE(open_file->vn))
	{								   // check if file allows for seeking
		lock_release(open_file->lock); // release lock
		return ESPIPE;
	}

	off_t offset = open_file->offset;

	switch (whence)
	{
	case SEEK_SET:
		offset = pos;
		break;
	case SEEK_CUR:
		offset = offset + pos;
		break;
	case SEEK_END:; // empty statement for label
		struct stat *file_stat = NULL;
		int err = VOP_STAT(open_file->vn, file_stat);
		if (err)
		{
			lock_release(open_file->lock); // release lock
			return err;
		}
		offset = file_stat->st_size + pos;
		break;
	default:
		lock_release(open_file->lock); // release lock
		return EINVAL;
	}

	if (offset < 0)
	{
		lock_release(open_file->lock); // release lock
		return EINVAL;
	}

	open_file->offset = offset; // update offset in file handle

	lock_release(open_file->lock); // release lock

	*retval = offset;
	return 0;
}

int sys___getcwd(userptr_t buf, size_t buflen, int *retval)
{

	// set up iovec and uio structs to copy cwd
	struct iovec iov;
	struct uio u;

	iov.iov_ubase = buf;
	iov.iov_len = buflen;
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_resid = buflen;
	u.uio_offset = 0;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = curproc->p_addrspace;

	int err = vfs_getcwd(&u);
	if (err)
	{
		return err;
	}

	*retval = 0; // on success return 0
	return 0;
}

int sys_close(int fd)
{

	struct fhandle *open_file;
	open_file = curproc->p_fdtable[fd];

	lock_acquire(open_file->lock);

	if (open_file == NULL) // does it make sense do this check after the lock, if open_file was null?
	{
		lock_release(open_file->lock);
		return EBADF;
	}
	// KASSERT(open_file != NULL); // we make sure that open file is not null
	// we check if fd is invalid
	if (fd < 0 || fd > OPEN_MAX)
	{
		lock_release(open_file->lock);
		return EINVAL;
	}
	// entry_decref(entry, true);
	// ft->entries[fd] = NULL;
	if (open_file->ref_count -= 1 > 0)
	{
		lock_release(open_file->lock); // we release to not produce deadlock
		return 0;
	}; // we decrement by one and return 0

	if (open_file->ref_count == 0)
	{
		// maybe create another function with this, with other optfile
		KASSERT(open_file->vn != NULL); // i interrup the program if file is null
		vfs_close(open_file->vn);
		open_file->vn = NULL;
		lock_release(open_file->lock);
		lock_destroy(open_file->lock);
		kfree(open_file); //i dont know if kfree is implemented fully
	}
	curproc->p_fdtable[fd] = NULL;
	return 0;
}