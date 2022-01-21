#include "file_syscalls.h" // prototype for this file
#include <kern/errno.h>	   // Errors (EBADF, EFAULT, ..)
#include <types.h>	   // types (userptr_t, size_t, ..)
#include <limits.h>	   // OPEN_MAX
#include <current.h>	   // curproc
#include <proc.h>	   // proc struct (for curproc)
#include <kern/fcntl.h>	   // open flags (O_RDONLY, O_WRONLY, ..)
#include <uio.h>	   // for moving data (uio, iovec)
#include <vnode.h>	   // for moving data (VOP_OPEN, VOP_READ, ..)
#include <vfs.h>	   // for vfs functions (vfs_open, vfs_close)
#include <copyinout.h>	   // for moving data (copyinstr)
#include <kern/seek.h>	   // for seek constants (SEEK_SET, SEEK_CUR, ..)
#include <kern/stat.h>	   // for getting file info via VOP_STAT (stat)


int
sys_open(userptr_t filename, int flags, int *retval)
{
	int fd;
	char path[PATH_MAX + 1];
	size_t pathlen;
	off_t offset;
	struct fhandle *open_file;

	DEBUG(DB_SYSCALL,
		"Open syscall invoked, filename buf: %p, flags: 0x%x.\n",
		filename, flags);

	// check flags are legal combination
	if (flags & O_WRONLY) {
		if (flags & !(O_WRONLY | O_CREAT | O_EXCL | O_TRUNC | O_APPEND)) {
			DEBUG(DB_SYSFILE, "Open error: Invalid flags. flags: 0x%x.\n", flags);
			return EINVAL;
		}
	} else if (flags & O_RDWR) {
		if (flags & !(O_RDWR | O_CREAT | O_EXCL | O_TRUNC)) {
			DEBUG(DB_SYSFILE, "Open error: Invalid flags. flags: 0x%x.\n", flags);
			return EINVAL;
		}
	} else {  // O_RDONLY
		if (flags & !(O_RDONLY | O_CREAT | O_EXCL)) {
			DEBUG(DB_SYSFILE, "Open error: Invalid flags. flags: 0x%x.\n", flags);
			return EINVAL;
		}
	}

	// find first available slot in file table
	fd = -1;
	for (int i = 3; i < OPEN_MAX; i++) {
		if (curproc->p_fdtable[i] == NULL) {
			fd = i;
			break;
		}
	}
	// no slot found
	if (fd == -1) {
		DEBUG(DB_SYSFILE, "Open error: File table full.\n");
		return EMFILE;
	}

	// copy filename from userpointer into kernel buffer
	int err = copyinstr(filename, path, sizeof(path) - 1, &pathlen);
	if (err) {
		DEBUG(DB_SYSFILE, "Open error: Couldn't read filename.\n");
		return err;
	}

	// create fhandle struct
	open_file = kmalloc(sizeof(struct fhandle));
	err = create_fhandle_struct(path, flags, 0, 0, open_file);
	if (err) {
		DEBUG(DB_SYSFILE,
			"Open error: couldn't open file. path: %s (could be altered!),"
			" fd: %d, err: %d.\n",
			path, fd, err);
		kfree(open_file);
		return err;
	}

	// set offset  to EOF if O_APPEND
	if (flags & O_APPEND) {
		struct stat *file_stat = NULL;
		err = VOP_STAT(open_file->vn, file_stat);
		if (err) {
			DEBUG(DB_SYSFILE,
				"Open error: Couldn't compute offset. err: %d\n",
				err);
			vfs_close(open_file->vn);
			kfree(open_file);
			return err;
		}
		open_file->offset = file_stat->st_size;
	}

	// TODO: lock acquired before added to file table? revise locking of fhandles!!!!!
	lock_acquire(open_file->lock); // synchronize access to file table during open

	// add file handle to fdtable
	curproc->p_fdtable[fd] = open_file;

	lock_release(open_file->lock); // release lock

	*retval = fd;
	return 0;
}

