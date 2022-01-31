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
All of the process management related system calls are implemented in `proc_syscalls.c`. Similarly to what was implemented in `file_syscalls.c`, for properly management of processes and PIDs , its header file defines a struct `pidhandle` that helps to keep consistent information of processes and their important aspescts, such as PIDs and status. Using this structure, it was possible to get a good management of processes information, synchronization and consistency with multi-processes. One of the main objects for this part of the implementation is to have a safe life cycle for the process and synchronized information during all the running, and get a good functioning for program execution from the shell. 
It is important to notice that implementing synchronization principles was key to get a good result on the implementation of system calls, such as locks and conditional variables which are defined and described in file `synch.c`.

## File System Calls
#### Responsible: Julian
The file system calls can be divided into several categories. Most are related to interacting with files, such as `open`, `dup2`, `close`, `read`, `write`, and `lseek`. The remaining two, `__getcwd` and `chdir`, are instead used to read and manipulate the current working directory.
The CWD is defined per-process and describes the location in the file system from which a process operates. This dictates for example the relative paths of other files to the process. It is stored as a `vnode` struct in the `proc` struct. `__getcwd` can be used to read it and `chdir` can set it to a new directory.
If a process is interacting with a file, then the 
If a process wants to access a file, it can not do so directly, but it must first open that file by calling the `open` system call. This system call returns an integer, the file descriptor, to the calling process. Now whenever the process wants to perform an action on this file, it can identify the file to the kernel using the file descriptor. This file descriptor can be duplicated using the `dup2` system call, and closed with the `close` system call, ending the interaction.
Internally the file descriptor points to a file handle, a `fhandle` struct. File handles track meta data about a process interacting with a file. It contains information directly related to the interaction, such as the `vnode` describing the file, the current offset into the file, and access permissions in the form of flags. But it also also holds information related to ensuring proper management, such as a lock to synchronize file access and a reference counter to know, when the file interaction has ended.
All file handles associated with a process are stored in the file table, an array of `fhandle`s called `p_fdtable` that is stored in the `proc` struct. Usually file handles are modified implicitly such as during syscalls `read` or `write`, to keep their state up to date. But they can also be changed explicitly with syscalls such as `lseek` or `dup2`.
Lastly there are a few special file descriptors, which have a special meaning by convention. The first three entries of the file table correspond to stdin, stdout and stderr and are generally available to a process without specifically having to open them. To achieve this we open them in the first user process that is being created and pass them down to other processes in the fork system call. This is achieved in the `runprogram` function.

**Overall responsibility:** Julian Neubart

## Process System Calls
#### Responsible (except execv): Pablo
#### Responsible (execv only): Julian

The process system calls has a strong dependency with the structs defined for handling processes inside `proc.h`. These system calls are mainly related to process synchronization in cycle of life and process execution. Firstly, for synchronization we defined system calls such as `getpid`, `waitpid`, `fork` and `exit`. On the other hand, `execv` is not as closely related to synchronization.

`execv` is a bit of a special syscall, as it is quite complex. It comes from userspace, enters kernel space to handle the syscall and returns back into userspace. While handling the syscall it also fundamentally changes the calling process by assigning it a completely new address space. It thus also changes any variables and even the code the user process executes. It is provided with a program name and an argument vector by the user. The new address space and executable are defined in almost the exact same way as runprogram. The interesting part from an implementation point of view is instead how the argument vector is passed to the userprogram after it has received a fresh address space. The answer is in the only place of the new address space the user knows, the stack.
By copying the program name and all of the user arguments onto the new stack, the user can easily access them. As this operation is quite complex we decided to already allocate the memory in kernel space and carefully copy all of the data and offsets carefully into this kernel buffer. From low to high memory locations it first contains the relative offset for each argument and then the data itself. It is important to pay attention to proper alignment here as pointers need to be aligned by 4 and the stack needs to be aligned by 8 (largest possible value on stack). Doing it this way we can simply update the offset by the actual pointers once the stack is defined and just copy the whole structure back into userspace, making this part very simple.

In order to implement the latter system calls, it was necessary to create a new structure that took care of processes and related them with their pids. In this way, inside file `proc.h` is defined a new structure called `pidhandle` and some auxiliar methods to manage it. The new structure works as a PID table that keeps the information about processes like exit status, running status and the list of process with their corresponding PIDs. The assignment of the processor's id starts with `proc_create` where the kernel process gets `pid = 1`.
More specifically, the structure `pidhandle` has 8 arguments, and each of this help to maintain the information of processes, such as status of process, exit codes, a list of processes, among others. There are several functions implemented inside `proc.c` that intiate and control the pidhandle structure, such as `pidhandle_bootstrap` that is called once the kernel is initiated, `pidhandle_add` that adds a new process into the pidhandle. Further details of these methods are described in the following sections. 

Almost all process system calls use the structure created, since it was simpler to obtain pids from a simulated table and also easier to manage. In this way, `getpid` returns the pid of the actual process thanks to the new field in the proc structure, and `waitpid`, `exit` and `fork` interact in a more direct way with `pidtable`.


## Synchronization

#### Responsible synchronization: Pablo
The implementation of synchronization is something that's cleary useful for this project, for example for system calls `read` and `write`. For this matter, we implemented locks and conditional variables using `waitchannels` and `spinlocks` that were already implemented in OS161.


## Testing
### Responsible: Julian
Since OS161 has a huge library of user programs we decided to try and keep things as simple as possible by utilizing this library as much as possible. Many of these programs represent very simple programs that could be actually used by an end user. They are thus complex end to end tests that often call several different syscalls and usually only provide valid parameters. In this section we discuss how we choose a subset of this testing library to verify the correctness of our implementation as thoroughly as possible. In addition to the provided programs we implemented a number of small tests ourselves to cover otherwised missed functionality.
An essential property of any system call is, that the system should never crash. Even if the user provided invalid arguments. The robustness of the system can be verified using `badcall` which tests for edge cases and `randcall` which calls random syscalls with random arguments. Even if some functions are not yet implemented, the kernel should never crash.
The first most basic test we performed was `palin` to show that `write` can properly print to stdout as this is essential for almost every other test. A slightly more complicated test we performed, is the `conman` test, which utilizes both stdin and stdout. Finally `bigseek` was the big test for our file syscalls. This program uses both console I/O as well as file I/O, and the `seek` syscall, covering almost all of the basic file syscalls. `close` and `dup2` were tested together in a simple self-written testcase `testclosedup`, as well as `__getcwd` and `chdir` in `testwdir`, rounding off our testing of the files syscalls.
To test execv we wrote a simple program `testexecv` which calls `argtest` with a specified set of arguments. `getpid`, `waitpid` and `fork` were finally all tested together by calling forktest. For `exit` we tried simple calls to the system call with tests `false` and `true` inside `bin` tests.

## Bugs and TODOs

During the development of the project, we dealed with some issues that we could not solve entirely. 
We dealed with some issues respecting to fork and maintainence of pid when exiting a process after a fork failure, so we decided to define a new `option` called `OPT_FORK`. In this way, when it is set to `1` , tests like `forktest`, `bigfork` and `forkbomb` work partially due to not having enough memory or to this bug presented, that's basically trying to exit a process with a non-existing `pid`, since `proc_destroy` is runned first than the function that manages "freeing" of `pidhandle`.

