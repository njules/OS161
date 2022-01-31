# Structs

## proc

`kern/include/proc.h`

Preexisting process structure.

Added `p_fdtable`.
`p_fdtable` is an array of length `OPEN_MAX`.
It is indexed by the file descriptor `fd`, which returned after opening a file.
`p_fdtable` stores file handles `fhandle`.
It is initialized in `static struct proc *proc_create(const char *name);` in `kern/proc/proc.c`.
`stdin`, `stdout`, and `stderr` are opened in `int runprogram(char *progname);` and added to `p_fdtable`.
Added `pid`
`pid` is a pid_t type argument that represents the id of the process with a maximum value defined of `MAX_RUNNING_PROCS` equal to `250` and a minimum of 1..
Added `children`, which corresponds to al of the childrend of the actual process.

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
#if OPT_SHELL
	struct fhandle *p_fdtable[OPEN_MAX];  // file table
  	pid_t pid;
	struct array *children;
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

## pdhandle 

`kern/include/proc.h`

- PID handle structure: In charge of keeping all the information about processes, their pids and exit codes. It also contains a lock and a conditional variable variable to make syncrhonization factible.

```
struct pidhandle
{
	struct lock *pid_lock;
	struct cv *pid_cv;
	struct proc *pid_proc[PID_MAX + 1];
	int qty_available; 
	int next_pid;
};

``` 

# Methods

## syscall

`kern/arch/mips/syscall/syscall.c`

Preexisting method to dispatch syscalls.

Added support for `SYS_open, SYS_read, SYS_lseek, SYS___getcwd, SYS_chdir, SYS_close, SYS_dup2, SYS_write, SYS_waitpid, SYS_getpid, SYS__exit` and partial support for `SYS_fork`.

```
void syscall(struct trapframe *tf);
```

## sys_open

`kern/syscall/file_syscalls.c`

open syscall handler.

```
int sys_open(userptr_t filename, int flags, int *retval);
```

## sys_close

`kern/syscall/file_syscalls.c`

close syscall handler.

```
int sys_close(int filehandler);
```

## sys_read

`kern/syscall/file_syscalls.c`

read syscall handler.

```
int sys_read(int fd, userptr_t buf, size_t size, ssize_t *retval);
```

## sys_write

`kern/syscall/file_syscalls.c`

write syscall handler.

```
int sys_write(int fd, userptr_t buf, size_t size, ssize_t *retval);
```
## sys_lseek

`kern/syscall/file_syscalls.c`

lseek syscall handler.

```
int sys_lseek(int fd, off_t pos, int whence, off_t *retval);
```

## sys___getcwd

`kern/syscall/file_syscalls.c`

__getcwd syscall handler.

```
int sys___getcwd(userptr_t buf, size_t buflen, int *retval);
```

## sys_dup2

`kern/syscall/file_syscalls.c`

dup2 syscall handler.

```
int sys_dup2(int oldfd, int newfd, int *retval);
```

## sys_chdir

`kern/syscall/file_syscalls.c`

chdir syscall handler.

```
int sys_chdir(const char *path, int32_t *retval);
```

## sys_getpid

`kern/syscall/proc_syscalls.c`

getpid syscall handler.

```
int sys_getpid(int *retval);
```

## sys_waitpid

`kern/syscall/proc_syscalls.c`

waitpid syscall handler.

```
int sys_waitpid(pid_t pid, int *retval, int options);
```

## sys_execv

`kern/syscall/proc_syscalls.c`

execv syscall handler.

```
int sys_execv(userptr_t program, userptr_t args);
```

## sys_fork

`kern/syscall/proc_syscalls.c`

fork syscall handler.

```
int sys_fork(struct trapframe *, int *retval );
```

## sys__exit

`kern/syscall/proc_syscalls.c`

exit syscall handler.

```
void sys__exit(int exitcode);
```

## pidhandle_bootstrap

`kern/proc/proc.c`

initialize pidhandle structure.

```
void pidhandle_bootstrap(void);
```

## get_proc_pid

`kern/proc/proc.c`

get process associated with pid.

```
struct proc *get_proc_pid(pid_t);
```

## pidhandle_add

`kern/proc/proc.c`

When a new process is added, it updates the pid table handle and updates children list.

```
int pidhandle_add(struct proc *, int32_t *);
```

## pidhandle_free_pid

`kern/proc/proc.c`

frees a given pid from the pid handle table

```
int pidhandle_free_pid(pid_t);
```

# Options

## shell

Enable file syscalls and process syscalls.

```
optfile	   shell	syscall/file_syscalls.c
optfile	   shell	syscall/proc_syscalls.c
```

## synch

Enable locks and condition variables.

```
optfile	   synch	thread/synch.c
```

## fork

Enable fork syscall and process managing.

```
optfile	   fork	
```


# Tests

### fork
- forktest (forks several times)
- forkbomb
- bigfork
Enable option fork for these tests.

### execv
- testexecv (calls argtest with args)
- argtest (can be called to print passed args)

### exit
- called after every program
- TODO: test exitcodes?

### waitpid
- forktest (waits for all forked processes to end)
- bad_waitpid

### getpid
- getpidtest
- forktest (verify fork returned pid)
- forkbomb

### open
- sink, conman, palin (console IO)
- bigseek (open file)

### dup2
- testdup2

### close
- TODO: test with dup2?

### read
- sink (read from console)
- conman (read from console + write)
- bigseek(read from file)

### write
- palin (write to console)
- conman (write to console + read)
- bigseek(write to file)

### lseek
- bigseek (seeks various positions in file, also large (signed vs unsigned) and longs)

### chdir
- testwdir (changes and reads dir)
- Command `cd` from kernel

### _getcwd
- testwdir (changes and reads dir)
- Command `pwd` from kernel

### all
- badcall (test invalid parameters for all syscalls)
- randcall (calls syscalls with random (invalid) parameters)

### ideas
- bigfile
- bigexec
- crash
- factorial
- farm
- faulter (no panic on invalid pointer cast) TODO: where?
- chain of processes (waitpid)
- kitchen

# TODOs
- syscalls
  - getpid
  - fork
  - waitpid
  - exit
