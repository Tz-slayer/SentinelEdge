/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: profiler manager impl head
 */

#ifndef PROFILER_MANAGER_H
#define PROFILER_MANAGER_H
#include <hccl/hccl_types.h>
#include "hccl_common.h"
#include "profiler_base_pub.h"

namespace hccl {
class ProfilerManagerImpl;
class ProfilerManager {
public:
    ProfilerManager(s32 devicePhyId, s32 deviceLogicId, u32 realUserRank);
    ~ProfilerManager();
    HcclResult InitProfiler();
    HcclResult GetandClearOverFlowTasks(std::vector<HcclDumpInfo> &hcclDumpInfo);
    void TaskSdmaProfiler(ProfilerType profilerType, HcclRtStream stream, TaskParaDMA &para);
    void TaskRdmaProfiler(ProfilerType profilerType, HcclRtStream stream, TaskParaDMA &para);
    void TaskReduceInlineProfiler(ProfilerType profilerType, HcclRtStream stream, TaskParaReduce &para);
    void TaskReduceTbeProfiler(ProfilerType profilerType, HcclRtStream stream, TaskParaReduce &para);
    void TaskRecordProfiler(ProfilerType profilerType, HcclRtStream stream, TaskParaNotify &para);
    void TaskWaitProfiler(ProfilerType profilerType, HcclRtStream stream, TaskParaNotify &para);
    void TaskProfiler(ProfilerType profilerType, HcclRtStream stream);
    void TaskProfiler(ProfilerType profilerType, TaskParaHost &para);
private:
    std::unique_ptr<ProfilerManagerImpl> pimpl_;
};
} // namespace hccl
#endif