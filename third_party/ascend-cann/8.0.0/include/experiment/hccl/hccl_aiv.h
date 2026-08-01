/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: AIV Kernel入口
 */

#ifndef HCCL_AIV_H
#define HCCL_AIV_H

#include <vector>
#include "string"

#include "hccl_types.h"
#include "runtime/kernel.h"
#include "hccl_common.h"

namespace hccl {
constexpr u32 AIV_ALL_REDUCE_BIG_SIZE = 16 * 1024 * 1024;
constexpr u32 AIV_UB_MAX_DATA_SIZE = 190 * 1024;
constexpr u32 MAX_RANK_SIZE = 8; // 910B server内卡数

using AivTagArray = std::array<s32, MAX_RANK_SIZE + MAX_RANK_SIZE>;

struct AlltoAllExtraArgs {
    u64 sendCountMatrix[MAX_RANK_SIZE * MAX_RANK_SIZE] = {};
    u64 sendCounts[MAX_RANK_SIZE] = {};
    u64 sendDispls[MAX_RANK_SIZE] = {};
    u64 recvCounts[MAX_RANK_SIZE] = {};
    u64 recvDispls[MAX_RANK_SIZE] = {};
    u64 maxCount = 0;
};

HcclResult RegisterKernel(DevType deviceType);

HcclResult ExecuteKernelLaunch(HcclCMDType cmdType, const void* input, const void* output, u64 count,
    HcclDataType dataType, HcclReduceOp op, u32 rank, u32 rankSize, u32 root, void** cclBuffersIn, void** cclBuffersOut,
    const std::string &tagKey, rtStream_t stream, bool isOpBase = true, u64 bufferSize = 200 * 1024 * 1024,
    s32 aivRdmaStep = -1, bool useAivRdmaSmall = false, const AlltoAllExtraArgs* extraArgsPtr = nullptr, u32 devId = 16);
}
#endif // HCCL_AIV_H