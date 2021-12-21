#include "execv_syscall.h"  // prototype for this file
#include <kern/errno.h>  // Errors (ENOSYS, ..)
#include <limits.h>  // limit constants (PATH_MAX, ..)

int sys_execv(userptr_t program, userptr_t args) {

	int err;
	int buflen;
	struct vnode *v;
	struct addrspace *as;
	vaddr_t entrypoint, stackptr, tos;
	int argc;

	for (argc=0; args[argc]; argc++);
	char* kargs[argc];
	char* uargs[argc+1];

	/* copy args into kargs */
	for (int i=0; i<argc; i++) {
		buflen = strlen(args[i])+1;
		kargs[i] = kmalloc(buflen);
		if (!kargs[i]) {
			free_kargs(kargs, i);
			return ENOMEM;
		}
		err = copyin(args[i], kargs[i], buflen);
		if (err) {
			free_kargs(kargs, i+1);
			return err;
		}
	}
	
	/* copy program into kernel */
	buflen = strlen(program)+1;
	char[buflen] kprogram;
	err = copyin(program, kprogram, buflen);
	if (err) {
		free_kargs(kargs, argc);
		return err;
	}
	
	/* open executable */
	err = vfs_open(progname, O_RDONLY, 0, &v);
	if (err) {
		free_kargs(kargs, argc);
		return err;
	}

	/* destroy old address space and switch to new one */
	destroy_current_as();
	as = as_create();
	if (as == NULL) {
		free_kargs(kargs, argc);
		vfs_close(v);
		return ENOMEM;
	}
	proc_setas(as);
	as_activate();

	/* Load the executable. */
	err = load_elf(v, &entrypoint);
	if (err) {
		destroy_current_as();
		free_kargs(kargs, argc);
		vfs_close(v);
		return err;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	err = as_define_stack(as, &stackptr);
	if (err) {
		destroy_current_as();
		free_kargs(kargs, argc);
		return err;
	}

	/* copy args onto stack in new address space */
	tos = stackptr;
	for (int i=0; i<argc; i++) {
		buflen = strlen(kargs[i]+1);
		tos -= buflen;
		copyout(kargs[i], tos, buflen);  // TODO: catch error?
		uargs[i] = tos;
	}
	uargs[argc] = NULL;
	tos -= tos&0x3 ? 0x4 - tos&0x3 : 0;  // padd for pointers
	buflen = (argc+1) * sizeof(char*);
	tos -= buflen;
	copyout(uargs, tos, buflen);

	// TODO: stack align

	free_kargs(kargs, argc);

	/* Warp to user mode. */
	enter_new_process(argc /*argc*/, tos /*userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

void destroy_current_as() {
	/* destroys the current address space */
	struct addrspace *as;
	as = proc_setas(NULL);
	as_deactivate();
	as_destroy(as);
}

void free_kargs(char** argv, int n) {
	/* frees memory allocated to first n elemets of kargs */
	for (int i=0; i<n; i++) {
		kfree(argv[i]);
	}
}

