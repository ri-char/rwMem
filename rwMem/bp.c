#include "bp.h"
#include "api_proxy.h"
#include "asm/current.h"
#include "asm/debug-monitors.h"
#include "asm/processor.h"
#include "asm/ptrace.h"
#include "linux/anon_inodes.h"
#include "linux/atomic/atomic-instrumented.h"
#include "linux/file.h"
#include "linux/hw_breakpoint.h"
#include "linux/poll.h"
#include "linux/spinlock.h"
#include "linux/spinlock_types.h"
#include "linux/task_work.h"
#include "linux/types.h"
#include "linux/wait.h"

struct hit_bp_cb {
	struct callback_head twork;
	struct file *file;
};

DEFINE_SPINLOCK(step_list_lock);
LIST_HEAD(step_list);

struct step_list_entry {
	struct list_head list;
	pid_t pid;
	struct file *file;
	bool removed;
};

static void bp_callback_after(struct callback_head *twork)
{
	struct hit_bp_cb *twcb = container_of(twork, struct hit_bp_cb, twork);
	struct rwmem_bp_private_data *priv_data = twcb->file->private_data;

	// mark stopped
	spin_lock(&priv_data->flag_lock);
	priv_data->stopped_flag = true;
	spin_unlock(&priv_data->flag_lock);

	// wake up the fasync
	kill_fasync(&priv_data->fasync, SIGIO, POLL_IN);

	// wake up the poll
	atomic_set(&priv_data->poll, EPOLLIN);
	wake_up_all(&priv_data->poll_wq);

	// wait for continue
	wait_event(priv_data->wq, priv_data->continue_flag);

	// we can continue here, unset flags
	atomic_set(&priv_data->poll, 0);
	spin_lock(&priv_data->flag_lock);
	priv_data->continue_flag = false;
	priv_data->stopped_flag = false;
	spin_unlock(&priv_data->flag_lock);

	// clean up
	kfree(twcb);
}

int rwmem_bp_step_handler(struct pt_regs *regs, unsigned long esr)
{
	pid_t pid = current->pid;
	struct hit_bp_cb *twcb;
	struct list_head *pos;
	struct step_list_entry *entry;
	struct file *file = NULL;
	bool removed;

	// Find the file associated with the pid
	spin_lock(&step_list_lock);
	list_for_each (pos, &step_list) {
		entry = list_entry(pos, struct step_list_entry, list);
		if (entry->pid == pid) {
			file = entry->file;
			removed = entry->removed;
			list_del(pos);
			kfree(entry);
			// if the file is closed, just ignore it
			if (removed) {
				printk(KERN_INFO
				       "step_callback find a removed step\n");
				spin_unlock(&step_list_lock);
				return DBG_HOOK_HANDLED;
			}
			break;
		}
	}
	spin_unlock(&step_list_lock);

	// If no file is found, we cannot continue
	if (!file) {
		return DBG_HOOK_ERROR;
	}

	// create a resume task
	printk(KERN_INFO "step_callback %lx\n", regs->pc);
	twcb = kzalloc(sizeof(*twcb), GFP_KERNEL);
	twcb->file = file;
	init_task_work(&twcb->twork, bp_callback_after);
	task_work_add(current, &twcb->twork, TWA_RESUME);

	// remove the single step flag
	user_disable_single_step(current);
	return DBG_HOOK_HANDLED;
}

void bp_callback(struct perf_event *perf, struct perf_sample_data *sample_data,
		 struct pt_regs *regs)
{
	struct hit_bp_cb *twcb;
	struct file *file =
		(void *)((uint64_t)(perf->overflow_handler_context) |
			 ((uint64_t)0xff << 56));
	struct debug_info *debug_info;
	debug_info = &current->thread.debug;

	printk(KERN_INFO "bp_callback %lx\n", regs->pc);

	// create a resume task
	twcb = kzalloc(sizeof(*twcb), GFP_KERNEL);
	twcb->file = file;
	init_task_work(&twcb->twork, bp_callback_after);
	task_work_add(current, &twcb->twork, TWA_RESUME);
}

static int rwmem_bp_fasync(int fd, struct file *filp, int on)
{
	struct inode *inode = file_inode(filp);
	struct rwmem_bp_private_data *event = filp->private_data;
	int retval;

	inode_lock(inode);
	retval = fasync_helper(fd, filp, on, &event->fasync);
	inode_unlock(inode);

	if (retval < 0)
		return retval;

	return 0;
}

static __poll_t rwmem_bp_poll(struct file *file, poll_table *wait)
{
	struct rwmem_bp_private_data *event = file->private_data;
	__poll_t events;
	poll_wait(file, &event->poll_wq, wait);
	events = atomic_xchg(&event->poll, 0);
	return events;
}

