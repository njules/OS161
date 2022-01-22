#include "proc_syscalls.h" // prototype for this file
#include <kern/errno.h>    // Errors (EBADF, EFAULT, ..)
#include <types.h>        
#include <limits.h>        // OPEN_MAX
#include <current.h>       // curproc
#include <proc.h>          // proc struct (for curproc)
#include <kern/fcntl.h>    
#include <uio.h>           
#include <vnode.h>         
#include <vfs.h>          
#include <copyinout.h>    
#include <kern/seek.h>     
#include <kern/stat.h>     

/*
Gets PID of the current process.
*/
int sys_getpid(int32_t *retval){
    lock_acquire(pidhandle->pid_lock);

    *retval = curproc->pid;

    lock_release(pidhandle->pid_lock);
    return 0;
} 