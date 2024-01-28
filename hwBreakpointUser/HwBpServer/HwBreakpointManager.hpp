#ifndef HW_BREAKPOINT_MANAGER_H_
#define HW_BREAKPOINT_MANAGER_H_
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>


#include <linux/perf_event.h>

// 当前驱动版本号
#define SYS_VERSION 01
#define DEV_FILENAME "/dev/hwBreakpoint"

enum {
    HW_BREAKPOINT_LEN_1 = 1,
    HW_BREAKPOINT_LEN_2 = 2,
    HW_BREAKPOINT_LEN_4 = 4,
    HW_BREAKPOINT_LEN_8 = 8,
};

enum {
    HW_BREAKPOINT_EMPTY = 0,
    HW_BREAKPOINT_R = 1,
    HW_BREAKPOINT_W = 2,
    HW_BREAKPOINT_RW = HW_BREAKPOINT_R | HW_BREAKPOINT_W,
    HW_BREAKPOINT_X = 4,
    HW_BREAKPOINT_INVALID = HW_BREAKPOINT_RW | HW_BREAKPOINT_X,
};
struct my_user_pt_regs {
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
    uint64_t orig_x0;
    uint64_t syscallno;
};

struct USER_HIT_INFO {
    size_t hit_addr;             // 命中地址
    size_t hit_count;            // 命中次数
    struct my_user_pt_regs regs; // 最后一次命中的寄存器数据
};
struct HIT_CONDITIONS {
    char enable_regs[31];
    char enable_sp;
    char enable_pc;
    char enable_pstate;
    char enable_orig_x0;
    char enable_syscallno;
    struct my_user_pt_regs regs;
};

#define MAJOR_NUM 100
#define IOCTL_GET_NUM_BRPS _IOR(MAJOR_NUM, 3, char *)            // 获取CPU支持硬件执行断点的数量
#define IOCTL_GET_NUM_WRPS _IOR(MAJOR_NUM, 4, char *)            // 获取CPU支持硬件访问断点的数量
#define IOCTL_SET_HWBP_HIT_CONDITIONS _IOR(MAJOR_NUM, 5, char *) // 设置硬件断点命中记录条件
#define IOCTL_ADD_PROCESS_HWBP _IOR(MAJOR_NUM, 6, char *)        // 设置进程硬件断点
#define IOCTL_DEL_PROCESS_HWBP _IOR(MAJOR_NUM, 7, char *)        // 删除进程硬件断点
#define IOCTL_GET_HWBP_HIT_ADDR_COUNT _IOR(MAJOR_NUM, 8, char *) // 获取硬件断点命中地址数量

// 声明
//////////////////////////////////////////////////////////////////////////
// C语言形式接口：
/////////////////////////////////////////////////////////////////////////

// 连接驱动，返回值：驱动连接句柄，>=0代表成功
static int hwBreakpointProcDriver_Connect();

// 断开驱动，返回值：TRUE成功，FALSE失败
static bool hwBreakpointProcDriver_Disconnect(int nDriverLink);

// 驱动_获取CPU支持硬件执行断点的数量，返回值：TRUE成功，FALSE失败
static int hwBreakpointProcDriver_GetNumBRPS(int nDriverLink);

// 驱动_获取CPU支持硬件访问断点的数量，返回值：TRUE成功，FALSE失败
static int hwBreakpointProcDriver_GetNumWRPS(int nDriverLink);

// 驱动_设置硬件断点命中记录条件，返回值：TRUE成功，FALSE失败
static bool hwBreakpointProcDriver_SetHwBpHitConditions(int nDriverLink, HIT_CONDITIONS *hitConditions);

// 驱动_新增硬件断点，返回值：TRUE成功，FALSE失败
static uint64_t hwBreakpointProcDriver_AddProcessHwBp(int nDriverLink, uint64_t hProcess, uint64_t lpBaseAddress, uint64_t hwBreakpointLen, unsigned int hwBreakpointType);

// 驱动_删除硬件断点，返回值：TRUE成功，FALSE失败
static bool hwBreakpointProcDriver_DelProcessHwBp(int nDriverLink, uint64_t hHwBreakpointHandle);

