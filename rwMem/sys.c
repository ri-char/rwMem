#include "sys.h"
#include "api_proxy.h"
#include "asm/debug-monitors.h"
#include "bp.h"
#include "linux/fdtable.h"
#include "linux/file.h"
#include "linux/hw_breakpoint.h"
#include "linux/kern_levels.h"
#include "linux/pid.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include "linux/types.h"
#include "linux/wait.h"
#include "phy_mem.h"
#include "proc_maps.h"

int rwmem_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int rwmem_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t rwmem_read(struct file *filp, char __user *buf, size_t size,
		   loff_t *ppos)
{
	char data[17] = { 0 };
	unsigned long read = x_copy_from_user(data, buf, 17);
	if (read == 0) {
		pid_t pid = (pid_t) * (size_t *)&data;
		size_t proc_virt_addr = *(size_t *)&data[8];
		bool is_force_read = data[16] == '\x01' ? true : false;
		size_t read_size = 0;
		struct pid *pid_struct = find_get_pid(pid);
		if (!pid_struct) {
			return -EINVAL;
		}

		if (is_force_read == false &&
		    !check_proc_map_can_read(pid_struct, proc_virt_addr,
					     size)) {
			put_pid(pid_struct);
			return -EFAULT;
		}

		while (read_size < size) {
			size_t phy_addr = 0;
			size_t pfn_sz = 0;
			char *lpOutBuf = NULL;

			pte_t *pte;

			bool old_pte_can_read;
			phy_addr = get_proc_phy_addr(pid_struct,
						     proc_virt_addr + read_size,
						     (pte_t *)&pte);
			printk_debug(KERN_INFO "calc phy_addr:0x%zx\n",
				     phy_addr);
			if (phy_addr == 0) {
				break;
			}

			old_pte_can_read = is_pte_can_read(pte);
			if (is_force_read) {
				if (!old_pte_can_read) {
					if (!change_pte_read_status(pte,
								    true)) {
						break;
					}
				}
			} else if (!old_pte_can_read) {
				break;
			}

			pfn_sz = size_inside_page(
				phy_addr, ((size - read_size) > PAGE_SIZE) ?
						  PAGE_SIZE :
						  (size - read_size));
			printk_debug(KERN_INFO "pfn_sz:%zu\n", pfn_sz);

			lpOutBuf = (char *)(buf + read_size);
			read_ram_physical_addr(phy_addr, lpOutBuf, false,
					       pfn_sz);

			if (is_force_read && old_pte_can_read == false) {
				change_pte_read_status(pte, false);
			}

			read_size += pfn_sz;
		}

		put_pid(pid_struct);
		return read_size;
	} else {
		printk_debug(KERN_INFO
			     "READ FAILED ret:%lu, user:%p, size:%zu\n",
			     read, buf, size);
	}
	return -EFAULT;
}

ssize_t rwmem_write(struct file *filp, const char __user *buf, size_t size,
		    loff_t *ppos)
{
	char data[17] = { 0 };
	unsigned long write = x_copy_from_user(data, buf, 17);
	if (write == 0) {
		pid_t pid = (pid_t) * (size_t *)data;
		size_t proc_virt_addr = *(size_t *)&data[8];
		bool is_force_write = data[16] == '\x01' ? true : false;
		size_t write_size = 0;
		struct pid *pid_struct = find_get_pid(pid);
		if (!pid_struct) {
			return -EINVAL;
		}

		if (is_force_write == false &&
		    !check_proc_map_can_write(pid_struct, proc_virt_addr,
					      size)) {
			put_pid(pid_struct);
			return -EFAULT;
		}

		while (write_size < size) {
			size_t phy_addr = 0;
			size_t pfn_sz = 0;
			char *lpInputBuf = NULL;

			pte_t *pte;
			bool old_pte_can_write;
			phy_addr =
				get_proc_phy_addr(pid_struct,
						  proc_virt_addr + write_size,
						  (pte_t *)&pte);

			printk_debug(KERN_INFO "phy_addr:0x%zx\n", phy_addr);
			if (phy_addr == 0) {
				break;
			}

			old_pte_can_write = is_pte_can_write(pte);
			if (is_force_write) {
				if (!old_pte_can_write) {
					if (!change_pte_write_status(pte,
								     true)) {
						break;
					}
				}
			} else if (!old_pte_can_write) {
				break;
			}

			pfn_sz = size_inside_page(
				phy_addr, ((size - write_size) > PAGE_SIZE) ?
						  PAGE_SIZE :
						  (size - write_size));
			printk_debug(KERN_INFO "pfn_sz:%zu\n", pfn_sz);

			lpInputBuf = (char *)(((size_t)buf + (size_t)17 +
					       write_size));
			write_ram_physical_addr(phy_addr, lpInputBuf, false,
						pfn_sz);

			if (is_force_write && old_pte_can_write == false) {
				change_pte_write_status(pte, false);
			}

			write_size += pfn_sz;
		}
		put_pid(pid_struct);
		return write_size;
	} else {
		printk_debug(KERN_INFO
			     "WRITE FAILED ret:%lu, user:%p, size:%zu\n",
			     write, buf, size);
	}
	return -EFAULT;
}

long rwmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case IOCTL_GET_PROCESS_MAPS_COUNT: {
		uint64_t res;
		struct pid *pid_struct;

		pid_struct = find_get_pid(arg);
		if (!pid_struct) {
			return -EINVAL;
		}
		res = get_proc_map_count(pid_struct);
		put_pid(pid_struct);

		return res;
	}
	case IOCTL_GET_PROCESS_MAPS_LIST: {
		char buf[24];
		size_t name_len, buf_size;
		pid_t pid;
		struct pid *pid_struct;
		int have_pass = 0;
		uint64_t count = 0;
		if (x_copy_from_user((void *)buf, (void *)arg, 24)) {
			return -EINVAL;
		}
		pid = (pid_t) * (size_t *)buf;
		name_len = *(size_t *)&buf[8];
		buf_size = *(size_t *)&buf[16];

		pid_struct = find_get_pid(pid);
		if (!pid_struct) {
			return -EINVAL;
		}
		count = get_proc_maps_list(pid_struct, name_len,
					   (void *)((size_t)arg + (size_t)8),
					   buf_size - 8, false, &have_pass);
		put_pid(pid_struct);
		if (x_copy_to_user((void *)arg, &count, 8)) {
			return -EFAULT;
		}
		return have_pass;
	}
	case IOCTL_CHECK_PROCESS_ADDR_PHY: {
		struct {
			pid_t pid;
			size_t virt_addr_start, virt_addr_end;
		} param;
		size_t proc_virt_addr;
		struct pid *pid_struct;
		struct task_struct *task;
		pte_t *pte;
		size_t ret = 0;
		size_t pages, bufLen, i;
		uint8_t *retBuf;
		if (x_copy_from_user((void *)&param, (void *)arg,
				     sizeof(param))) {
			return -EFAULT;
		}
		if ((param.virt_addr_start | param.virt_addr_end) &
		    (PAGE_SIZE - 1)) {
			return -EINVAL;
		}
		if (param.virt_addr_start >= param.virt_addr_end) {
			return -EINVAL;
		}

		pid_struct = find_get_pid(param.pid);
		if (!pid_struct) {
			return -EINVAL;
		}
		task = pid_task(pid_struct, PIDTYPE_PID);
		if (!task) {
			put_pid(pid_struct);
			return -EINVAL;
		}

#define MAX_MALLOC_SIZE 1024

		pages = (param.virt_addr_end - param.virt_addr_start) /
			PAGE_SIZE;
		bufLen = (pages + 7) / 8;
		bufLen = bufLen > MAX_MALLOC_SIZE ? MAX_MALLOC_SIZE : bufLen;
		retBuf = kmalloc(bufLen, GFP_KERNEL);
		if (!retBuf) {
			put_pid(pid_struct);
			return -ENOMEM;
		}
		memset(retBuf, 0, bufLen);

		for (proc_virt_addr = param.virt_addr_start, i = 0;
		     proc_virt_addr < param.virt_addr_end;
		     proc_virt_addr += PAGE_SIZE) {
			ret = get_task_proc_phy_addr(task, proc_virt_addr,
						     (pte_t *)&pte);
			if (ret && is_pte_can_read(pte)) {
				retBuf[i / 8] |= 1 << (i % 8);
			}
			i++;
			if (i == MAX_MALLOC_SIZE * 8) {
				if (x_copy_to_user((void *)arg, retBuf,
						   bufLen)) {
					kfree(retBuf);
					put_pid(pid_struct);
					return -EFAULT;
				}
				i = 0;
				memset(retBuf, 0, bufLen);
				arg += MAX_MALLOC_SIZE;
			}
		}
		put_pid(pid_struct);
		if (i && x_copy_to_user((void *)arg, retBuf, (i + 7) / 8)) {
			kfree(retBuf);
			return -EFAULT;
		}
		kfree(retBuf);
		return pages;
	}
	case IOCTL_ADD_BP: {
		struct {
			pid_t pid;
			uint8_t type;
			uint8_t len;
			size_t virt_addr;
		} param;
		struct perf_event_attr attr;
		struct pid *pid_struct;
		struct task_struct *task;
		struct perf_event *bp;
		struct file *file;
		struct rwmem_bp_private_data * private_data;
		int fd;
		if (x_copy_from_user((void *)&param, (void *)arg,
				     sizeof(param))) {
			return -EFAULT;
		}
		if (param.type > 4) {
			return -EINVAL;
		}
		if (param.len != 1 && param.len != 2 && param.len != 4 &&
		    param.len != 8) {
			return -EINVAL;
		}

		pid_struct = find_get_pid(param.pid);
		if (!pid_struct) {
			return -EINVAL;
		}
		task = get_pid_task(pid_struct, PIDTYPE_PID);
		put_pid(pid_struct);
		if (!task) {
			return -EINVAL;
		}

		hw_breakpoint_init(&attr);
		attr.exclude_kernel = 1;
		attr.bp_addr = param.virt_addr;
		attr.bp_len = param.len;
		attr.bp_type = param.type;
		attr.disabled = 0;
		fd = get_unused_fd_flags(O_CLOEXEC);
		if (fd < 0) {
			put_task_struct(task);
			return fd;
		}
		file = create_rwmem_bp_file();
		if (IS_ERR(file)) {
			put_task_struct(task);
			put_unused_fd(fd);
			return PTR_ERR(file);
		}
		fd_install(fd, file);

		bp = perf_event_create_kernel_counter(
			&attr, -1, task, bp_callback,
			(void *)(((uint64_t)file & ~((uint64_t)~((uint8_t)0xcb) << 56))));
		if (IS_ERR(bp)) {
			put_task_struct(task);
			close_fd(fd);
			return PTR_ERR(bp);
		}
		private_data = file->private_data;
		private_data->event = bp;
		private_data->target_task = task;

		return fd;
	}
	case IOCTL_GET_NUM_BRPS: {
		return ((read_cpuid(ID_AA64DFR0_EL1) >> 12) & 0xf) + 1;
	}
	case IOCTL_GET_NUM_WRPS: {
		return ((read_cpuid(ID_AA64DFR0_EL1) >> 20) & 0xf) + 1;
	}
	default:
		return -EINVAL;
	}
	return -EINVAL;
}

