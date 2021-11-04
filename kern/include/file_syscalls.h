#include <types.h>  // types (userptr_t, size_t, ...)
#include <synch.h>  // synchronization (lock)

#include "opt-filesc.h"

#if OPT_FILESC

struct fhandle {
	struct vnode *vn;
	off_t offset;
	int flags;
	unsigned int ref_count;
	struct lock *lock;
};

int sys_read(int fd, userptr_t buf_ptr, size_t size);

#endif

