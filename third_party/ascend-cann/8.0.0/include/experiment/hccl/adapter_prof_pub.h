/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: adapter层重构，prof接口
 */

#ifndef HCCL_INC_ADAPTER_PROF_PUB_H
#define HCCL_INC_ADAPTER_PROF_PUB_H

#include "hccl_common.h"

uint64_t hrtMsprofGetHashId(const char *hashInfo, uint32_t length);
uint64_t hrtMsprofSysCycleTime(void);

#endif  // HCCL_INC_ADAPTER_PROF_PUB_H