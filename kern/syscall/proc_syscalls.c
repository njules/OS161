#include "proc_syscalls.h" // prototype for this file
#include <kern/errno.h>    // Errors (EBADF, EFAULT, ..)
#include <types.h>         // types (userptr_t, size_t, ..)
#include <limits.h>        // OPEN_MAX
#include <current.h>       // curproc
#include <proc.h>          // proc struct (for curproc)
#include <kern/fcntl.h>    // open flags (O_RDONLY, O_WRONLY, ..)
#include <uio.h>           // for moving data (uio, iovec)
#include <vnode.h>         // for moving data (VOP_OPEN, VOP_READ, ..)
#include <vfs.h>           // for vfs functions (vfs_open, vfs_close)
#include <copyinout.h>     // for moving data (copyinstr)
#include <kern/seek.h>     // for seek constants (SEEK_SET, SEEK_CUR, ..)
#include <kern/stat.h>     // for getting file info via VOP_STAT (stat)
