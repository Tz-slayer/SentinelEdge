/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 * Description: reporter for hccl
 */

#ifndef COMMON_PROFILING_PROFILING_MANAGER_PUB_H
#define COMMON_PROFILING_PROFILING_MANAGER_PUB_H

#include <string>
#include <cstdio>
#include "hccl_common.h"
#include "profiler_base_pub.h"
#include "adapter_prof_pub.h"

namespace hccl {

class ProfilingManagerPub {
public:
    static HcclResult CallMsprofReportMultiThreadInfo(const std::vector<uint32_t> &tidInfo);
    static HcclResult GetAddtionInfoState();
    static HcclResult GetTaskApiState();
    static HcclResult CallMsprofReportHostApi(HcclCMDType cmdType, uint64_t beginTime, u64 count, HcclDataType dataType,
        AlgType algType, uint64_t groupName);
    static HcclResult CallMsprofReportMc2CommInfo(uint64_t timeStamp, const void *data, int len);
    static HcclResult CallMsprofReportHostNodeApi(uint64_t beginTime, uint64_t endTime, const std::string profName,
        uint32_t threadId);
    static HcclResult CallMsprofReportHostNodeBasicInfo(uint64_t endTime, const std::string profName,
        uint32_t threadId);
    static bool GetAllState();
    static HcclResult ClearStoragedProfilingInfo();
};
} // namespace hccl
#endif // COMMON_PROFILING_PROFILING_MANAGER_H
