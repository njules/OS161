#include "proc_syscalls.h"
#include <lib.h>
#include <kern/errno.h>
#include <limits.h>
#include <addrspace.h>
#include <proc.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <copyinout.h>
#include <syscall.h>

#define ALIGN_POINTER 4
#define ALIGN_STACK 8

int
sys_execv(userptr_t program, userptr_t args)
{
/*	int buflen;
	struct vnode *v;
	struct addrspace *as;
	vaddr_t entrypoint, stackptr;
	userptr_t tos;
*/
	int err;
	char *kprogram;
	int buflen;
	int argc;
	char *kargs;
	int kargslen;
	int argsoffset;

	/* copy program into kprogram */
	buflen = strlen((char*) program)+1;
	kprogram = kmalloc(buflen);
	err = copyin(program, kprogram, buflen);
	if (err) {
		return err;
	}

	/* compute number of arguments and allocated kargs buffer */
	kargslen = 0;
	for (argc=0; ((char**) args)[argc]; argc++) {
		buflen = strlen(((char**) args)[argc]);
		kargslen += align(buflen, ALIGN_POINTER);
	}
	kargslen += argc * ALIGN_POINTER;
	kargs = kmalloc(kargslen);
	if (!kargs) {
		kfree(kprogram);
		return ENOMEM;
	}

	/* copy args into kargs buffer */
	argsoffset = argc * ALIGN_POINTER;
	for (int i=0; i<argc; i++) {
		err = copyinstr(args[i], kargs+argsoffset, kargslen-argsoffset, &buflen);
		argsoffset = align(args
		if (err) {
			kfree(kprogram);
			kfree(kargs);
			return err;
		}
	}

	/* compute number of arguments *//*
	for (argc=0; ((char **) args)[argc]; argc++);
	char* kargs[argc];
	char* uargs[argc+1];

	*//* copy args into kargs *//*
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
	
	*//* copy program into kernel *//*
	buflen = strlen(program)+1;
	err = copyin(program, kprogram, buflen);
	if (err) {
		free_kargs(kargs, argc);
		return err;
	}
	
	*//* open executable *//*
	err = vfs_open(kprogram, O_RDONLY, 0, &v);
	if (err) {
		free_kargs(kargs, argc);
		return err;
	}

	*//* destroy old address space and switch to new one *//*
	destroy_current_as();
	as = as_create();
	if (as == NULL) {
		free_kargs(kargs, argc);
		vfs_close(v);
		return ENOMEM;
	}
	proc_setas(as);
	as_activate();

	*//* Load the executable. *//*
	err = load_elf(v, &entrypoint);
	if (err) {
		destroy_current_as();
		free_kargs(kargs, argc);
		vfs_close(v);
		return err;
	}

	*//* Done with the file now. *//*
	vfs_close(v);

	*//* Define the user stack in the address space *//*
	err = as_define_stack(as, &stackptr);
	if (err) {
		destroy_current_as();
		free_kargs(kargs, argc);
		return err;
	}

	*//* copy args onto stack in new address space *//*
	tos = (userptr_t) stackptr;
	for (int i=0; i<argc; i++) {
		buflen = strlen(kargs[i]+1);
		tos -= buflen;
		copyout(kargs[i], tos, buflen);  // TODO: catch error?
		uargs[i] = (char*) tos;
	}
	uargs[argc] = NULL;
	// TODO: padding
	buflen = (argc+1) * sizeof(char*);
	tos -= buflen;
	copyout(uargs, tos, buflen);

	// TODO: stack align

	free_kargs(kargs, argc);

	*//* Warp to user mode. */
//	enter_new_process(argc /*argc*/, tos /*userspace addr of argv*/,
//			  NULL /*userspace addr of environment*/,
//			  stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

int
align(int pointer, int align)
{
	if (pointer % align) 
		return 0;
	else
		return align - (pointer % align);
}
/*
void
destroy_current_as()
{
	// destroys the current address space 
	struct addrspace *as;
	as = proc_setas(NULL);
	as_deactivate();
	as_destroy(as);
}*/

void
free_kargs(char** argv, int n)
{
	/* frees memory allocated to first n elemets of kargs */
	for (int i=0; i<n; i++) {
		kfree(argv[i]);
	}
}

