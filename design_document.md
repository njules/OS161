# Project C2: SHELL
#### Members of the group: 
- Julian Neubert (S288423)
- Pablo Andres Vejar Gomez (S291761)

#### Date of delivery: 31/01/2022

## Project Summary
The main purpose of this project is to support running multiple processes at once from actual compiled programs stored on disk. These programs will be loaded into OS161 and executed in user mode, under the control of your kernel and the command shell in bin/sh (menu command: p bin/sh). 
In order to achieve this, as a team, we implemented several system calls to support process and open file management inside our kernel. This also required the creation and management of new data structures to store information about the system process state.
We added the `OPT_SHELL` option to neatly encapsulate any additions made to the codebase of OS161 during the implementation of our project. This way OS161 can easily be reverted to it's original state and run without the process and file syscall implemented. Two files are being optionally included based on the value of `OPT_SHELL`.
`file_syscalls.c` contains the main part of the code to support all file-related system calls. Its header file, `file_syscalls.h`, additionally defines a struct `fhandle`, which stores information related to open files that are being used by a process. A big part of our implementation of these system calls is to make sure that any change of information is correctly reflected in the state managing structures. Most of the actual interaction with the file system is handled by another abstraction layer defined in `vfs.h`. If all information required to interact with a file has been properly saved, the logic of these system calls is pretty straight forward. One important thing to note is that each system call will carefully need to verify any user supplied arguments and move data between user and kernel spaces in a safe way.
All of the process management related system calls are implemented in `proc_syscalls.c`. [more][synch?]

## File System Calls
The file system calls can be divided into several categories. Most are related to interacting with files, such as `open`, `dup2`, `close`, `read`, `write`, and `lseek`. The remaining two, `__getcwd` and `chdir`, are instead used to read and manipulate the current working directory.
The CWD is defined per-process and describes the location in the file system from which a process operates. This dictates for example the relative paths of other files to the process. It is stored as a `vnode` struct in the `proc` struct. `__getcwd` can be used to read it and `chdir` can set it to a new directory.
If a process is interacting with a file, then the 
If a process wants to access a file, it can not do so directly, but it must first open that file by calling the `open` system call. This system call returns an integer, the file descriptor, to the calling process. Now whenever the process wants to perform an action on this file, it can identify the file to the kernel using the file descriptor. This file descriptor can be duplicated using the `dup2` system call, and closed with the `close` system call, ending the interaction.
Internally the file descriptor points to a file handle, a `fhandle` struct. File handles track meta data about a process interacting with a file. It contains information directly related to the interaction, such as the `vnode` describing the file, the current offset into the file, and access permissions in the form of flags. But it also also holds information related to ensuring proper management, such as a lock to synchronize file access and a reference counter to know, when the file interaction has ended.
All file handles associated with a process are stored in the file table, an array of `fhandle`s called `p_fdtable` that is stored in the `proc` struct. Usually file handles are modified implicitly such as during syscalls `read` or `write`, to keep their state up to date. But they can also be changed explicitly with syscalls such as `lseek` or `dup2`.
Lastly there are a few special file descriptors, which have a special meaning by convention. The first three entries of the file table correspond to stdin, stdout and stderr and are generally available to a process without specifically having to open them. To achieve this we open them in the first user process that is being created and pass them down to other processes in the fork system call. This is achieved in the `runprogram` function.

## Process System Calls

## Testing

## Bugs and TODOs

-----------
First, in order to support in a good manner file system calls, we created a new opt-file called `file_syscalls.c` that contains all the file-related system calls and some auxiliar functions that help the process. Inside the header file, we also defined a new structure called `fhandle` that works as a file table for our system where values `fid = 0`, `fid =1` and `fid=2`are destined to `stdin`, `stdout` and `stderr` respectively. As detailed more forward in this report, we defined the following system calls: `sys_open`, `sys_close`, `sys_read`, `sys_write`, `sys___getcwd`, `sys_chdir` and `sys_dup2`. [elaborate more]
Secondly, we implemented support for processes and their corresponding system calls, and for this part we created a new opt-file called `proc_syscalls.c` and defined new auxiliar methods to help the management of the new structure `pidhandle` defined in `proc.h`. Inside these files we have a simulation of a PID table that keeps the information about processes like exit status, running status and a list of process with their corresponding PID's, and as defined, `pid = 1 ` is reserved for the kernel process. For this part, we decided to keep also track of the children of processes. As written in the further sections, we defined the following system calls: `sys_getpid`, `sys_waitpid`, `sys_execv`, `sys_fork` and `sys_exit`. 
During this last part, we dealed with some issues respecting to fork and maintainence of process information when exiting a process after a fork failure, so we decided to define a new `option` called `OPT_FORK`, even though tests run with few errors. With respect to the other syscalls in the process side, they are working correctly and with some minor issues.
Lastly, we implemented synchronization for mainly obvious reasons such as synchronization when reading and writing. For this matter, we implemented locks and conditional variables using `waitchannels` and `spinlocks` that were already implemented in OS161. [maybe finish up with some conclusions]
--------------

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
- called after every program [i'm not sure that this is true]
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
- forkbomb
- chain of processes (waitpid)
- kitchen


## Conclusions
[to fill]








# TODOs
- cwd is per thread but fdtable is per process, fix inconsistency?
- check if synchronization is done properly for file syscalls
- check if there are problems with definition of retvals
- syscalls
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
  - __getcwd (Pablo)
  - getpid
  - fork
  - execv
  - waitpid
  - exit
