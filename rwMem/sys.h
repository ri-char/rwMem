#ifndef _KERNEL_RWMEM_SYS_H_
#define _KERNEL_RWMEM_SYS_H_
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/perf_event.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define RWMEM_MAJOR_NUM 100

#define IOCTL_GET_PROCESS_MAPS_COUNT _IOW(RWMEM_MAJOR_NUM, 0, size_t)
#define IOCTL_GET_PROCESS_MAPS_LIST _IOWR(RWMEM_MAJOR_NUM, 1, char *)
#define IOCTL_CHECK_PROCESS_ADDR_PHY _IOWR(RWMEM_MAJOR_NUM, 2, char *)
#define IOCTL_ADD_BP _IOWR(RWMEM_MAJOR_NUM, 3, char *)
#define IOCTL_GET_NUM_BRPS _IO(RWMEM_MAJOR_NUM, 4)
#define IOCTL_GET_NUM_WRPS _IO(RWMEM_MAJOR_NUM, 5)

struct init_device_info {
	char proc_self_status[4096];
	int proc_self_maps_cnt;
};

static int g_rwProcMem_major = 0;
static dev_t g_rwProcMem_devno;

// rwProcMemDev设备结构体
struct rwmem_dev {
	struct cdev *pcdev;
};
static struct rwmem_dev *g_rwProcMem_devp;

static struct class *g_Class_devp;

int rwmem_open(struct inode *inode, struct file *filp);
int rwmem_release(struct inode *inode, struct file *filp);
ssize_t rwmem_read(struct file *filp, char __user *buf, size_t size,
		   loff_t *ppos);
ssize_t rwmem_write(struct file *filp, const char __user *buf, size_t size,
		    loff_t *ppos);
long rwmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

static const struct file_operations rwmem_fops = {
	.owner = THIS_MODULE,

	.read = rwmem_read,
	.write = rwmem_write,
	.llseek = no_llseek,
	.unlocked_ioctl = rwmem_ioctl,
	.open = rwmem_open,
	.release = rwmem_release,
};

#endif