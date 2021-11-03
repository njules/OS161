# Structs

## proc

`kern/include/proc.h`

Preexisting process structure.

Added `p_fdtable`
`p_fdtable` is an array of length `OPEN_MAX`.
It is indexed by the file descriptor `fd`, which returned after opening a file.
`p_fdtable` stores file handles `fhandle`.
It is initialized in `static struct proc *proc_create(const char *name) {}` in `kern/proc/proc.c`.

```
struct proc {
	char *p_name;			/* Name of this process */
	struct spinlock p_lock;		/* Lock for this structure */
	unsigned p_numthreads;		/* Number of threads in this process */

	/* VM */
	struct addrspace *p_addrspace;	/* virtual address space */

	/* VFS */
	struct vnode *p_cwd;		/* current working directory */

	/* add more material here as needed */
#if OPT_FILESC
	struct fhandle *p_fdtable[OPEN_MAX];  // file table
#endif
};
```

## fhandle

`kern/include/file_syscalls.h`

File handle structure.

```
struct fhandle {
	struct vnode *vn;
	off_t offset;
	int flags;
	unsigned int ref_count;
	struct lock *lock;
};
```


# Methods

## syscall

`kern/arch/mips/syscall/syscall.c`

Preexisting method to dispatch syscalls.

Added support for `SYS_READ`.

```
void syscall(struct trapframe *tf) {}
```

## sys_read

`kern/syscall/file_syscalls.c`

Read syscall handler.

```
int sys_read(int fd, userptr_t buf_ptr, size_t size) {}
```


# TODOs
- add stdin, stdout and stderr to fdtable
- syscalls
  - open
  - write
  - close
  - lseek
  - dup2
  - chdir
  - getcwd
  - getpid
  - fork
  - execv
  - waitpid
  - exit
- tests
  - open
  - read
  - write
  - close
  - lseek
  - dup2
  - chdir
  - getcwd
  - getpid
  - fork
  - execv
  - waitpid
  - exit