int
sys_read(int fd, userptr_t buf, size_t size, ssize_t *retval)
{
	int err;
	struct fhandle *open_file;
	struct iovec iov;
	struct uio u;

	DEBUG(DB_SYSCALL,
		"Read syscall invoked, fd:%d, buf: %p, size: %d.\n",
		fd, buf, size);

	// check fd is within bounds
	if (fd < 0 || fd >= OPEN_MAX) {
		DEBUG(DB_SYSFILE,
			"Read error: File descriptor out of bounds. fd: %d.\n",
			fd);
		return EBADF;
	}

	// get file handle from file table
	open_file = curproc->p_fdtable[fd];

	// check that fd points to valid file handle
	if (open_file == NULL) {
		DEBUG(DB_SYSFILE,
			"Read error: fd points to invalid p_fdtable entry. fd: %d.\n",
			fd);
		return EBADF;
	}

	// check that flags allow reading from file
	if (open_file->flags & O_WRONLY) {
		DEBUG(DB_SYSFILE,
			"Read error: Flags do not allow file to be read from."
			" fd: %d, flags: 0x%x.\n",
			fd, open_file->flags);
		return EBADF;
	}

	// synchronize access to file handle during read
	lock_acquire(open_file->lock);

	// init iovec and uio to read from file
	uio_kinit(&iov, &u, buf, size, open_file->offset, UIO_READ);
	u.uio_segflg = UIO_USERSPACE;
	u.uio_space = curproc->p_addrspace;

	// read from file
	err = VOP_READ(open_file->vn, &u);
	if (err) {
		DEBUG(DB_SYSFILE,
			"Read error: Couldn't read to uio struct. err: %d\n",
			err);
		lock_release(open_file->lock);
		return err;
	}

	// update offset
	open_file->offset = u.uio_offset;

	lock_release(open_file->lock);

	// return number of bytes read
	*retval = size - u.uio_resid;
	return 0;
}

