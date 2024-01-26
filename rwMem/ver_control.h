#ifndef VERSION_CONTROL_H_
#define VERSION_CONTROL_H_
#define DEV_FILENAME "rwProcMem37" //当前驱动DEV文件名

//直接调用内核API进行用户层数据交换
#define CONFIG_DIRECT_API_USER_COPY

//启用页表计算物理内存的地址
#define CONFIG_USE_PAGE_TABLE_CALC_PHY_ADDR

//启用读取pagemap文件来计算物理内存的地址
//#define CONFIG_USE_PAGEMAP_FILE_CALC_PHY_ADDR

//打印内核调试信息
//#define CONFIG_DEBUG_PRINTK

#ifdef CONFIG_DEBUG_PRINTK
#define printk_debug printk
#else
static inline void printk_debug(char *fmt, ...) {}
#endif

#endif /* VERSION_CONTROL_H_ */
