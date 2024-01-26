#ifndef PROC_MAPS_AUTO_OFFSET_H_
#define PROC_MAPS_AUTO_OFFSET_H_
#include "api_proxy.h"
#include "linux/sched/mm.h"
#include "ver_control.h"
#include <linux/version.h>


#ifndef MM_STRUCT_MMAP_LOCK 
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,43)
#define MM_STRUCT_MMAP_LOCK mmap_sem
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,43)
#define MM_STRUCT_MMAP_LOCK mmap_lock
#endif
#endif

static inline int down_read_mmap_lock(struct mm_struct *mm) {
	struct rw_semaphore *sem;
	sem = &mm->MM_STRUCT_MMAP_LOCK;
	down_read(sem);
	return 0;
}
static inline int up_read_mmap_lock(struct mm_struct *mm) {
	struct rw_semaphore *sem;
	sem = &mm->MM_STRUCT_MMAP_LOCK;
	up_read(sem);
	return 0;
}

static inline struct file * get_vm_file(struct vm_area_struct *vma) {
	return vma->vm_file;
}
#endif /* PROC_MAPS_AUTO_OFFSET_H_ */