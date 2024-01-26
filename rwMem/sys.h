#ifndef SYS_H_
#define SYS_H_
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

#include "api_proxy.h"
#include "phy_mem.h"
#include "proc_maps.h"
#include "ver_control.h"
#include <linux/cdev.h>
#include <linux/device.h>
//////////////////////////////////////////////////////////////////

#define MAJOR_NUM 100

#define IOCTL_OPEN_PROCESS _IOWR(MAJOR_NUM, 1, char *)           // 打开进程
#define IOCTL_CLOSE_HANDLE _IOWR(MAJOR_NUM, 2, char *)           // 关闭进程
#define IOCTL_GET_PROCESS_MAPS_COUNT _IOWR(MAJOR_NUM, 3, char *) // 获取进程的内存块地址数量
#define IOCTL_GET_PROCESS_MAPS_LIST _IOWR(MAJOR_NUM, 4, char *)  // 获取进程的内存块地址列表
#define IOCTL_CHECK_PROCESS_ADDR_PHY _IOWR(MAJOR_NUM, 5, char *) // 检查进程内存是否有物理内存位置

struct init_device_info {
    char proc_self_status[4096];
    int proc_self_maps_cnt;
};

//////////////////////////////////////////////////////////////////
static int g_rwProcMem_major = 0;
static dev_t g_rwProcMem_devno;

// rwProcMemDev设备结构体
struct rwProcMemDev {
    struct cdev *pcdev;
};
static struct rwProcMemDev *g_rwProcMem_devp;

static struct class *g_Class_devp;

int rwProcMem_open(struct inode *inode, struct file *filp);
int rwProcMem_release(struct inode *inode, struct file *filp);
ssize_t rwProcMem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos);
ssize_t rwProcMem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos);
// static long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
// static long (*compat_ioctl) (struct file *, unsigned int cmd, unsigned long arg);
long rwProcMem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
loff_t rwProcMem_llseek(struct file *filp, loff_t offset, int orig);

static const struct file_operations rwProcMem_fops = {
    .owner = THIS_MODULE,
    .llseek = rwProcMem_llseek,

    .read = rwProcMem_read,
    .write = rwProcMem_write,

    .unlocked_ioctl = rwProcMem_ioctl,
    .compat_ioctl = rwProcMem_ioctl,
    .open = rwProcMem_open,
    .release = rwProcMem_release,
};

#endif /* SYS_H_ */