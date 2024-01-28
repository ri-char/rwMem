#ifndef API_H_
#define API_H_

#include <pthread.h>
#include <stdint.h>
#include <sys/queue.h>
#include "hwbpserver.h"
#ifdef HAS_LINUX_USER_H
#include <linux/user.h>
#else
#include <sys/user.h>
#endif
#include <vector>
#include <string>

bool GetProcessTask(int pid, std::vector<int> &vOutput);
void ProcessAddProcessHwBp(AddProcessHwBpInfo &params, int &allTaskCount, int &insHwBpSuccessTaskCount, std::vector<struct USER_HIT_INFO> &vHit);
int ProcessSetHwBpHitConditions(struct HIT_CONDITIONS params);

#endif /* API_H_ */
