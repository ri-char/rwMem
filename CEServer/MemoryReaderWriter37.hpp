#ifndef MEMORY_READER_WRITER_H_
#define MEMORY_READER_WRITER_H_

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <vector>

#include "IMemReaderWriterProxy.h"
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// 默认驱动文件名
#define RWPROCMEM_FILE_NODE "/dev/rwMem"

// 安静输出模式
#define QUIET_PRINTF

#ifdef QUIET_PRINTF
#undef TRACE
#define TRACE(fmt, ...)
#else
#define TRACE(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#define MAJOR_NUM 100
#define IOCTL_GET_PROCESS_MAPS_COUNT _IOWR(MAJOR_NUM, 0, char *) // 获取进程的内存块地址数量
#define IOCTL_GET_PROCESS_MAPS_LIST _IOWR(MAJOR_NUM, 1, char *)  // 获取进程的内存块地址列表
#define IOCTL_CHECK_PROCESS_ADDR_PHY _IOWR(MAJOR_NUM, 2, char *) // 检查进程内存是否有物理内存位置

class CMemoryReaderWriter {
  public:
    CMemoryReaderWriter() {}
    ~CMemoryReaderWriter() { DisconnectDriver(); }

    // 连接驱动（驱动节点文件路径名，是否使用躲避SELinux的通信方式，错误代码，机器码ID），返回值：驱动连接句柄，>=0代表成功
    BOOL ConnectDriver(const char *lpszDriverFileNodePath, BOOL bUseBypassSELinuxMode, int &err) {
        if (m_nDriverLink >= 0) {
            return TRUE;
        }
        m_nDriverLink = _rwProcMemDriver_Connect(lpszDriverFileNodePath);
        if (m_nDriverLink < 0) {
            err = m_nDriverLink;
            return FALSE;
        }
        _rwProcMemDriver_UseBypassSELinuxMode(bUseBypassSELinuxMode);
        err = 0;
        return TRUE;
    }

    // 断开驱动，返回值：TRUE成功，FALSE失败
    BOOL DisconnectDriver() {
        if (m_nDriverLink >= 0) {
            _rwProcMemDriver_Disconnect(m_nDriverLink);
            m_nDriverLink = -1;
            return TRUE;
        } else {
            return FALSE;
        }
    }

    // 驱动是否连接正常，返回值：TRUE已连接，FALSE未连接
    BOOL IsDriverConnected() { return m_nDriverLink >= 0; }

    // 驱动_打开进程（进程PID），返回值：进程句柄，0为失败
    uint64_t OpenProcess(uint64_t pid) { return pid; }

    // 驱动_读取进程内存（进程句柄，进程内存地址，读取结果缓冲区，读取结果缓冲区大小，实际读取字节数，是否暴力读取），返回值：TRUE成功，FALSE失败
    BOOL ReadProcessMemory(uint64_t hProcess, uint64_t lpBaseAddress, void *lpBuffer, size_t nSize, size_t *lpNumberOfBytesRead = NULL, BOOL bIsForceRead = FALSE) {
        return _rwProcMemDriver_ReadProcessMemory(m_nDriverLink, hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead, bIsForceRead);
    }

    // 驱动_读取进程内存_单线程极速版（进程句柄，进程内存地址，读取结果缓冲区，读取结果缓冲区大小，实际读取字节数，是否暴力读取），返回值：TRUE成功，FALSE失败
    BOOL ReadProcessMemory_Fast(uint64_t hProcess, uint64_t lpBaseAddress, void *lpBuffer, size_t nSize, size_t *lpNumberOfBytesRead = NULL, BOOL bIsForceRead = FALSE) {
        return _rwProcMemDriver_ReadProcessMemory_Fast(m_nDriverLink, hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead, bIsForceRead);
    }

    // 驱动_写入进程内存（进程句柄，进程内存地址，写入数据缓冲区，写入数据缓冲区大小，实际写入字节数，是否暴力写入），返回值：TRUE成功，FALSE失败
    BOOL WriteProcessMemory(uint64_t hProcess, uint64_t lpBaseAddress, void *lpBuffer, size_t nSize, size_t *lpNumberOfBytesWritten = NULL, BOOL bIsForceWrite = FALSE) {
        return _rwProcMemDriver_WriteProcessMemory(m_nDriverLink, hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten, bIsForceWrite);
    }

