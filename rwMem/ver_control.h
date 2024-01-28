#ifndef VERSION_CONTROL_H_
#define VERSION_CONTROL_H_
#define DEV_FILENAME "rwMem" //当前驱动DEV文件名

//直接调用内核API进行用户层数据交换
#define CONFIG_DIRECT_API_USER_COPY

//打印内核调试信息
//#define CONFIG_DEBUG_PRINTK

#ifdef CONFIG_DEBUG_PRINTK
#define printk_debug printk
#else
static inline void printk_debug(char *fmt, ...) {}
#endif

#endif /* VERSION_CONTROL_H_ */