// 驱动_读取硬件断点命中记录信息，返回值：TRUE成功，FALSE失败
static bool hwBreakpointProcDriver_ReadHwBpInfo(int nDriverLink, uint64_t hHwBreakpointHandle, std::vector<USER_HIT_INFO> &vOutput);

// 驱动_清除硬件断点命中记录信息，返回值：TRUE成功，FALSE失败
static bool hwBreakpointProcDriver_CleanHwBpInfo(int nDriverLink);

class CHwBreakpointManager {
  public:
    CHwBreakpointManager() {}
    ~CHwBreakpointManager() { DisconnectDriver(); }

    // 连接驱动（错误代码），返回值：驱动连接句柄，>=0代表成功
    bool ConnectDriver(int &err) {
        if (m_nDriverLink >= 0) {
            return true;
        }
        m_nDriverLink = hwBreakpointProcDriver_Connect();
        if (m_nDriverLink < 0) {
            err = m_nDriverLink;
            return false;
        } else {
            err = 0;
        }
        return true;
    }

    // 断开驱动，返回值：TRUE成功，FALSE失败
    bool DisconnectDriver() {
        if (m_nDriverLink >= 0) {
            hwBreakpointProcDriver_Disconnect(m_nDriverLink);
            m_nDriverLink = -1;
            return true;
        }
        return false;
    }

    // 驱动是否连接正常，返回值：TRUE已连接，FALSE未连接
    bool IsDriverConnected() { return m_nDriverLink >= 0 ? true : false; }

    // 驱动_获取CPU支持硬件执行断点的数量，返回值：TRUE成功，FALSE失败
    int GetNumBRPS() { return hwBreakpointProcDriver_GetNumBRPS(m_nDriverLink); }
    // 驱动_获取CPU支持硬件访问断点的数量，返回值：TRUE成功，FALSE失败
    int GetNumWRPS() { return hwBreakpointProcDriver_GetNumWRPS(m_nDriverLink); }

    // 驱动_设置硬件断点命中记录条件，返回值：TRUE成功，FALSE失败
    bool SetHwBpHitConditions(HIT_CONDITIONS &hitConditions) { return hwBreakpointProcDriver_SetHwBpHitConditions(m_nDriverLink, &hitConditions); }

    // 驱动_新增硬件断点，返回值：TRUE成功，FALSE失败
    uint64_t AddProcessHwBp(uint64_t hProcess, uint64_t lpBaseAddress, uint64_t hwBreakpointLen, unsigned int hwBreakpointType) {
        return hwBreakpointProcDriver_AddProcessHwBp(m_nDriverLink, hProcess, lpBaseAddress, hwBreakpointLen, hwBreakpointType);
    }

    // 驱动_删除硬件断点，返回值：TRUE成功，FALSE失败
    bool DelProcessHwBp(uint64_t hHwBreakpointHandle) { return hwBreakpointProcDriver_DelProcessHwBp(m_nDriverLink, hHwBreakpointHandle); }

    // 驱动_读取硬件断点命中记录信息，返回值：TRUE成功，FALSE失败
    bool ReadHwBpInfo(uint64_t hHwBreakpointHandle, std::vector<USER_HIT_INFO> &vOutput) {
        return hwBreakpointProcDriver_ReadHwBpInfo(m_nDriverLink, hHwBreakpointHandle, vOutput);
    }

    // 驱动_清除硬件断点命中记录信息，返回值：TRUE成功，FALSE失败
    bool CleanHwBpInfo() { return hwBreakpointProcDriver_CleanHwBpInfo(m_nDriverLink); }

  private:
    int m_nDriverLink = -1;
};
extern CHwBreakpointManager g_Driver;

static int hwBreakpointProcDriver_Connect() {
    int nDriverLink = open(DEV_FILENAME, O_RDWR);
    if (nDriverLink < 0) {
        printf("open error():%s\n", strerror(errno));
    }
    return nDriverLink;
}

static bool hwBreakpointProcDriver_Disconnect(int nDriverLink) {
    if (nDriverLink < 0) {
        return false;
    }
    close(nDriverLink);
    return true;
}

