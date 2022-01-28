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
#include <addrspace.h>
    

/*
Gets PID of the current process.
*/
int sys_getpid(int *retval){
    lock_acquire(pidhandle->pid_lock);

    *retval = curproc->pid;
    

    lock_release(pidhandle->pid_lock);
    return 0;
} 

/* 
Function to wait for the exit of a child process 
*/
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

    if (retval != NULL){
        /*  copyout copies LEN bytes from a kernel-space address SRC to a
 * user-space address USERDEST.*/
        int res = copyout(&exitcode, (userptr_t) retval, sizeof(int));
        if (res){
            return res;
        }
    }
    return 0;
}
/*
Function that child first enters, in charge of setting trapframes
*/
void child_forkentry(void *data1, unsidgned long data2){
    (void) data2;

    /* We load the address space into child's thread addrespace, so we enlarge the stack*/
    /* of a total of 16 bytes, since every trapfram is 4 bytes*/
    void *tf = (void *) curthread->t_stack + 16;
    memcpy(tf, (const void *) data1, sizeof(struct trapframe));

    /* We activate the new address space */
    as_activate();
    kfree((struct trapframe *) data1);
    /* We return to user mode */
    mips_usermode(tf);
    
}

/* 
Function to fork current process and create a child 
*/
int sys_fork(struct trapframe *tf, int *retval ){

    struct proc *new_proc; 
    struct trapframe *new_tf;
    int res;

    res = handle_proc_fork(&new_proc, "new_child_process");
    /* If there's an error, return error */
    if (res) {
        return res;
    }

    new_tf = kmalloc(sizeof(struct trapframe));
	if (new_tf == NULL) {
		return ENOMEM;
	}
    // we store the copy of the trampfram on a kernel heap and set to 0 all trapframes
	memcpy((void *) new_tf, (const void *) tf, sizeof(struct trapframe));
	(new_tf)->tf_v0 = 0;
	(new_tf)->tf_v1 = 0;  /* This should only be done if retval is 64 bit, so we leave just in case*/
	(new_tf)->tf_a3 = 0; 
    (new_tf)->tf_epc += 4; /* This will avoid child to keep calling fork*/ 

    /* We return the pid to the parent process */
    *retval = new_proc->pid;
    /* The two arguments that child_forkentry receives are data1 and data 2
    but sinces fork doesn't take any arguments (more than trapframe and retval) we pass 0
    */
    res = thread_fork("new_child_thread", new_proc, child_forkentry, new_tf, 0);

    if (res) {
		proc_destroy(new_proc);
		pidtable_freepid(new_proc->pid);
		kfree(new_tf);
		return res;
	}

    return 0;

}

/* 
Exits the current process 
*/
void sys__exit(int exitcode){

	process_exit(curproc, exitcode);
	thread_exit();
    panic("Exit syscall should never get to this point.");

}
