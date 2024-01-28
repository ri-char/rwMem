#ifndef VER_CONTROL_H_
#define VER_CONTROL_H_
#include <linux/version.h>
#define DEV_FILENAME "hwBreakpoint" //当前驱动DEV文件名


//是否打印内核调试日志
//#define CONFIG_DEBUG_PRINTK
#ifndef FILE_OP_DIR_ITER
#define FILE_OP_DIR_ITER iterate_shared
#endif

#ifdef CONFIG_DEBUG_PRINTK
#define printk_debug printk
#else
static inline void printk_debug(char *fmt, ...) {}
#endif

#endif /* VER_CONTROL_H_ */