    // 驱动_写入进程内存_单线程极速版（进程句柄，进程内存地址，写入数据缓冲区，写入数据缓冲区大小，实际写入字节数，是否暴力写入），返回值：TRUE成功，FALSE失败
    BOOL WriteProcessMemory_Fast(uint64_t hProcess, uint64_t lpBaseAddress, void *lpBuffer, size_t nSize, size_t *lpNumberOfBytesWritten = NULL, BOOL bIsForceWrite = FALSE) {
        return _rwProcMemDriver_WriteProcessMemory_Fast(m_nDriverLink, hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten, bIsForceWrite);
    }

    // 驱动_关闭进程（进程句柄），返回值：TRUE成功，FALSE失败
    BOOL CloseHandle(uint64_t hProcess) { return TRUE; }

    // 驱动_获取进程内存块列表（进程句柄，是否仅显示物理内存，输出缓冲区，输出是否完整），返回值：TRUE成功，FALSE失败
    // （参数showPhy说明: FALSE为显示全部内存，TRUE为只显示在物理内存中的内存，注意：如果进程内存不存在于物理内存中，驱动将无法读取该内存位置的值）
    // （参数bOutListCompleted说明: 若输出FALSE，则代表输出缓冲区里的进程内存块列表不完整，若输出TRUE，则代表输出缓冲区里的进程内存块列表完整可靠）
    BOOL VirtualQueryExFull(uint64_t hProcess, BOOL showPhy, std::vector<DRIVER_REGION_INFO> &vOutput, BOOL &bOutListCompleted) {
        return _rwProcMemDriver_VirtualQueryExFull(m_nDriverLink, hProcess, showPhy, vOutput, &bOutListCompleted);
    }

    // 获取驱动连接FD，返回值：驱动连接的FD
    int GetLinkFD() { return m_nDriverLink; }

    // 设置驱动连接FD
    void SetLinkFD(int fd) { m_nDriverLink = fd; }

    // 设置是否使用躲避SELinux的通信方式
    void SeUseBypassSELinuxMode(BOOL bUseBypassSELinuxMode) { _rwProcMemDriver_UseBypassSELinuxMode(bUseBypassSELinuxMode); }

  private:
    int _rwProcMemDriver_MyIoctl(int fd, unsigned int cmd, unsigned long buf, unsigned long bufSize) {
        if (m_bUseBypassSELinuxMode == TRUE) {
            // 驱动通信方式：lseek，躲开系统SELinux拦截
            char *lseekBuf = (char *)malloc(sizeof(cmd) + bufSize);
            *(unsigned long *)lseekBuf = cmd;
            memcpy((void *)((size_t)lseekBuf + (size_t)sizeof(cmd)), (void *)buf, bufSize);
            uint64_t ret = lseek64(fd, (off64_t)lseekBuf, SEEK_CUR);
            memcpy((void *)buf, (void *)((size_t)lseekBuf + (size_t)sizeof(cmd)), bufSize);
            free(lseekBuf);
            return ret;
        } else {
            // 驱动通信方式：ioctl
            return ioctl(fd, cmd, buf);
        }
    }

    int _rwProcMemDriver_Connect(const char *lpszDriverFileNodePath) {
        int nDriverLink = open(lpszDriverFileNodePath, O_RDWR);
        if (nDriverLink < 0) {
            int last_err = errno;
            if (last_err == EACCES) {
                chmod(lpszDriverFileNodePath, 666);
                nDriverLink = open(lpszDriverFileNodePath, O_RDWR);
                last_err = errno;
                chmod(lpszDriverFileNodePath, 0600);
            }
            if (nDriverLink < 0) {
                TRACE("open error():%s\n", strerror(last_err));
                return -last_err;
            }
        }
        return nDriverLink;
    }

    BOOL _rwProcMemDriver_Disconnect(int nDriverLink) {
        if (nDriverLink < 0) {
            return FALSE;
        }
        close(nDriverLink);
        return TRUE;
    }

