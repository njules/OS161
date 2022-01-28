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
#define ALIGN_STACK 8  // TODO: stack padding

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

	/* copy program into kprogram */
	buflen = strlen((char*) program)+1;
	kprogram = kmalloc(buflen);
	err = copyin(program, kprogram, buflen);
	if (err) {
		DEBUG(DB_SYSEXECV,
			"Execv error: Couldn't copyin program name. err: %d.\n",
			err);
		return err;
	}
	DEBUG(DB_SYSEXECV, "Execv: Got program: \"%s\".\n", kprogram);

	/* compute number of arguments and allocate kargs buffer */
	kargslen = 0;
	for (argc=0; ((char**) args)[argc]; argc++) {
		buflen = strlen(((char**) args)[argc]) + 1;
		kargslen += align(buflen, ALIGN_POINTER);
	}
	kargslen += (argc + 1) * ALIGN_POINTER;
//	kargslen = align(kargslen, ALIGN_STACK);
	kargs = kmalloc(kargslen);
	if (!kargs) {
		kfree(kprogram);
		return ENOMEM;
	}
	DEBUG(DB_SYSEXECV,
		"Execv: Allocated 0x%x bytes for pointers.\n",
		(argc + 1) * ALIGN_POINTER);
	DEBUG(DB_SYSEXECV,
		"Execv: Preparing to copyin %d args into kargs buffer of size 0x%x.\n",
		argc, kargslen);

	/* copy args into kargs buffer */
	argsoffset = (argc + 1) * ALIGN_POINTER;
	for (int i=0; i<argc; i++) {
		((char**) kargs)[i] = (char*) argsoffset;
		DEBUG(DB_SYSEXECV,
			"Execv:  Copying arg %d into offset %p\n",
			i, (char*) argsoffset);
		err = copyinstr(
			(userptr_t) ((char**) args)[i],
			kargs + argsoffset,
			kargslen - argsoffset,
			(size_t*) &buflen);
		if (err) {
			DEBUG(DB_SYSEXECV,
				"Execv error: Couldn't copyin user argument."
				" err: %d, argidx: %d, arg: \"%s\".\n",
				err, i, ((char**) args)[i]);
			kfree(kprogram);
			kfree(kargs);
			return err;
		}
		DEBUG(DB_SYSEXECV, "Execv:  Copied arg %d: %s\n", i, (char*) kargs + argsoffset);
		argsoffset = align(argsoffset + buflen, ALIGN_POINTER);
	}
	((char**) kargs)[argc] = NULL;
	DEBUG(DB_SYSEXECV, "Execv: Copied %d args into kernel buffer.\n", argc);

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
	if (as == NULL) {  // TODO: how do I handle errors after old as has been destroyed (can't go back)
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

void
sys__exit(int exitcode)
{
	process_exit(curproc, exitcode);
	thread_exit();
	panic("Exit syscall should never get to this point.");
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
Gets PID of the current process.
*/
int sys_getpid(int *retval){
    lock_acquire(pidhandle->pid_lock);

    *retval = curproc->pid;
    

    lock_release(pidhandle->pid_lock);
    return 0;
} 

int
align(int pointer, int align)
{
	if (pointer % align) 
		return pointer + align - (pointer % align);
	else
		return pointer;
}