static struct step_hook rwmem_bp_step_hook = {
	.fn = rwmem_bp_step_handler,
};


static int __init rwmem_dev_init(void)
{
	int result;
	printk(KERN_INFO "load %s\n", DEV_FILENAME);

	g_rwProcMem_devp = kmalloc(sizeof(struct rwmem_dev), GFP_KERNEL);
	if (!g_rwProcMem_devp) {
		result = -ENOMEM;
		goto _fail;
	}
	memset(g_rwProcMem_devp, 0, sizeof(struct rwmem_dev));

	result = alloc_chrdev_region(&g_rwProcMem_devno, 0, 1, DEV_FILENAME);
	g_rwProcMem_major = MAJOR(g_rwProcMem_devno);

	if (result < 0) {
		printk(KERN_EMERG "rwProcMem alloc_chrdev_region failed %d\n",
		       result);
		return result;
	}

	g_rwProcMem_devp->pcdev = kmalloc(sizeof(struct cdev) * 3, GFP_KERNEL);
	cdev_init(g_rwProcMem_devp->pcdev,
		  (struct file_operations *)&rwmem_fops);
	g_rwProcMem_devp->pcdev->owner = THIS_MODULE;
	g_rwProcMem_devp->pcdev->ops = (struct file_operations *)&rwmem_fops;
	if (cdev_add(g_rwProcMem_devp->pcdev, g_rwProcMem_devno, 1)) {
		printk(KERN_NOTICE "Error in cdev_add()\n");
		result = -EFAULT;
		goto _fail;
	}
	g_Class_devp = class_create(THIS_MODULE, DEV_FILENAME);
	device_create(g_Class_devp, NULL, g_rwProcMem_devno, NULL, "%s",
		      DEV_FILENAME);
	register_user_step_hook(&rwmem_bp_step_hook);
	return 0;
_fail:
	unregister_chrdev_region(g_rwProcMem_devno, 1);
	return result;
}

static void __exit rwmem_dev_exit(void)
{
	device_destroy(g_Class_devp, g_rwProcMem_devno);
	class_destroy(g_Class_devp);

	cdev_del(g_rwProcMem_devp->pcdev);
	unregister_chrdev_region(g_rwProcMem_devno, 1);
	unregister_user_step_hook(&rwmem_bp_step_hook);
	kfree(g_rwProcMem_devp->pcdev);
	kfree(g_rwProcMem_devp);
	printk(KERN_INFO "unload %s\n", DEV_FILENAME);
}

module_init(rwmem_dev_init);
module_exit(rwmem_dev_exit);

MODULE_AUTHOR("Linux");
MODULE_DESCRIPTION("Linux default module");
MODULE_LICENSE("GPL");