    void _rwProcMemDriver_UseBypassSELinuxMode(BOOL bUseBypassSELinuxMode) { m_bUseBypassSELinuxMode = bUseBypassSELinuxMode; }
    BOOL _rwProcMemDriver_ReadProcessMemory(int nDriverLink, uint64_t hProcess, uint64_t lpBaseAddress, void *lpBuffer, size_t nSize, size_t *lpNumberOfBytesRead,
                                            BOOL bIsForceRead) {

        if (lpBaseAddress <= 0) {
            return FALSE;
        }
        if (nDriverLink < 0) {
            return FALSE;
        }
        if (!hProcess) {
            return FALSE;
        }
        if (nSize <= 0) {
            return FALSE;
        }
        if (nSize < 17) {
            char *buf = (char *)calloc(1, 17);
            *(uint64_t *)&buf[0] = hProcess;
            *(uint64_t *)&buf[8] = lpBaseAddress;
            buf[16] = bIsForceRead == TRUE ? '\x01' : '\x00';
            ssize_t realRead = read(nDriverLink, buf, nSize);
            if (realRead <= 0) {
                TRACE("read(): %s\n", strerror(errno));
                free(buf);
                return FALSE;
            }
            if (realRead > 0) {
                memcpy(lpBuffer, buf, realRead);
            }

            if (lpNumberOfBytesRead) {
                *lpNumberOfBytesRead = realRead;
            }
            free(buf);
        } else {
            char *buf = (char *)lpBuffer;
            *(uint64_t *)&buf[0] = hProcess;
            *(uint64_t *)&buf[8] = lpBaseAddress;
            buf[16] = bIsForceRead == TRUE ? '\x01' : '\x00';
            ssize_t realRead = read(nDriverLink, buf, nSize);
            if (realRead <= 0) {
                TRACE("read(): %s\n", strerror(errno));
                return FALSE;
            }

            if (lpNumberOfBytesRead) {
                *lpNumberOfBytesRead = realRead;
            }
        }
        return TRUE;
    }

    BOOL _rwProcMemDriver_ReadProcessMemory_Fast(int nDriverLink, uint64_t hProcess, uint64_t lpBaseAddress, void *lpBuffer, size_t nSize, size_t *lpNumberOfBytesRead,
                                                 BOOL bIsForceRead) {

        if (lpBaseAddress <= 0) {
            return FALSE;
        }
        if (nDriverLink < 0) {
            return FALSE;
        }
        if (!hProcess) {
            return FALSE;
        }
        if (nSize <= 0) {
            return FALSE;
        }
        int bufSize = nSize < 17 ? 17 : nSize;

        // 上一次读内存申请的缓冲区，下一次继续用，可以提速
        static char *lastMallocReadMemBuf = NULL;
        static size_t lastMallocReadMemSize = 0;

        if (lastMallocReadMemSize < bufSize) {
            if (lastMallocReadMemBuf) {
                free(lastMallocReadMemBuf);
            }
            lastMallocReadMemBuf = (char *)malloc(bufSize);
            lastMallocReadMemSize = bufSize;
        }
        memset(lastMallocReadMemBuf, 0, bufSize);
        *(uint64_t *)&lastMallocReadMemBuf[0] = hProcess;
        *(uint64_t *)&lastMallocReadMemBuf[8] = lpBaseAddress;
        lastMallocReadMemBuf[16] = bIsForceRead == TRUE ? '\x01' : '\x00';

        ssize_t realRead = read(nDriverLink, lastMallocReadMemBuf, nSize);

        if (realRead <= 0) {
            TRACE("read(): %s\n", strerror(errno));
            return FALSE;
        }
        if (realRead > 0) {
            memcpy(lpBuffer, lastMallocReadMemBuf, realRead);
        }

        if (lpNumberOfBytesRead) {
            *lpNumberOfBytesRead = realRead;
        }
        return TRUE;
    }

