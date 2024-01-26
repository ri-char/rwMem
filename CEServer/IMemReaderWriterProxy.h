#ifndef MEM_READER_WRITER_PROXY_H_
#define MEM_READER_WRITER_PROXY_H_
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <mutex>
#include <thread>
#include <sstream>
#include <stdint.h>

#include <unistd.h>
#include <sys/sysinfo.h>
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define PAGE_NOACCESS 1
#define PAGE_READONLY 2
#define PAGE_READWRITE 4
#define PAGE_WRITECOPY 8
#define PAGE_EXECUTE 16
#define PAGE_EXECUTE_READ 32
#define PAGE_EXECUTE_READWRITE 64

#define MEM_MAPPED 262144
#define MEM_PRIVATE 131072


#pragma pack(1)
typedef struct {
	uint64_t baseaddress;
	uint64_t size;
	uint32_t protection;
	uint32_t type;
	char name[4096];
} DRIVER_REGION_INFO, *PDRIVER_REGION_INFO;
#pragma pack()

#endif /* MEM_READER_WRITER_PROXY_H_ */