int
sys_write(int fd, userptr_t buf, size_t size, ssize_t *retval)
{
	struct fhandle *open_file;
	struct iovec iov;
	struct uio u;
	int err;

	DEBUG(DB_SYSCALL,
		"Write syscall invoked, fd: %d, buf: %p, size: %d.\n",
		fd, buf, size);

	// check fd is within bounds
	if (fd < 0 || fd >= OPEN_MAX) {
		DEBUG(DB_SYSFILE,
			"Write error: File descriptor out of bounds. fd: %d.\n",
			fd);
		return EBADF;
	}

	// get file handle from file table
	open_file = curproc->p_fdtable[fd];

	// check that fd points to valid file handle
	if (open_file == NULL) {
		DEBUG(DB_SYSFILE,
			"Write error: fd points to invalid p_fdtable entry. fd: %d.\n",
			fd);
		return EBADF;
	}
<<<<<<< HEAD
	lock_acquire(open_file->lock);
	if (!(open_file->flags & (O_WRONLY | O_RDWR))) {
=======

	// check that flags allow writing to file
	if (open_file->flags & O_RDONLY) {
>>>>>>> master
		DEBUG(DB_SYSFILE,
			"Write error: Flags do not allow file to be written to."
			" fd: %d, flags: 0x%x.\n",
			fd, open_file->flags);
		lock_release(open_file->lock);
		return EBADF;
	}
	
	// synchronize access to file handle during write
	lock_acquire(open_file->lock);

	// init iovec and uio to write to file
	uio_kinit(&iov, &u, buf, size, open_file->offset, UIO_WRITE);
	u.uio_segflg = UIO_USERSPACE;
	u.uio_space = curproc->p_addrspace;

	// write to vnode
	err = VOP_WRITE(open_file->vn, &u);
	if (err) {
		DEBUG(DB_SYSFILE,
			"Write error: Couldn't write to uio struct. err: %d\n",
			err);
<<<<<<< HEAD
		// TODO: enable synch again
		lock_release(open_file->lock); // we release the lock and return result
=======
		lock_release(open_file->lock);
>>>>>>> master
		return err;
	}

	// update offset
	open_file->offset = u.uio_offset;

<<<<<<< HEAD
	// TODO: enable synch again
=======
>>>>>>> master
	lock_release(open_file->lock);

	// return number of bytes read
	*retval = size - u.uio_resid;
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

	if (open_file == NULL)
	{
		return EBADF;
	}

	lock_acquire(open_file->lock);
	// we make sure that open file is not null
	// we check if fd is invalid
	if (fd < 0 || fd > OPEN_MAX)
	{
		lock_release(open_file->lock);
		return EINVAL;
	}

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

int sys_dup2(int oldfd, int newfd, int *retval)
{

	if (oldfd < 0 || oldfd >= OPEN_MAX || newfd < 0 || newfd >= OPEN_MAX)
	{
		return EBADF;
	}
	// If oldfd is a valid file descriptor, and newfd has the same value as oldfd, then dup2() does
	// nothing, and returns newfd.
	if (oldfd == newfd)
	{
		return newfd;
	}

	struct fhandle *old_file;
	struct fhandle *previous_file;
	old_file = curproc->p_fdtable[oldfd];

	if (old_file == NULL) // nothing to copy
	{
		return EBADF;
	}

	// If the descriptor newfd was previously open, it is silently closed before being reused.
	previous_file = curproc->p_fdtable[newfd];
	if (previous_file != NULL)
	{
		lock_acquire(previous_file->lock);

		previous_file->ref_count -= 1;
		if (previous_file->ref_count == 0)
		{
			vfs_close(previous_file->vn); // we close the previously open file
			lock_destroy(previous_file->lock);
		}
		lock_release(previous_file->lock);
		curproc->p_fdtable[newfd] = NULL; // we set it to null
		kfree(previous_file);
	}

	//asign to the new fd the old file
	curproc->p_fdtable[newfd] = old_file;
	lock_acquire(old_file->lock);
	old_file->ref_count += 1;
	lock_release(old_file->lock);
	*retval = newfd;

	return 0;
}

int sys_chdir(const char *pathname, int32_t *retval)
{
	char *path;
	size_t *path_length;

	if (pathname)
	{
		return EFAULT; // or ENOTDIR or EINVAL
	}

	if (strlen(pathname))
	{
		return EINVAL;
	}

	// We need to check if pathname is valid, we copy the string from userspace
	// to kernel and check ofr a valid address (already implemented in copyinout.c)
	KASSERT(curthread != NULL); // we make sure the thread is not null

	path = kmalloc(PATH_MAX);
	path_length = kmalloc(sizeof(int));

	int err = copyinstr((const_userptr_t)pathname, path, PATH_MAX, path_length);

	if (err)
	{
		kfree(path);
		kfree(path_length);
		return err;
	}
	kfree(path);
	kfree(path_length);

	int result = vfs_chdir((char *)pathname);

	if (result)
	{
		*retval = (int32_t)-1;
		return result;
	}

	*retval = (int32_t)0;
	return 0;
}

int
create_fhandle_struct(char* path, int flags, int mode, off_t offset, struct fhandle* retval)
{
	struct vnode *vn;
	int err;

	// initialize fhandle struct
	retval->vn = vn;
	retval->offset = offset;
	retval->flags = flags;
	retval->ref_count = 1;
	retval->lock = lock_create(path);

	// open file
	err = vfs_open(path, flags, mode, &vn);
	if (err) {
		return err;
	}

	return 0;
}

int
open_console(struct fhandle *fdtable[])
{
	int err;

	char con0[] = "con:";
	fdtable[0] = kmalloc(sizeof(struct fhandle));
	err = create_fhandle_struct(con0, O_RDONLY, 0664, 0, fdtable[0]);
	if (err) {
		DEBUG(DB_SYSFILE,
			"ConsoleIO error: couldn't open stdin. err: %d.\n",
			err);
		kfree(fdtable[0]);
		return err;
	}

	char con1[] = "con:";
	fdtable[1] = kmalloc(sizeof(struct fhandle));
	err = create_fhandle_struct(con1, O_WRONLY, 0664, 0, fdtable[1]);
	if (err) {
		DEBUG(DB_SYSFILE,
			"ConsoleIO error: couldn't open stdout. err: %d.\n",
			err);
		vfs_close(fdtable[0]->vn);
		kfree(fdtable[0]);
		kfree(fdtable[1]);
		return err;
	}

	char con2[] = "con:";
	fdtable[2] = kmalloc(sizeof(struct fhandle));
	err = create_fhandle_struct(con2, O_WRONLY, 0664, 0, fdtable[2]);
	if (err) {
		DEBUG(DB_SYSFILE,
			"ConsoleIO error: couldn't open stderr. err: %d.\n",
			err);
		vfs_close(fdtable[0]->vn);
		vfs_close(fdtable[1]->vn);
		kfree(fdtable[0]);
		kfree(fdtable[1]);
		kfree(fdtable[2]);
		return err;
	}

	return 0;
}

