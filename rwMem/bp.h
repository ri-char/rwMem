#ifndef _KERNEL_RWMEM_BP_H_
#define _KERNEL_RWMEM_BP_H_

#include "linux/perf_event.h"
#include "linux/fs.h"
#include "linux/spinlock_types.h"

#define RWMEM_BP_MAJOR_NUM 101

struct set_reg_param {
	uint64_t id;
	uint64_t value;
};
struct set_simd_reg_param {
	uint64_t id;
	__uint128_t value;
};

#define IOCTL_BP_CONTINUE _IO(RWMEM_BP_MAJOR_NUM, 0)
#define IOCTL_BP_SET_REG _IOW(RWMEM_BP_MAJOR_NUM, 1, struct set_reg_param)
#define IOCTL_BP_SET_SIMD_REG                                                  \
	_IOW(RWMEM_BP_MAJOR_NUM, 2, struct set_simd_reg_param)
#define IOCTL_BP_STEP _IO(RWMEM_BP_MAJOR_NUM, 3)
#define IOCTL_BP_IS_STOPPED _IO(RWMEM_BP_MAJOR_NUM, 4)

void bp_callback(struct perf_event *perf, struct perf_sample_data *sample_data,
		 struct pt_regs *regs);
int rwmem_bp_step_handler(struct pt_regs *regs, unsigned long esr);

struct rwmem_bp_private_data {
	struct perf_event *event;
	struct task_struct *target_task;
	struct wait_queue_head wq;
	struct wait_queue_head poll_wq;
	atomic_t poll;
	struct fasync_struct *fasync;
	struct spinlock flag_lock;
	bool continue_flag;
	bool stopped_flag;
};

struct file *create_rwmem_bp_file(void);
#endif