static int hwBreakpointProcDriver_GetNumBRPS(int nDriverLink) {
    if (nDriverLink < 0) {
        return 0;
    }
    int res = ioctl(nDriverLink, IOCTL_GET_NUM_BRPS, 0);
    return res;
}
static int hwBreakpointProcDriver_GetNumWRPS(int nDriverLink) {
    if (nDriverLink < 0) {
        return 0;
    }
    int res = ioctl(nDriverLink, IOCTL_GET_NUM_WRPS, 0);
    return res;
}

static bool hwBreakpointProcDriver_SetHwBpHitConditions(int nDriverLink, HIT_CONDITIONS *hitConditions) {
    if (nDriverLink < 0) {
        return false;
    }
    int res = ioctl(nDriverLink, IOCTL_SET_HWBP_HIT_CONDITIONS, hitConditions);
    if (res != 0) {
        printf("SetHwBpHitConditions ioctl():%s\n", strerror(errno));
        return false;
    }
    return true;
}
static uint64_t hwBreakpointProcDriver_AddProcessHwBp(int nDriverLink, uint64_t hProcess, uint64_t lpBaseAddress, uint64_t hwBreakpointLen, unsigned int hwBreakpointType) {
    if (nDriverLink < 0) {
        return 0;
    }

    unsigned char buf[32] = {0};
    memcpy(buf, &hProcess, 8);
    memcpy((void *)((size_t)buf + (size_t)8), &lpBaseAddress, 8);
    memcpy((void *)((size_t)buf + (size_t)16), &hwBreakpointLen, 8);
    memcpy((void *)((size_t)buf + (size_t)24), &hwBreakpointType, 8);

    int res = ioctl(nDriverLink, IOCTL_ADD_PROCESS_HWBP, &buf);
    if (res != 0) {
        printf("AddProcessHwBp ioctl():%s\n", strerror(errno));
        return 0;
    }
    uint64_t ptr = 0;
    memcpy(&ptr, &buf, 8);
    return ptr;
}

static bool hwBreakpointProcDriver_DelProcessHwBp(int nDriverLink, uint64_t hHwBreakpointHandle) {
    if (nDriverLink < 0) {
        return false;
    }
    if (!hHwBreakpointHandle) {
        return false;
    }

    char buf[8] = {0};
    memcpy(buf, &hHwBreakpointHandle, 8);

    int res = ioctl(nDriverLink, IOCTL_DEL_PROCESS_HWBP, &buf);
    if (res != 0) {
        printf("DelProcessHwBp ioctl():%s\n", strerror(errno));
        return false;
    }
    return true;
}

static bool hwBreakpointProcDriver_ReadHwBpInfo(int nDriverLink, uint64_t hHwBreakpointHandle, std::vector<USER_HIT_INFO> &vOutput) {
    if (nDriverLink < 0) {
        return false;
    }
    if (!hHwBreakpointHandle) {
        return false;
    }

    char buf[8] = {0};
    memcpy(buf, &hHwBreakpointHandle, 8);
    int count = ioctl(nDriverLink, IOCTL_GET_HWBP_HIT_ADDR_COUNT, &buf);
    // printf("count %d\n", count);
    if (count <= 0) {
        // printf("ioctl():%s\n", strerror(errno));
        return false;
    }

    uint64_t big_buf_len = sizeof(struct USER_HIT_INFO) * count;
    char *big_buf = (char *)malloc(big_buf_len);
    memset(big_buf, 0, big_buf_len);
    memcpy(big_buf, &hHwBreakpointHandle, 8);

    int res = read(nDriverLink, big_buf, big_buf_len);
    // printf("res %d\n", res);
    if (res <= 0) {
        free(big_buf);
        return false;
    }
    size_t copy_pos = (size_t)big_buf;

    for (; res > 0; res--) {
        struct USER_HIT_INFO hInfo = {0};
        memcpy(&hInfo, (void *)copy_pos, sizeof(hInfo));
        copy_pos += sizeof(hInfo);
        vOutput.push_back(hInfo);
    }
    free(big_buf);
    return true;
}

static bool hwBreakpointProcDriver_CleanHwBpInfo(int nDriverLink) {
    if (nDriverLink < 0) {
        return false;
    }
    int res = write(nDriverLink, (void *)1, 1);
    if (res == 0) {
        printf("CleanHwBpInfo write():%s\n", strerror(errno));
        return false;
    }
    return true;
}

#endif /* HW_BREAKPOINT_MANAGER_H_ */