int rwmem_bp_release(struct inode *inode, struct file *filp)
{
	struct step_list_entry *entry;
	struct rwmem_bp_private_data *data = filp->private_data;
	// wake up target task
	spin_lock(&data->flag_lock);
	data->continue_flag = true;
	spin_unlock(&data->flag_lock);
	wake_up_all(&data->wq);

	// mark the file as removed
	spin_lock(&step_list_lock);
	list_for_each_entry (entry, &step_list, list) {
		if (entry->file == filp) {
			entry->removed = true;
			break;
		}
	}
	spin_unlock(&step_list_lock);

	// clean up
	if (data->event) {
		unregister_hw_breakpoint(data->event);
	}
	if (data->target_task) {
		put_task_struct(data->target_task);
	}

	kfree(data);
	filp->private_data = NULL;
	return 0;
}
ssize_t rwmem_bp_read(struct file *filp, char __user *buf, size_t size,
		      loff_t *ppos)
{
	struct rwmem_bp_private_data *data = filp->private_data;
	struct user_fpsimd_state *uregs;
	struct pt_regs *pt_regs;

	// If the target task is not stopped, we cannot read the registers
	if (!data->stopped_flag) {
		return -EINVAL;
	}
	pt_regs = task_pt_regs(data->target_task);

	if (sizeof(pt_regs->user_regs) + sizeof(*uregs) <= size) {
		// The buffer is large enough. Copy both pt_regs and uregs
		uregs = &data->target_task->thread.uw.fpsimd_state;
		if (x_copy_to_user(buf, &pt_regs->user_regs,
				   sizeof(pt_regs->user_regs))) {
			return -EFAULT;
		}
		if (x_copy_to_user(buf + sizeof(pt_regs->user_regs), uregs,
				   sizeof(*uregs))) {
			return -EFAULT;
		}
		return sizeof(*pt_regs) + sizeof(*uregs);
	} else if (sizeof(pt_regs->user_regs) <= size) {
		// The buffer is large enough for pt_regs only
		if (x_copy_to_user(buf, &pt_regs->user_regs,
				   sizeof(pt_regs->user_regs))) {
			return -EFAULT;
		}
		return sizeof(*pt_regs);
	} else {
		// The buffer is too small
		return -EINVAL;
	}
}
long rwmem_bp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case IOCTL_BP_CONTINUE: {
		struct rwmem_bp_private_data *data = filp->private_data;
		// If the target task is not stopped, we cannot continue
		if (!data->stopped_flag) {
			return -EINVAL;
		}
		spin_lock(&data->flag_lock);
		data->continue_flag = true;
		spin_unlock(&data->flag_lock);
		wake_up(&data->wq);
		return 0;
	}
	case IOCTL_BP_SET_REG: {
		struct set_reg_param param;
		struct rwmem_bp_private_data *data = filp->private_data;
		struct pt_regs *pt_regs;
		if (!data->stopped_flag) {
			return -EINVAL;
		}
		if (x_copy_from_user((void *)&param, (void *)arg,
				     sizeof(param))) {
			return -EFAULT;
		}
		if (param.id >= 34) {
			return -EINVAL;
		}
		pt_regs = task_pt_regs(data->target_task);
		pt_regs->user_regs.regs[param.id] = param.value;
		return 0;
	}
	case IOCTL_BP_SET_SIMD_REG: {
		struct rwmem_bp_private_data *data = filp->private_data;
		struct user_fpsimd_state *uregs =
			&data->target_task->thread.uw.fpsimd_state;
		struct set_simd_reg_param param;
		if (!data->stopped_flag) {
			return -EINVAL;
		}
		if (x_copy_from_user((void *)&param, (void *)arg,
				     sizeof(param))) {
			return -EFAULT;
		}
		if (param.id < 32) {
			uregs->vregs[param.id] = param.value;
			return 0;
		} else if (param.id == 32) {
			uregs->fpsr = param.value;
			return 0;
		} else if (param.id == 33) {
			uregs->fpcr = param.value;
			return 0;
		} else {
			return -EINVAL;
		}
	}
	case IOCTL_BP_STEP: {
		struct debug_info *debug_info;
		struct step_list_entry *entry;
		struct rwmem_bp_private_data *data = filp->private_data;

		// If the target task is not stopped, we cannot continue
		if (!data->stopped_flag) {
			return -EINVAL;
		}
		// Add the pid to the step list
		spin_lock(&step_list_lock);
		entry = (struct step_list_entry *)kmalloc(
			sizeof(struct step_list_entry), GFP_KERNEL);
		entry->pid = data->target_task->pid;
		entry->file = filp;
		entry->removed = false;
		list_add(&entry->list, &step_list);
		spin_unlock(&step_list_lock);

		// Set the single step flag
		debug_info = &data->target_task->thread.debug;
		// FIXME: check whether the target task is already in single step mode (for example ptrace)
		// Currently, we just consider whether it is in step mode by hw_breakpoint.
		if (test_ti_thread_flag(&data->target_task->thread_info,
					TIF_SINGLESTEP))
			debug_info->suspended_step = 1;
		else
			user_enable_single_step(data->target_task);

		// Wake up the target task
		spin_lock(&data->flag_lock);
		data->continue_flag = true;
		spin_unlock(&data->flag_lock);
		wake_up(&data->wq);
		return 0;
	}
	case IOCTL_BP_IS_STOPPED: {
		struct rwmem_bp_private_data *data = filp->private_data;
		return !!data->stopped_flag;
	}
	default:
		return -EINVAL;
	}
}

static const struct file_operations rwmem_bp_fops = {
	.owner = THIS_MODULE,

	.llseek = no_llseek,
	.poll = rwmem_bp_poll,
	.read = rwmem_bp_read,
	.unlocked_ioctl = rwmem_bp_ioctl,
	.release = rwmem_bp_release,
	.fasync = rwmem_bp_fasync,
};

struct file *create_rwmem_bp_file(void)
{
	struct rwmem_bp_private_data *data =
		kzalloc(sizeof(struct rwmem_bp_private_data), GFP_KERNEL);
	init_waitqueue_head(&data->wq);
	init_waitqueue_head(&data->poll_wq);
	spin_lock_init(&data->flag_lock);
	return anon_inode_getfile("[bp_handle]", &rwmem_bp_fops, data, O_RDWR);
}