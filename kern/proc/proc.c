/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

#include <types.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <limits.h>
#include <kern/errno.h>

/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

/* Global PID handle table */
 struct pidhandle *pidhandle;

/*
 * Create a proc structure.
 */
static struct proc *
proc_create(const char *name)
{
	struct proc *proc;

	proc = kmalloc(sizeof(*proc));
	if (proc == NULL)
	{
		return NULL;
	}
	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL)
	{
		kfree(proc);
		return NULL;
	}

	proc->p_numthreads = 0;
	spinlock_init(&proc->p_lock);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;

#if OPT_SHELL
	proc->children = array_create();
	if (proc->children == NULL) 
	{
		kfree(proc);
		return NULL;
	}
	DEBUG(DB_SYSFILE, "Initializing file table\n");
	bzero(proc->p_fdtable, OPEN_MAX * sizeof(struct fhandle *));
	proc->pid = 1; // the kernel thread is defined to be 1
#endif

	return proc;
}

/*
 * Destroy a proc structure.
 *
 * Note: nothing currently calls this. Your wait/exit code will
 * probably want to do so.
 */
void proc_destroy(struct proc *proc)
{
	/*
	 * You probably want to destroy and null out much of the
	 * process (particularly the address space) at exit time if
	 * your wait/exit design calls for the process structure to
	 * hang around beyond process exit. Some wait/exit designs
	 * do, some don't.
	 */

	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	/*
	 * We don't take p_lock in here because we must have the only
	 * reference to this structure. (Otherwise it would be
	 * incorrect to destroy it.)
	 */

	/* VFS fields */
	if (proc->p_cwd)
	{
		VOP_DECREF(proc->p_cwd);
		proc->p_cwd = NULL;
	}

	/* VM fields */
	if (proc->p_addrspace)
	{
		/*
		 * If p is the current process, remove it safely from
		 * p_addrspace before destroying it. This makes sure
		 * we don't try to activate the address space while
		 * it's being destroyed.
		 *
		 * Also explicitly deactivate, because setting the
		 * address space to NULL won't necessarily do that.
		 *
		 * (When the address space is NULL, it means the
		 * process is kernel-only; in that case it is normally
		 * ok if the MMU and MMU- related data structures
		 * still refer to the address space of the last
		 * process that had one. Then you save work if that
		 * process is the next one to run, which isn't
		 * uncommon. However, here we're going to destroy the
		 * address space, so we need to make sure that nothing
		 * in the VM system still refers to it.)
		 *
		 * The call to as_deactivate() must come after we
		 * clear the address space, or a timer interrupt might
		 * reactivate the old address space again behind our
		 * back.
		 *
		 * If p is not the current process, still remove it
		 * from p_addrspace before destroying it as a
		 * precaution. Note that if p is not the current
		 * process, in order to be here p must either have
		 * never run (e.g. cleaning up after fork failed) or
		 * have finished running and exited. It is quite
		 * incorrect to destroy the proc structure of some
		 * random other process while it's still running...
		 */
		struct addrspace *as;

		if (proc == curproc)
		{
			as = proc_setas(NULL);
			as_deactivate();
		}
		else
		{
			as = proc->p_addrspace;
			proc->p_addrspace = NULL;
		}
		as_destroy(as);
	}

#if OPT_SHELL
	/* PID Fields */
	/* We empty the children array of processes */
	array_destroy(proc->children);
	
#endif
	KASSERT(proc->p_numthreads == 0);
	spinlock_cleanup(&proc->p_lock);

	kfree(proc->p_name);
	kfree(proc);
}

/*
 * Create the process structure for the kernel.
 */
void proc_bootstrap(void)
{
	kproc = proc_create("[kernel]");
	if (kproc == NULL)
	{
		panic("proc_create for kproc failed\n");
	}

}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name)
{
	struct proc *newproc;

	newproc = proc_create(name);
	if (newproc == NULL)
	{
		return NULL;
	}
#if OPT_SHELL
	/* PID fields */
	//We add to proces s handle table
	int ret = pidhandle_add(newproc, &newproc->pid);
	if (ret){
		kfree(newproc);
		return NULL;
	}
#endif
	/* VM fields */

	newproc->p_addrspace = NULL;

	/* VFS fields */

	/*
	 * Lock the current process to copy its current directory.
	 * (We don't need to lock the new process, though, as we have
	 * the only reference to it.)
	 */
	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL)
	{
		VOP_INCREF(curproc->p_cwd);
		newproc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);

	return newproc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
