#include <proc_syscalls.h> // prototype for this file
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
int sys_getpid(int *retval){
    lock_acquire(pidhandle->pid_lock);

    *retval = curproc->pid;
    

    lock_release(pidhandle->pid_lock);
    return 0;
} 

int sys_waitpid(pid_t pid, int *retval, int options){

    struct proc *child;
    bool isachild = false;
    int childrennum;
    int exitcode;
    /* Our implementation does not allow another option that's different from 0*/
    if (options){
        return EINVAL;
    }

    /*Only allow values for PID that are between the minimum and maximum*/
    if (pid < PID_MIN || pid > PID_MAX || pidhandle->pid_status[pid] == (int)NULL){
        return EINVAL;
    }

    /*Check if actual pid is child of the current process */
    child = pidhandle->pid_proc[pid];
    childrennum = array_num(curproc->children);
    for(int i = 0; i< childrennum; i++){
        struct proc *tmpChild = array_get(curproc->children, i);
        if (child == tmpChild){
            isachild = true;
            break;
        }
    }
    /* It is not a child process*/
    if (!isachild){
        return ECHILD;
    } 

    lock_acquire(pidhandle->pid_lock);
    

    while(pidhandle->pid_status[pid] != ZOMBIE_STATUS){
        cv_wait(pidhandle->pid_cv, pidhandle->pid_lock);
    }

    exitcode = pidhandle->pid_exitcode[pid];

    lock_release(pidhandle->pid_lock);

    if (retval){
        /*  copyout copies LEN bytes from a kernel-space address SRC to a
 * user-space address USERDEST.*/
        int res = copyout(&exitcode, (userptr_t) retval, sizeof(int));
        if (res){
            return res;
        }
    }
    return 0;
}

void sys__exit(int exitcode){

	process_exit(curproc, exitcode);
	thread_exit();
    panic("Exit syscall should never get to this point.");

}
