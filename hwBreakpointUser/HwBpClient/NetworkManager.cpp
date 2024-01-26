#include "pch.h"
#include "NetworkManager.h"
#include "Global.h"


#define CMD_GETVERSION 0
#define CMD_CLOSECONNECTION 1
#define CMD_TERMINATESERVER 2
#define CMD_OPENPROCESS 3
#define CMD_CLOSEHANDLE 4
#define CMD_SETHITCONDITIONS 5
#define CMD_ADDPROCESSHWBP 6

//extern char *versionstring;
#pragma pack(1)
struct HwBpVersion {
	int version;
	unsigned char stringsize;
	//append the versionstring
};
struct AddProcessHwBpInfo {
	uint32_t pid;
	uint64_t address;
	uint32_t hwBpAddrLen;
	uint32_t hwBpAddrType;
	uint32_t hwBpThreadType;
	uint32_t hwBpKeepTimeMs;
};
#pragma pack()


CNetworkManager::CNetworkManager() {
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);
	m_skServer = NULL;
}
CNetworkManager::~CNetworkManager() {
	if (m_skServer) {
		closesocket(m_skServer);
	}
	WSACleanup();
}
bool CNetworkManager::ConnectHwBpServer(std::string ip, int port) {
	m_ip = ip;
	m_port = port;

	if (m_skServer) {
		closesocket(m_skServer);
		m_skServer = NULL;
	}

	if (m_ip.empty() || port == 0) {
		return false;
	}
	m_skServer = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	std::wstring stemp = std::wstring(ip.begin(), ip.end());

	InetPton(AF_INET, (PCWSTR)(stemp.c_str()), &addr.sin_addr.S_un.S_addr);
	if (connect(m_skServer, (sockaddr*)&addr, sizeof(sockaddr_in)) != 0) {
		closesocket(m_skServer);
		m_skServer = NULL;
		return false;
	}
	return true;
}
bool CNetworkManager::IsConnected() {
	return m_skServer ? true : false;
}
void CNetworkManager::Disconnect() {
	if (m_skServer) {
		closesocket(m_skServer);
		m_skServer = NULL;
	}
}
bool CNetworkManager::Reconnect() {
	return ConnectHwBpServer(m_ip, m_port);
}
bool CNetworkManager::SetHwBpHitConditions(HIT_CONDITIONS hitConditions) {
	if (!m_skServer) {
		return false;
	}

	unsigned char command = CMD_SETHITCONDITIONS;
	if (sendall(m_skServer, (char*)&command, 1, 0) <= 0) {
		closesocket(m_skServer);
		m_skServer = NULL;
		return false;
	}

	if (sendall(m_skServer, &hitConditions, sizeof(hitConditions), 0) > 0) {
		int result = 0;
		if (recvall(m_skServer, &result, sizeof(result), MSG_WAITALL) > 0) {
			return result == 1 ? true : false;
		}
	}
	return false;
}
bool CNetworkManager::AddProcessHwBp(
	uint32_t pid,
	uint64_t address,
	uint32_t hwBpAddrLen,
	uint32_t hwBpAddrType,
	uint32_t hwBpThreadType,
	uint32_t hwBpKeepTimeMs,
	uint32_t & allTaskCount,
	uint32_t & insHwBpSuccessTaskCount,
	std::vector<USER_HIT_INFO> & vHitResult) {
	vHitResult.clear();
	if (!m_skServer) {
		return false;
	}

	unsigned char command = CMD_ADDPROCESSHWBP;
	if (sendall(m_skServer, (char*)&command, 1, 0) <= 0) {
		closesocket(m_skServer);
		m_skServer = NULL;
		return false;
	}

	AddProcessHwBpInfo hwbpInfo = { 0 };
	hwbpInfo.pid = pid;
	hwbpInfo.address = address;
	hwbpInfo.hwBpAddrLen = hwBpAddrLen;
	hwbpInfo.hwBpAddrType = hwBpAddrType;
	hwbpInfo.hwBpThreadType = hwBpThreadType;
	hwbpInfo.hwBpKeepTimeMs = hwBpKeepTimeMs;
	if (sendall(m_skServer, &hwbpInfo, sizeof(hwbpInfo), 0) > 0) {
#pragma pack(1)
		struct {
			uint32_t allTaskCount;
			uint32_t insHwBpSuccessTaskCount;
			uint32_t hit_count;
		} result;
#pragma pack()
		if (recvall(m_skServer, &result, sizeof(result), MSG_WAITALL) > 0) {
			for (; result.hit_count > 0; result.hit_count--) {
				USER_HIT_INFO newHit = { 0 };
				if (recvall(m_skServer, &newHit, sizeof(newHit), MSG_WAITALL) > 0) {
					vHitResult.push_back(newHit);
				}
			}
			allTaskCount = result.allTaskCount;
			insHwBpSuccessTaskCount = result.insHwBpSuccessTaskCount;
			return true;
		}
	}
	return false;
}