    BOOL _rwProcMemDriver_WriteProcessMemory(int nDriverLink, uint64_t hProcess, uint64_t lpBaseAddress, void *lpBuffer, size_t nSize, size_t *lpNumberOfBytesWritten,
                                             BOOL bIsForceWrite) {
        if (lpBaseAddress <= 0) {
            return FALSE;
        }
        if (nDriverLink < 0) {
            return FALSE;
        }
        if (!hProcess) {
            return FALSE;
        }
        if (nSize <= 0) {
            return FALSE;
        }
        int bufSize = nSize + 17;

        char *buf = (char *)malloc(bufSize);
        memset(buf, 0, bufSize);
        *(uint64_t *)&buf[0] = hProcess;
        *(uint64_t *)&buf[8] = lpBaseAddress;
        buf[16] = bIsForceWrite == TRUE ? '\x01' : '\x00';
        memcpy((void *)((size_t)buf + (size_t)17), lpBuffer, nSize);

        ssize_t realWrite = write(nDriverLink, buf, nSize);
        if (realWrite <= 0) {
            TRACE("write(): %s\n", strerror(errno));
            free(buf);
            return FALSE;
        }

        if (lpNumberOfBytesWritten) {
            *lpNumberOfBytesWritten = realWrite;
        }
        free(buf);
        return TRUE;
    }
    BOOL _rwProcMemDriver_WriteProcessMemory_Fast(int nDriverLink, uint64_t hProcess, uint64_t lpBaseAddress, void *lpBuffer, size_t nSize, size_t *lpNumberOfBytesWritten,
                                                  BOOL bIsForceWrite) {
        if (lpBaseAddress <= 0) {
            return FALSE;
        }
        if (nDriverLink < 0) {
            return FALSE;
        }
        if (!hProcess) {
            return FALSE;
        }
        if (nSize <= 0) {
            return FALSE;
        }
        int bufSize = nSize + 17;

        // 上一次读内存申请的缓冲区，下一次继续用，可以提速
        static char *lastMallocWriteMemBuf = NULL;
        static size_t lastMallocWriteMemSize = 0;

        if (lastMallocWriteMemSize < bufSize) {
            if (lastMallocWriteMemBuf) {
                free(lastMallocWriteMemBuf);
            }
            lastMallocWriteMemBuf = (char *)malloc(bufSize);
            lastMallocWriteMemSize = bufSize;
        }
        *(uint64_t *)&lastMallocWriteMemBuf[0] = hProcess;
        *(uint64_t *)&lastMallocWriteMemBuf[8] = lpBaseAddress;
        lastMallocWriteMemBuf[16] = bIsForceWrite == TRUE ? '\x01' : '\x00';
        memcpy((void *)((size_t)lastMallocWriteMemBuf + (size_t)17), lpBuffer, nSize);

        ssize_t realWrite = write(nDriverLink, lastMallocWriteMemBuf, nSize);
        if (realWrite <= 0) {
            TRACE("write(): %s\n", strerror(errno));
            return FALSE;
        }

        if (lpNumberOfBytesWritten) {
            *lpNumberOfBytesWritten = realWrite;
        }
        return TRUE;
    }

