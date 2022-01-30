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
#include <lib.h>
#include <addrspace.h>
#include <syscall.h>  

#define ALIGN_POINTER 4
#define ALIGN_STACK 8 

/*
Gets PID of the current process.
*/
int sys_getpid(int *retval){

    *retval = curproc->pid;

    return 0;
}


int
sys_execv(userptr_t program, userptr_t args)
{
	int err;
	char *kprogram;
	int buflen;
	int argc;
	void *kargs;
	int kargslen;
	int argsoffset;
	struct vnode *v;
	struct addrspace *as;
	vaddr_t entrypoint, stackptr;
	userptr_t uargs;

	DEBUG(DB_SYSCALL,
		  "Execv syscall invoked, program: %p, args: %p.\n",
		  program, args);

	/* compute number of arguments and required space */
	kargslen = 0;
	for (argc=0; ((char**) args)[argc]; argc++) {
		buflen = strlen(((char**) args)[argc]) + 1;
		kargslen += buflen;
	}
	argc += 1;  // for program

	/* copy program into kprogram and reserve room in kargs */
	buflen = strlen((char*) program) + 1;
	kargslen += buflen;
	kprogram = kmalloc(buflen);
	err = copyin(program, kprogram, buflen);
	if (err) {
		DEBUG(DB_SYSEXECV,
			"Execv error: Couldn't copyin program name. err: %d.\n",
			err);
		return err;
	}
	DEBUG(DB_SYSEXECV, "Execv: Got program: \"%s\".\n", kprogram);

	/* reserve room for pointers, align and allocate */
	kargslen = align(kargslen, ALIGN_POINTER);
	kargslen += (argc + 1) * ALIGN_POINTER;
	kargslen = align(kargslen, ALIGN_STACK);
	kargs = kmalloc(kargslen);
	if (!kargs) {
		kfree(kprogram);
		return ENOMEM;
	}
	DEBUG(DB_SYSEXECV,
		"Execv: Preparing to copyin %d args into kargs buffer of size 0x%x.\n",
		argc, kargslen);

	/* copy kprogram into kargs buffer */
	argsoffset = (argc + 1) * ALIGN_POINTER;
	((char**) kargs)[0] = (char*) argsoffset;
	strcpy(kargs + argsoffset, kprogram);
	argsoffset += strlen(kprogram) + 1;

	/* copy args into kargs buffer */
	for (int i=1; i<argc; i++) {
		((char**) kargs)[i] = (char*) argsoffset;
		DEBUG(DB_SYSEXECV,
			"Execv:  Copying arg %d into offset %p\n",
			i, (char*) argsoffset);
		err = copyinstr(
			(userptr_t) ((char**) args)[i-1],
			kargs + argsoffset,
			kargslen - argsoffset,
			(size_t*) &buflen);
		if (err) {
			DEBUG(DB_SYSEXECV,
				"Execv error: Couldn't copyin user argument."
				" err: %d, argidx: %d, arg: \"%s\".\n",
				err, i-1, ((char**) args)[i-1]);
			kfree(kprogram);
			kfree(kargs);
			return err;
		}
		DEBUG(DB_SYSEXECV, "Execv:  Copied arg %d: %s\n", i-1, (char*) kargs + argsoffset);
		argsoffset += buflen;
	}
	((char**) kargs)[argc] = NULL;
	DEBUG(DB_SYSEXECV, "Execv: Copied %d args into kernel buffer.\n", argc-1);

	/* open executable */
	err = vfs_open(kprogram, O_RDONLY, 0, &v);
	if (err) {
		DEBUG(DB_SYSEXECV,
			"Execv error: Couldn't open executable \"%s\". err: %d.\n",
			kprogram, err);
		kfree(kprogram);
		kfree(kargs);
		return err;
	}
	DEBUG(DB_SYSEXECV, "Execv: opened executable %s.\n", kprogram);
	kfree(kprogram);

	/* destroy old address space and switch to new one */
	as = proc_setas(NULL);
	as_deactivate();
	as_destroy(as);
	as = as_create();
	if (as == NULL) {
		DEBUG(DB_SYSEXECV,
			"Execv error: Couldn't create new address space.\n");
		kfree(kargs);
		vfs_close(v);
		return ENOMEM;
	}
	proc_setas(as);
	as_activate();
	DEBUG(DB_SYSEXECV, "Execv: Set new address space.\n");

	/* Load the executable. */
	err = load_elf(v, &entrypoint);
	if (err) {
		DEBUG(DB_SYSEXECV,
			"Execv error: Couldn't load executable. err: %d.\n",
			err);
		kfree(kargs);
		vfs_close(v);
		return err;
	}

	/* Done with the file now. */
	vfs_close(v);
	DEBUG(DB_SYSEXECV, "Execv: Loaded executable.\n");

	/* Define the user stack in the address space */
	err = as_define_stack(as, &stackptr);
	if (err) {
		DEBUG(DB_SYSEXECV, "Execv error: Couldn't define stack. err: %d.\n", err);
		kfree(kargs);
		return err;
	}
	DEBUG(DB_SYSEXECV, "Execv: Defined stack.\n");

	/* compute pointers on stack for kargs before copyout */
	uargs = (userptr_t) stackptr - kargslen;
	DEBUG(DB_SYSEXECV, "Execv: uargs starts at %p\n", uargs);
	for (int i=0; i<argc; i++) {
		DEBUG(DB_SYSEXECV,
			"Execv:  Arg %d offset %p moves to %p\n",
			i, ((char**) kargs)[i], ((char**) kargs)[i] + (int) uargs);
		((char**) kargs)[i] = ((char**) kargs)[i] + (int) uargs;
	}

	/* copy args onto stack in new address space */
	err = copyout(kargs, uargs, kargslen);
	if (err) {
		DEBUG(DB_SYSEXECV, "Execv error: Couldn't copyout args. err: %d.\n", err);
		kfree(kargs);
		return err;
	}
	kfree(kargs);

#if 0  // advanced debugging
	kprintf("uargs struct at %p\n", uargs);
	for (int i=0; i<=argc; i++) {
		kprintf("  p%d: %p\n", i, ((char**) uargs)[i]);
	}
	for (int i=0; i<argc; i++) {
		kprintf("  str%d: %s\n", i, ((char**) uargs)[i]);
	}
#endif

	/* Warp to user mode. */
	enter_new_process(argc /*argc*/, uargs /*userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  (vaddr_t) uargs, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
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
    if (pid < 2 || pid > MAX_RUNNING_PROCS || pidhandle->pid_status[pid] == (int)NULL){
        return EINVAL;
    }

    /*Check if actual pid is child of the current process */
    child = pidhandle->pid_proc[pid];
    childrennum = array_num(curproc->children);
    for(int i = 0; i< childrennum; i++){
        if (child == array_get(curproc->children, i)){
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
#if OPT_FORK
/*
Function that child first enters, in charge of setting trapframes
*/
void child_forkentry(void *data1, unsigned long data2){
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
		kprintf("No more trapfame space :( \n");
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
    res = thread_fork("new_child_thread", new_proc, child_forkentry, new_tf, 1);
	KASSERT(new_proc->pid >= 1 && new_proc->pid <= MAX_RUNNING_PROCS);
    if (res) {
		pid_t pid = new_proc->pid;
		pidhandle_free_pid(pid);
		proc_destroy(new_proc);
		
		
		kfree(new_tf);
		return res;
	}

    return 0;

}
#endif
/* 
Exits the current process 
*/
void sys__exit(int exitcode){

	KASSERT(curproc != NULL);
	KASSERT(curproc->pid >= 1 && curproc->pid <= MAX_RUNNING_PROCS);
	process_exit(curproc, exitcode);
	thread_exit();
    panic("Exit syscall should never get to this point.");
}

int
align(int pointer, int align)
{
	if (pointer % align) 
		return pointer + align - (pointer % align);
	else
		return pointer;
}

