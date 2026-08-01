/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
 * Description: 提供Rank间数据一致性校验功能
 */

#ifndef RANK_CONSISTENT_H
#define RANK_CONSISTENT_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <hccl/hccl_types.h>

#include "hccl_common.h"
#include "externalinput_pub.h"

namespace hccl {
constexpr u32 DEFAULT_CRC_VALUE = 0xFFFFFFFF; // CRC默认值

class RankConsistentImpl;
class RankConsistent {
public:
    explicit RankConsistent();
    virtual ~RankConsistent();

    static RankConsistent& GetInstance(s32 deviceLogicId = 0xFF);

    // all gather
    HcclResult RecordOpPara(HcclCMDType opCMD, const std::string &tag, u64 count, HcclDataType dataType,
        u64 inCclBufferSize, u64 outCclBufferSize, const char *group = nullptr, u32 crc = DEFAULT_CRC_VALUE);
    // all reduce
    HcclResult RecordOpPara(HcclCMDType opCMD, const std::string &tag, u64 count, HcclDataType dataType,
        HcclReduceOp op, u64 inCclBufferSize, u64 outCclBufferSize, const char *group = nullptr,
        u32 crc = DEFAULT_CRC_VALUE);
    // broadcast
    HcclResult RecordOpPara(HcclCMDType opCMD, const std::string &tag, u64 count, HcclDataType dataType, u32 root,
        u64 inCclBufferSize, u64 outCclBufferSize, const char *group = nullptr, u32 crc = DEFAULT_CRC_VALUE);
    // reduce
    HcclResult RecordOpPara(HcclCMDType opCMD, const std::string &tag, u64 count, HcclDataType dataType,
        HcclReduceOp op, u32 root, u64 inCclBufferSize, u64 outCclBufferSize, const char *group = nullptr,
        u32 crc = DEFAULT_CRC_VALUE);
    // send && receive
    HcclResult RecordOpPara(HcclCMDType opCMD, const std::string &tag, u64 count, HcclDataType dataType, u32 rank,
        u32 srTag, u32 selfRank, u64 inCclBufferSize, u64 outCclBufferSize, const char *group,
        u32 crc = DEFAULT_CRC_VALUE);
    // batchsendrecv
    HcclResult RecordOpPara(HcclCMDType opCMD, const std::string &tag, u64 inCclBufferSize, u64 outCclBufferSize,
        const char *group = nullptr, u32 crc = DEFAULT_CRC_VALUE);
    HcclResult DelOpPara(const std::string &tag);

    HcclResult RecordVerInfo(const std::string &versionInfo);

    u64 GetRankConsistentDataLength();

    void RecordProtocolType(ProtocolType protocolType);

    HcclResult GetCheckFrame(u8 *destBuf, u64 maxDestBuf, const std::string &tag);

    HcclResult CheckFrameRecv(const u8 *recvBuf, u32 recvBufLen, const std::string &tag);

    void ClearCheckInfo();

    HcclResult RecordRankTableCrc(const char *rankTable);

    HcclResult CalcStringCrc(const char *str, u32 &crc);

    void SetCheckCannVersionSwitch(const bool cannVerCheckSwitch);

private:
    std::unique_ptr<RankConsistentImpl> pimpl_;
};
}
#endif