    BOOL _rwProcMemDriver_VirtualQueryExFull(int nDriverLink, uint64_t hProcess, BOOL showPhy, std::vector<DRIVER_REGION_INFO> &vOutput, BOOL *bOutListCompleted) {
        if (nDriverLink < 0) {
            return FALSE;
        }
        if (!hProcess) {
            return FALSE;
        }
        int count = _rwProcMemDriver_MyIoctl(nDriverLink, IOCTL_GET_PROCESS_MAPS_COUNT, (unsigned long)hProcess, sizeof(hProcess));
        TRACE("VirtualQueryExFull count %d\n", count);
        if (count <= 0) {
            TRACE("VirtualQueryExFull ioctl():%s\n", strerror(errno));
            return FALSE;
        }

        uint64_t big_buf_len = 8 + (8 + 8 + 4 + 512) * (count + 50);
        char *big_buf = (char *)calloc(1, big_buf_len);
        *(uint64_t *)&big_buf[0] = hProcess;

        uint64_t name_len = 512;
        *(uint64_t *)&big_buf[8] = name_len;
        *(uint64_t *)&big_buf[16] = big_buf_len;

        int unfinish = _rwProcMemDriver_MyIoctl(nDriverLink, IOCTL_GET_PROCESS_MAPS_LIST, (unsigned long)big_buf, big_buf_len);
        TRACE("VirtualQueryExFull unfinish %d\n", unfinish);
        if (unfinish < 0) {
            TRACE("VirtualQueryExFull ioctl():%s\n", strerror(errno));
            free(big_buf);
            return FALSE;
        }
        size_t copy_pos = (size_t)big_buf;
        uint64_t res = *(uint64_t *)big_buf;
        *bOutListCompleted = unfinish;
        copy_pos += 8;
        for (; res > 0; res--) {
            uint64_t vma_start = 0;
            uint64_t vma_end = 0;
            char vma_flags[4] = {0};
            char name[512] = {0};

            vma_start = *(uint64_t *)copy_pos;
            copy_pos += 8;
            vma_end = *(uint64_t *)copy_pos;
            copy_pos += 8;
            memcpy(&vma_flags, (void *)copy_pos, 4);
            copy_pos += 4;
            memcpy(&name, (void *)copy_pos, 512);
            name[sizeof(name) - 1] = '\0';
            copy_pos += 512;

            DRIVER_REGION_INFO rInfo = {0};
            rInfo.baseaddress = vma_start;
            rInfo.size = vma_end - vma_start;
            if (vma_flags[2] == '\x01') {
                // executable
                if (vma_flags[1] == '\x01') {
                    rInfo.protection = PAGE_EXECUTE_READWRITE;
                } else {
                    rInfo.protection = PAGE_EXECUTE_READ;
                }
            } else {
                // not executable
                if (vma_flags[1] == '\x01') {
                    rInfo.protection = PAGE_READWRITE;
                } else if (vma_flags[0] == '\x01') {
                    rInfo.protection = PAGE_READONLY;
                } else {
                    rInfo.protection = PAGE_NOACCESS;
                }
            }
            if (vma_flags[3] == '\x01') {
                rInfo.type = MEM_MAPPED;
            } else {
                rInfo.type = MEM_PRIVATE;
            }
            memcpy(&rInfo.name, &name, 512);
            rInfo.name[sizeof(rInfo.name) - 1] = '\0';
            if (showPhy) {
                // 只显示在物理内存中的内存
                DRIVER_REGION_INFO rPhyInfo = {0};

                uint64_t addr;
                int isPhyRegion = 0;
                char *isphy = _rwProcMemDriver_CheckMemAddrIsValid(nDriverLink, hProcess, vma_start, vma_end);
                if(!isphy) {
                    continue;
                }
                int i;
                for (addr = vma_start, i = 0; addr < vma_end; addr += getpagesize(), i++) {
                    if (isphy[i / 8] & ((char)1 << (i % 8))) {
                        if (isPhyRegion == 0) {
                            isPhyRegion = 1;
                            rPhyInfo.baseaddress = addr;
                            rPhyInfo.protection = rInfo.protection;
                            rPhyInfo.type = rInfo.type;
                            strcpy(rPhyInfo.name, rInfo.name);
                        }

                    } else {
                        if (isPhyRegion == 1) {
                            isPhyRegion = 0;
                            rPhyInfo.size = addr - rPhyInfo.baseaddress;
                            vOutput.push_back(rPhyInfo);
                        }
                    }
                }

                if (isPhyRegion == 1) {
                    // all vma region inside phy memory
                    rPhyInfo.size = vma_end - rPhyInfo.baseaddress;
                    vOutput.push_back(rPhyInfo);
                }
                delete[] isphy;

            } else {
                // 显示全部内存
                vOutput.push_back(rInfo);
            }
        }
        free(big_buf);

        return !unfinish;
    }

    char *_rwProcMemDriver_CheckMemAddrIsValid(int nDriverLink, uint64_t hProcess, uint64_t BeginAddress, uint64_t EndAddress) {
        if (nDriverLink < 0) {
            return FALSE;
        }
        if (!hProcess) {
            return FALSE;
        }
        size_t bufSize = std::max<int>(24, ((EndAddress/getpagesize()) - (BeginAddress/getpagesize()) + 7) / 8);
        char *ptr_buf = new char[bufSize];
        *(uint64_t *)&ptr_buf[0] = hProcess;
        *(uint64_t *)&ptr_buf[8] = BeginAddress;
        *(uint64_t *)&ptr_buf[16] = EndAddress;
        int r = _rwProcMemDriver_MyIoctl(nDriverLink, IOCTL_CHECK_PROCESS_ADDR_PHY, (unsigned long)ptr_buf, bufSize);
        if (r > 0) {
            return ptr_buf;
        }
        return nullptr;
    }

  private:
    int m_nDriverLink = -1;
    BOOL m_bUseBypassSELinuxMode = FALSE; // 记录是否有SELinux拦截
};

#endif /* MEMORY_READER_WRITER_H_ */
