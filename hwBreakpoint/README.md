# 驱动名称: Linux ARM64内核硬件断点进程调试驱动1
### 本驱动接口列表：
2.  驱动_获取CPU支持硬件执行断点的数量: GetNumBRPS
3.  驱动_获取CPU支持硬件访问断点的数量: GetNumWRPS
4.  驱动_设置进程硬件断点: AddProcessHwBp
5.  驱动_删除进程硬件断点: DelProcessHwBp
6.  驱动_读取硬件断点命中信息: ReadHwBpInfo
7.  驱动_清空硬件断点命中信息: CleanHwBpInfo