int proc_addthread(struct proc *proc, struct thread *t)
{
	int spl;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	proc->p_numthreads++;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = proc;
	splx(spl);

	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
void proc_remthread(struct thread *t)
{
	struct proc *proc;
	int spl;

	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	KASSERT(proc->p_numthreads > 0);
	proc->p_numthreads--;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = NULL;
	splx(spl);
}

/*
 * Fetch the address space of (the current) process.
 *
 * Caution: address spaces aren't refcounted. If you implement
 * multithreaded processes, make sure to set up a refcount scheme or
 * some other method to make this safe. Otherwise the returned address
 * space might disappear under you.
 */
struct addrspace *
proc_getas(void)
{
	struct addrspace *as;
	struct proc *proc = curproc;

	if (proc == NULL)
	{
		return NULL;
	}

	spinlock_acquire(&proc->p_lock);
	as = proc->p_addrspace;
	spinlock_release(&proc->p_lock);
	return as;
}

/*
 * Change the address space of (the current) process. Return the old
 * one for later restoration or disposal.
 */
struct addrspace *
proc_setas(struct addrspace *newas)
{
	struct addrspace *oldas;
	struct proc *proc = curproc;

	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	oldas = proc->p_addrspace;
	proc->p_addrspace = newas;
	spinlock_release(&proc->p_lock);
	return oldas;
}

#if OPT_SHELL
/* Returns the process assciated with the given PID. */
 struct proc *
 get_proc_pid(pid_t pid)
 {
 	if (pid < PID_MIN || pid > PID_MAX)
 	{
 		//return EDOM;
		return NULL;
 	}

 	struct proc *proc;

 	// only do this if pid lock is not already acquired, implement this
 	// TO DO
 	// If lock is not acquired, acquire it
	if (!(lock_do_i_hold(pidhandle->pid_lock))){
		lock_acquire(pidhandle->pid_lock);
	}

 	proc = pidhandle->pid_proc[pid];
	if (!(lock_do_i_hold(pidhandle->pid_lock))){
		lock_release(pidhandle->pid_lock);
	}
 	// release lock

 	return proc;
 }

 /* Frees a given PID from the PID handle table. */
 void pidhandle_free_pid(pid_t pid)
 {
 	//if (pid < PID_MIN || pid > PID_MAX)
 	//{
 		//return EDOM;
  	//	return ;
 	//}

 	lock_acquire(pidhandle->pid_lock);
 	pidhandle->qty_available++;
 	pidhandle->pid_proc[pid] = NULL;
	pidhandle->pid_status[pid] = (int) NULL;
	pidhandle->pid_exitcode[pid] = (int) NULL;
 	lock_release(pidhandle->pid_lock);
 }

 /*
  * Initializes the PID handle when the kernel starts.
  */
 void pidhandle_bootstrap()
 {
 	/* Set up the pid handle tables */
 	pidhandle = kmalloc(sizeof(struct pidhandle));
 	if (pidhandle == NULL)
 	{
 		panic("Error initializing PID handle table.\n");
 	}

 	pidhandle->pid_lock = lock_create("pid");
 	if (pidhandle->pid_lock == NULL)
 	{
 		panic("Error initializing PID handle's lock.\n");
 	}
 	/* TO DO, IMPLEMENT CV*/
 	pidhandle->pid_cv = cv_create("pidhandle cv");
 	if (pidhandle->pid_cv == NULL)
 	{
 		panic("Error initializing PID handle's cv.\n");
 	}

 	pidhandle->qty_available = 1; /* 1 space for kernel */
 	pidhandle->next_pid = PID_MIN;

 	pid_t kpid = kproc->pid;
 	/* Set the kernel thread process into the pid structure */
 	pidhandle->pid_proc[kpid] = kproc;
	pidhandle->pid_status[kpid] = RUNNING_STATUS;
	pidhandle->pid_exitcode[kpid] = (int) NULL;
 	pidhandle->qty_available--;

 	/* Initialize the handle table */
 	for (int i = PID_MIN; i < PID_MAX; i++)
 	{
 		pidhandle->qty_available++;
 		pidhandle->pid_proc[i] = NULL;
		pidhandle->pid_status[i] = (int) NULL;
		pidhandle->pid_exitcode[i] = (int) NULL;
 	}
 }

 /*
  * Manages adding a new process to the parent process, it also manages the pid table with the new entry. 
  */
 int pidhandle_add(struct proc *proc, int *retval)
 {
 	int nextpid;

 	if (proc == NULL)
 	{
 		//return ESRCH;
		return 0;
 	}

 	lock_acquire(pidhandle->pid_lock);

 	if (pidhandle->qty_available < 1)
 	{
 		lock_release(pidhandle->pid_lock);
 		//return ENPROC;
		return 0;
 	}

 	array_add(curproc->children, proc, NULL);
 	nextpid = pidhandle->next_pid;
 	*retval = nextpid;

 	pidhandle->pid_proc[nextpid] = proc;
	pidhandle->pid_status[nextpid] = RUNNING_STATUS;
	pidhandle->pid_exitcode[nextpid] = (int) NULL;
 	pidhandle->qty_available--;
	
 	/* Find next avaliable pid (from actual "next"), maybe implement status for processes */
 	if (pidhandle->qty_available > 0)
 	{
 		for (int i = nextpid; i < PID_MAX; i++)
 		{
 			if (pidhandle->pid_proc[i] == NULL || pidhandle->pid_status[i] == (int) NULL)
 			{
 				pidhandle->next_pid = i;
 				break;
 			}
 		}
 	}
 	else
 	{
 		pidhandle->next_pid = PID_MAX + 1;
 	}

 	lock_release(pidhandle->pid_lock);

 	return 0;
 }

/* handles process exit with pidhandle*/
void process_exit(struct proc *proc, int exitcode){

	KASSERT(proc != NULL);
	pid_t pid = proc->pid;
	struct proc *child;
	lock_acquire(pidhandle->pid_lock);

	/* Update list of children due to exit of process, this will be manage by status*/
	int childrennum = array_num(proc->children);
	/* We do a for loop backwards from last children so next pid is set correctly*/
	for(int i = childrennum -1; i >= 0; i--){

		child = array_get(proc->children, i);
		int childpid = child->pid;

		/* If is in zombie status, we destroy the process and clean the pidhandle*/

		if(pidhandle->pid_status[childpid] == ZOMBIE_STATUS){
			/* We update next pid*/
			if(childpid < pidhandle->next_pid){
				pidhandle->next_pid = childpid;
			}
			proc_destroy(child);
			pidhandle->qty_available++;
			pidhandle->pid_proc[childpid] = NULL;
			pidhandle->pid_status[childpid] = (int) NULL;
			pidhandle->pid_exitcode[childpid] = (int) NULL;
		}
		else if(pidhandle->pid_status[childpid] == RUNNING_STATUS){
			pidhandle->pid_status[childpid] = ORPHAN_STATUS;
		}
		else{
			panic("Child does not exist, I do not know how to manage.\n");
		}
	}

	if(pidhandle->pid_status[pid] == RUNNING_STATUS){
		pidhandle->pid_status[pid] = ZOMBIE_STATUS; /* parent has not executed wait*/
		pidhandle->pid_exitcode[pid] = exitcode;
	} 
	else if(pidhandle->pid_status[pid] == ORPHAN_STATUS){
		/* destroy process and delete it from handle */
		proc_destroy(curproc);
		pidhandle->qty_available++;
		pidhandle->pid_proc[pid] = NULL;
		pidhandle->pid_status[pid] = (int) NULL;
		pidhandle->pid_exitcode[pid] = (int) NULL;
	}
	/* TODO: maybe not broadcast all because there's no guarantee that they are waiting on us. Improve*/
	cv_broadcast(pidhandle->pid_cv, pidhandle->pid_lock); /* Broadcast to all waiting processes*/
	lock_release(pidhandle->pid_lock);

}

int handle_proc_fork(struct proc **new_proc, const char *new_name){
	int res;
	struct proc *proc; /* We create a temporary structure to define first*/

	proc = proc_create(new_name);
	if (proc == NULL) {
		return ENOMEM;
	}

	res = pidhandle_add(proc, &proc->pid);
	if (res) {
		proc_destroy(proc);
		return res;
	}
	/* We now copy all of the information of the current process into child*/
	res = as_copy(curproc->p_addrspace, &proc->p_addrspace);
	if (res) {
		pidhandle_free_pid(proc->pid); /* we free spot used for child process*/
		proc_destroy(proc);
		return res;
	}

	/* We use spinlocks to protect the copy of the working directory*/
	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		proc->p_cwd = curproc->p_cwd; 
	}
	spinlock_release(&curproc->p_lock);
	
	lock_acquire(curproc->proc_lock);
	/* We copy the filetable*/
	for( int i = 0; i < OPEN_MAX; i++){
		struct fhandle *fhandle_entry;
		fhandle_entry = curproc->p_fdtable[i];

		if (fhandle_entry == NULL) {
			continue;
		}
		lock_acquire(fhandle_entry->lock);
		fhandle_entry->ref_count ++;
		lock_release(fhandle_entry->lock);

		proc->p_fdtable[i] = fhandle_entry;
	}
	lock_release(curproc->proc_lock);
	*new_proc = proc;

	return 0;
}
#endif
