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


int 
sys_open(userptr_t filename, int flags, int *retval)
{
	int fd;
	char path[PATH_MAX + 1];
	size_t pathlen;
	//off_t offset;
	struct fhandle *open_file;

	DEBUG(DB_SYSCALL,
		  "Open syscall invoked, filename buf: %p, flags: 0x%x.\n",
		  filename, flags);

	// check flags are legal combination
	if (flags & O_WRONLY){
		if (flags & !(O_WRONLY | O_CREAT | O_EXCL | O_TRUNC | O_APPEND)) {
			DEBUG(DB_SYSFILE, "Open error: Invalid flags. flags: 0x%x.\n", flags);
			return EINVAL;
		}
	} else if (flags & O_RDWR) {
		if (flags & !(O_RDWR | O_CREAT | O_EXCL | O_TRUNC)) {
			DEBUG(DB_SYSFILE, "Open error: Invalid flags. flags: 0x%x.\n", flags);
			return EINVAL;
		}
	} else { // O_RDONLY
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

	// add file handle to fdtable
	curproc->p_fdtable[fd] = open_file;


	*retval = fd;
	return 0;
}

int
sys_dup2(int oldfd, int newfd, int *retval)
{
	struct fhandle *old_fhandle;
	struct fhandle *new_fhandle;

	DEBUG(DB_SYSCALL,
		"Dup2 syscall invoked, oldfd: %d, newfd: %d.\n",
		oldfd, newfd);

	if (oldfd < 0 || oldfd >= OPEN_MAX || newfd < 0 || newfd >= OPEN_MAX) {
		DEBUG(DB_SYSFILE,
			  "Dup2 error: File descriptor out of bounds. oldfd: %d, newfd: %d.\n",
			  oldfd, newfd);
		return EBADF;
	}

	old_fhandle = curproc->p_fdtable[oldfd];
	if (old_fhandle == NULL) {
		DEBUG(DB_SYSFILE,
			  "Dup2 error: Invalid fd. oldfd: %d.\n",
			  oldfd);
		return EBADF;
	}

	if (oldfd == newfd) {
		return newfd;
	}

	// If the descriptor newfd was previously open, it is silently closed before being reused.
	new_fhandle = curproc->p_fdtable[newfd];
	if (new_fhandle != NULL) {
		lock_acquire(new_fhandle->lock);
		DEBUG(DB_SYSFILE, "Closing new file handle. fd: %d.\n", newfd);
		curproc->p_fdtable[newfd] = NULL;
		destroy_fhandle_struct(new_fhandle);
	}

	//asign to the new fd the old file
	curproc->p_fdtable[newfd] = old_fhandle;
	lock_acquire(old_fhandle->lock);
	old_fhandle->ref_count += 1;
	lock_release(old_fhandle->lock);

	*retval = newfd;
	return 0;
}

int
sys_close(int fd)
{
	struct fhandle *open_file;

	DEBUG(DB_SYSCALL,
		  "Close syscall invoked, fd:%d.\n",
		  fd);

	if (fd < 0 || fd > OPEN_MAX) {
		DEBUG(DB_SYSFILE,
			  "Close error: File descriptor out of bounds. fd: %d.\n",
			  fd);
		return EINVAL;
	}

	// get file handle from file table
	open_file = curproc->p_fdtable[fd];

	// check that fd points to valid file handle
	if (open_file == NULL) {
		DEBUG(DB_SYSFILE,
			  "Close error: fd points to invalid p_fdtable entry. fd: %d.\n",
			  fd);
		return EBADF;
	}

	// synchronize access to file handle
	lock_acquire(open_file->lock);

	open_file->ref_count -= 1;
	if (open_file->ref_count == 0) {
		DEBUG(DB_SYSFILE, "Closing file handle. fd: %d.\n", fd);
		curproc->p_fdtable[fd] = NULL;
		destroy_fhandle_struct(open_file);
		return 0;
	}

	lock_release(open_file->lock);
	return 0;
}

int sys_read(int fd, userptr_t buf, size_t size, ssize_t *retval)
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

	// synchronize access to file handle during read
	lock_acquire(open_file->lock);

	// check that flags allow reading from file
	if (open_file->flags & O_WRONLY) {
		DEBUG(DB_SYSFILE,
			  "Read error: Flags do not allow file to be read from."
			  " fd: %d, flags: 0x%x.\n",
			  fd, open_file->flags);
		return EBADF;
	}

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
	if (fd < 0 || fd >= OPEN_MAX)
	{
		DEBUG(DB_SYSFILE,
			  "Write error: File descriptor out of bounds. fd: %d.\n",
			  fd);
		return EBADF;
	}

	// get file handle from file table
	open_file = curproc->p_fdtable[fd];

	// check that fd points to valid file handle
	if (open_file == NULL)
	{
		DEBUG(DB_SYSFILE,
			  "Write error: fd points to invalid p_fdtable entry. fd: %d.\n",
			  fd);
		return EBADF;
	}

	// check that flags allow writing to file
	if (open_file->flags & O_RDONLY)
	{
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
	if (err)
	{
		DEBUG(DB_SYSFILE,
			  "Write error: Couldn't write to uio struct. err: %d\n",
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

int
sys_chdir(userptr_t pathname, int32_t *retval)
{
	char *kpath;
	size_t pathlen;

	DEBUG(DB_SYSCALL,
		  "Chdir syscall invoked, buf: %p.\n",
		  pathname);

	kpath = kmalloc(PATH_MAX);
	int err = copyinstr(pathname, kpath, PATH_MAX, &pathlen);
	if (err) {
		DEBUG(DB_SYSFILE,
			  "Chdir error: Couldn't read pathname. err: %d\n",
			  err);
		kfree(kpath);
		return err;
	}

	DEBUG(DB_SYSFILE, "Changing dir to: %s\n", kpath);

	err = vfs_chdir(kpath);
	if (err) {
		DEBUG(DB_SYSFILE,
			  "Chdir error: Couldn't change directory. err: %d\n",
			  err);
		kfree(kpath);
		return err;
	}
	
	*retval = 0;
	return 0;
}

int
sys___getcwd(userptr_t buf, size_t buflen, int *retval)
{
	// set up iovec and uio structs to copy cwd
	struct iovec iov;
	struct uio u;

	DEBUG(DB_SYSCALL,
		  "Getcwd syscall invoked, buf: %p, size: %d.\n",
		  buf, buflen);

	uio_kinit(&iov, &u, buf, buflen, 0, UIO_READ);
	u.uio_segflg = UIO_USERSPACE;
	u.uio_space = curproc->p_addrspace;

	int err = vfs_getcwd(&u);
	if (err) {
		DEBUG(DB_SYSFILE,
			  "Getcwd error: Couldn't read cwd. err: %d\n",
			  err);
		return err;
	}

	*retval = 0; // on success return 0
	return 0;
}

int
create_fhandle_struct(char *path, int flags, int mode, off_t offset, struct fhandle *retval)
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
	if (err)
	{
		return err;
	}

	return 0;
}

void
destroy_fhandle_struct(struct fhandle *open_file)
{
	vfs_close(open_file->vn);
	lock_destroy(open_file->lock);
	kfree(open_file);
	return;
}

int
open_console(struct fhandle *fdtable[])
{
	int err;

	char con0[] = "con:";
	fdtable[0] = kmalloc(sizeof(struct fhandle));
	err = create_fhandle_struct(con0, O_RDONLY, 0664, 0, fdtable[0]);
	if (err)
	{
		DEBUG(DB_SYSFILE,
			  "ConsoleIO error: couldn't open stdin. err: %d.\n",
			  err);
		kfree(fdtable[0]);
		return err;
	}

	char con1[] = "con:";
	fdtable[1] = kmalloc(sizeof(struct fhandle));
	err = create_fhandle_struct(con1, O_WRONLY, 0664, 0, fdtable[1]);
	if (err)
	{
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
	if (err)
	{
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
