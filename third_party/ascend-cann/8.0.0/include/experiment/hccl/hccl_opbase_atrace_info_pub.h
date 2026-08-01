/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: Atrace 管理类定义
 */

#ifndef HCCL_OPBASE_ATRACE_INFO_PUB_H
#define HCCL_OPBASE_ATRACE_INFO_PUB_H

#include "hccl_common.h"
#include "hccl/base.h"
#include <string>

namespace hccl {

enum class AtraceOption {
    Opbasekey,
    Algtype
};

class HcclOpBaseAtraceInfo {
public:
    HcclOpBaseAtraceInfo();
    ~HcclOpBaseAtraceInfo();
    uint32_t index;
    HcclTraHandle handle{0};

    HcclResult Init(std::string &logInfo);
    void DeInit();
    HcclResult SaveOpbaseKeyTraceInfo(std::string &logInfo, AtraceOption op);
    HcclResult SavealgtypeTraceInfo(std::string &algtype, const std::string &tag);
};
}

#endif // HCCL_OPBASE_ATRACE_INFO_PUB_H