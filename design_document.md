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

Added support for `SYS_OPEN, SYS_READ, SYS_LSEEK`.

```
void syscall(struct trapframe *tf) {}
```

## sys_open

`kern/syscall/file_syscalls.c`

open syscall handler.

```
int sys_open(userptr_t filename, int flags, int *retval);
```

## sys_read

`kern/syscall/file_syscalls.c`

read syscall handler.

```
int sys_read(int fd, userptr_t buf, size_t size, ssize_t *retval);
```

## sys_lseek

`kern/syscall/file_syscalls.c`

lseek syscall handler.

```
int sys_lseek(int fd, off_t pos, int whence, off_t *retval);
```


# TODOs
- add stdin, stdout and stderr to fdtable
- syscalls
  - write (Pablo)
  - close (Pablo)
  - dup2 (Pablo)
  - chdir (Pablo)
  - getcwd (Julian)
  - getpid
  - fork
  - execv
  - waitpid
  - exit
- tests
  - open (Pablo)
  - read (Pablo)
  - write (Julian)
  - close (Julian)
  - lseek (Pablo)
  - dup2 (Julian)
  - chdir (Julian)
  - getcwd (Pablo)
  - getpid
  - fork
  - execv
  - waitpid
  - exit
  - exit
