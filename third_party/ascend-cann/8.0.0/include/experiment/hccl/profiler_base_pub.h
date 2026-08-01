/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 * Description: HCCL的profiler虚基类头文件，用于派生task记录类
 */

#ifndef PROFILER_BASE_PUB_H
#define PROFILER_BASE_PUB_H

#include <memory>
#include <mutex>
#include <string>
#include <map>
#include <thread>

#include <hccl/hccl_types.h>
#include "hccl_common.h"
#include "workflow_pub.h"

namespace hccl {
enum class StepType {
    STEP_STAGE = 0,
    STEP_STEP,
    STEP_MAX
};

enum class OpDict {
    SUM = 0,
    PROD,
    MAX,
    MIN
};

enum class DataType {
    DINT8 = 0,
    DINT16,
    DINT32,
    DFP16,
    DFP32,
    DINT64,
    DUINT64
};

enum class TaskType {
    TASK_SDMA = 0,
    TASK_RDMA,
    TASK_REDUCE_INLINE,
    TASK_REDUCE_TBE,
    TASK_NOTIFY_RECORD,
    TASK_NOTIFY_WAIT
};

enum class SimpleTaskType {
    SDMA = 0,
    RDMA,
    LOCAL
};

enum class TaskRole {
    DST = 0,
    SRC
};

enum class LinkType {
    LINK_ONCHIP = 0,
    LINK_HCCS,
    LINK_PCIE,
    LINK_ROCE,
    LINK_SIO,
    LINK_HCCS_SW,
    LINK_STANDARD_ROCE,
    LINK_RESERVED
};

enum class RdmaType {
    RDMA_SEND_NOTIFY = 0,
    RDMA_SEND_PAYLOAD,
    RDMA_TYPE_RESERVED
};

enum class ProfilerType {
    TASK_PROFILING = 0,
    TASK_EXCEPTION = 1,
    TASK_OVERFLOW,
    TASK_ALL,
    TASK_RESERVE
};

struct StepData {
    s32 streamID;
    s32 planeID; // bit[31..28] = 节点内8P-ring对应的物理环, bit[27..16] = rank_size, bit[15..0] = rank_id
    s32 stage;
    s32 step;

    StepData() : streamID(0), planeID(0), stage(0), step(-1) {}
};

struct TaskParaDMA {
    const void *src{nullptr};
    const void *dst{nullptr};
    std::size_t size{0};
    u64 notifyID{INVALID_U64};
    LinkType linkType{LinkType::LINK_ONCHIP};
    u32 remoteUserRank{INVALID_VALUE_RANKID};
    RdmaType rdmaType{RdmaType::RDMA_TYPE_RESERVED};
    u32 ctxId{INVALID_UINT}; // 子图 ctxId信息
    TaskParaDMA() {}

    TaskParaDMA(const void *inputSrc, const void *inputDst, std::size_t inputSize)
        : src(inputSrc),
          dst(inputDst),
          size(inputSize),
          notifyID(INVALID_U64),
          linkType(LinkType::LINK_ONCHIP),
          remoteUserRank(INVALID_VALUE_RANKID),
          rdmaType(RdmaType::RDMA_TYPE_RESERVED),
          ctxId(INVALID_UINT)
    {}

    TaskParaDMA(const void *inputSrc, const void *inputDst, std::size_t inputSize, u64 inputNotifyID)
        : src(inputSrc),
          dst(inputDst),
          size(inputSize),
          notifyID(inputNotifyID),
          linkType(LinkType::LINK_ONCHIP),
          remoteUserRank(INVALID_VALUE_RANKID),
          rdmaType(RdmaType::RDMA_TYPE_RESERVED),
          ctxId(INVALID_UINT)
    {}

    TaskParaDMA(const void *inputSrc, const void *inputDst, std::size_t inputSize, u64 inputNotifyID,
        LinkType inputLinkType, RdmaType inputRdmaType)
        : src(inputSrc),
          dst(inputDst),
          size(inputSize),
          notifyID(inputNotifyID),
          linkType(inputLinkType),
          remoteUserRank(static_cast<u32>(inputNotifyID >> 32)),  // notifyID的高32位为对端usrrank
          rdmaType(inputRdmaType),
          ctxId(INVALID_UINT)
    {}
    TaskParaDMA(const void *inputSrc, const void *inputDst, std::size_t inputSize, LinkType inputLinkType,
        u32 remoteUserRank) // 无notifyID时需要传入remoteUserRank
        : src(inputSrc),
          dst(inputDst),
          size(inputSize),
          notifyID(INVALID_U64),
          linkType(inputLinkType),
          remoteUserRank(remoteUserRank),
          rdmaType(RdmaType::RDMA_TYPE_RESERVED),
          ctxId(INVALID_UINT)
    {}
    TaskParaDMA(const void *inputSrc, const void *inputDst, std::size_t inputSize, u64 inputNotifyID,
        LinkType inputLinkType, RdmaType inputRdmaType, u32 inputCtxId)
        : src(inputSrc),
          dst(inputDst),
          size(inputSize),
          notifyID(inputNotifyID),
          linkType(inputLinkType),
          remoteUserRank(static_cast<u32>(inputNotifyID >> 32)),  // notifyID的高32位为对端usrrank
          rdmaType(inputRdmaType),
          ctxId(inputCtxId)
    {}
    TaskParaDMA(const void *inputSrc, const void *inputDst, std::size_t inputSize, LinkType inputLinkType,
        u32 remoteUserRank, RdmaType inputRdmaType, u32 inputCtxId)
        : src(inputSrc),
          dst(inputDst),
          size(inputSize),
          notifyID(INVALID_U64),
          linkType(inputLinkType),
          remoteUserRank(remoteUserRank),
          rdmaType(inputRdmaType),
          ctxId(inputCtxId)
    {}
};

struct TaskParaReduce {
    const void *src;
    const void *dst;
    std::size_t size;

    HcclReduceOp op;
    HcclDataType dataType;
    LinkType linkType;
    u32 remoteUserRank;
    u32 ctxId; // 子图 ctxId信息
    TaskParaReduce()
        : src(nullptr),
          dst(nullptr),
          size(0),
          op(HCCL_REDUCE_RESERVED),
          dataType(HCCL_DATA_TYPE_RESERVED),
          linkType(LinkType::LINK_ONCHIP),
          remoteUserRank(INVALID_UINT),
          ctxId(INVALID_UINT)
    {}

    TaskParaReduce(const void *inputSrc, const void *inputDst, std::size_t inputSize, HcclReduceOp inputOp,
        HcclDataType inputDataType)
        : src(inputSrc),
          dst(inputDst),
          size(inputSize),
          op(inputOp),
          dataType(inputDataType),
          linkType(LinkType::LINK_ONCHIP),
          remoteUserRank(INVALID_UINT),
          ctxId(INVALID_UINT)
    {}

    TaskParaReduce(const void *inputSrc, const void *inputDst, std::size_t inputSize, HcclReduceOp inputOp,
        HcclDataType inputDataType, LinkType inputLinkType)
        : src(inputSrc),
          dst(inputDst),
          size(inputSize),
          op(inputOp),
          dataType(inputDataType),
          linkType(inputLinkType),
          remoteUserRank(INVALID_UINT),
          ctxId(INVALID_UINT)
    {}
    TaskParaReduce(const void *inputSrc, const void *inputDst, std::size_t inputSize, HcclReduceOp inputOp,
        HcclDataType inputDataType, LinkType inputLinkType, u32 remoteUserRank)
        : src(inputSrc),
          dst(inputDst),
          size(inputSize),
          op(inputOp),
          dataType(inputDataType),
          linkType(inputLinkType),
          remoteUserRank(remoteUserRank),
          ctxId(INVALID_UINT)
    {}
    TaskParaReduce(const void *inputSrc, const void *inputDst, std::size_t inputSize, HcclReduceOp inputOp,
        HcclDataType inputDataType, LinkType inputLinkType, u32 remoteUserRank, u32 inputCtxId)
        : src(inputSrc),
          dst(inputDst),
          size(inputSize),
          op(inputOp),
          dataType(inputDataType),
          linkType(inputLinkType),
          remoteUserRank(remoteUserRank),
          ctxId(inputCtxId)
    {}
};

struct TaskParaNotify {
    u64 notifyID;
    s32 stage; // 用于标识stream间同步时所在的stage， 非用于stream同步的默认为-1
    u32 remoteUserRank;
    u32 ctxId; // 子图 ctxId信息
    TaskParaNotify() : notifyID(0), stage(INVALID_VALUE_STAGE), remoteUserRank(INVALID_UINT), ctxId(INVALID_UINT) {}
    explicit TaskParaNotify(u64 notifyIDInput)
        : notifyID(notifyIDInput),
          stage(INVALID_VALUE_STAGE),
          remoteUserRank(static_cast<u32>(notifyIDInput >> 32)), // 无remoteRank时，使用notify的高32位为remote rank
          ctxId(INVALID_UINT)
    {}
    TaskParaNotify(u64 notifyIDInput, s32 stageIn)
        : notifyID(notifyIDInput),
          stage(stageIn),
          remoteUserRank(static_cast<u32>(notifyIDInput >> 32)), // 无remoteRank时，使用notify的高32位为remote rank
          ctxId(INVALID_UINT)
    {}
    TaskParaNotify(u64 notifyIDInput, s32 stageIn, u32 remoteUserRank)
        : notifyID(notifyIDInput), stage(stageIn), remoteUserRank(remoteUserRank), ctxId(INVALID_UINT)
    {}
    TaskParaNotify(u64 notifyIDInput, s32 stageIn, u32 remoteUserRank, u32 ctxIdInput)
        : notifyID(notifyIDInput), stage(stageIn), remoteUserRank(remoteUserRank), ctxId(ctxIdInput)
    {}
};

struct TaskParaHost {
    u32 streamID;
    u32 taskID;
    u64 len;
    std::chrono::microseconds duration;
    std::string tag;
    TaskParaHost(u32 streamID, u32 taskID, u64 len, std::chrono::microseconds duration, std::string &tag)
        : streamID(streamID),
          taskID(taskID),
          len(len),
          duration(duration),
          tag(tag)
    {}
};

struct GroupRankInfo {
    u32 rankSize{0};
    u32 rankId{0};
    u32 remoteRankId{INVALID_VALUE_RANKSIZE};
};

struct OpDataInfo {
    u64 count{0};
    const void *src{nullptr};
    const void *dst{nullptr};
    u32 index{0};
    u32 rootId{0};
    u32 deviceId{0};
    HcclDataType dataType{HcclDataType::HCCL_DATA_TYPE_RESERVED};
    HcclReduceOp reduceType{HcclReduceOp::HCCL_REDUCE_RESERVED};
    struct timeval tv{0};
};

#define HCCL_PROFILER_ADD_STREAM_BY_STREAMID(streamId, tag, planeID, algType)                    \
    do {                                                                             \
        HcclResult __ret = ProfilerBase::AddStream(streamId, tag, planeID, algType); \
        if (UNLIKELY(__ret != 0)) {                                                  \
            HCCL_ERROR("[Add][ProfilerStream]profiler add plane error[%d]", __ret);  \
            return HCCL_E_INTERNAL;                                                  \
        }                                                                            \
    } while (0)

#define HCCL_PROFILER_DEL_STREAM_BY_STREAMID(streamId)                                             \
    do {                                                                               \
        HcclResult __ret = ProfilerBase::DelStream(streamId);                          \
        if (UNLIKELY(__ret != 0)) {                                                    \
            HCCL_ERROR("[Del][ProfilerStream]profiler delete plane error[%d]", __ret); \
            return HCCL_E_INTERNAL;                                                    \
        }                                                                              \
    } while (0)

#define HCCL_PROFILER_ADD_STREAM(stream, tag, planeID, algType)                     \
    do {                                                                            \
        s32 streamId = 0;                                                           \
        CHK_RET(hrtGetStreamId(stream, streamId));                                  \
        HcclResult __ret = ProfilerBase::AddStream(streamId, tag, planeID, algType);\
        if (UNLIKELY(__ret != 0)) {                                                 \
            HCCL_ERROR("[Add][ProfilerStream]profiler add plane error[%d]", __ret); \
            return HCCL_E_INTERNAL;                                                 \
        }                                                                           \
    } while (0)

#define HCCL_PROFILER_DEL_STREAM(stream)                                                    \
    do {                                                                                    \
        s32 streamId = 0;                                                                   \
        CHK_RET(hrtGetStreamId(stream, streamId));                                          \
        HcclResult __ret = ProfilerBase::DelStream(streamId);                               \
        if (UNLIKELY(__ret != 0)) {                                                         \
            HCCL_ERROR("[Del][ProfilerStream]profiler delete plane error[%d]", __ret);      \
            return HCCL_E_INTERNAL;                                                         \
        }                                                                                   \
    } while (0)

#define HCCL_PROFILER_ADD_TAG(tag, group, workFlowMode)                        \
    do {                                                                       \
        HcclResult __ret = ProfilerBase::AddTag(tag, group, workFlowMode); \
        if (UNLIKELY(__ret != 0)) {                                        \
            HCCL_ERROR("profiler add tag error[%d]", __ret);               \
            return HCCL_E_INTERNAL;                                        \
        }                                                                  \
    } while (0)

#define HCCL_PROFILER_ADD_TAG_SENDRECV(tag, group, workFlowMode)               \
    do {                                                                       \
        HcclResult __ret = ProfilerBase::AddTag(tag, group, workFlowMode, true); \
        if (UNLIKELY(__ret != 0)) {                                        \
            HCCL_ERROR("profiler add tag error[%d]", __ret);               \
            return HCCL_E_INTERNAL;                                        \
        }                                                                  \
    } while (0)

#define HCCL_PROFILER_DEL_TAG(tag)                                  \
    do {                                                            \
        HcclResult __ret = ProfilerBase::DelTag(tag);           \
        if (UNLIKELY(__ret != 0)) {                             \
            HCCL_ERROR("profiler delete tag error[%d]", __ret); \
            return HCCL_E_INTERNAL;                             \
        }                                                       \
    } while (0)

// 兼容性考虑，需保留
#define HCCL_PROFILER_ADD_OPDATA(tag, count, src, dst, dataType, rootId, group)                \
    do {                                                                       \
        HcclResult __ret = ProfilerBase::AddOpData(tag, count, src, dst, dataType, rootId, group); \
        if (UNLIKELY(__ret != 0)) {                                        \
            HCCL_ERROR("profiler add opData error[%d]", __ret);               \
            return HCCL_E_INTERNAL;                                        \
        }                                                                  \
    } while (0)

#define HCCL_PROFILER_ADD_OPDATA_OP(tag, count, src, dst, dataType, rootId, group, reduceType)                \
    do {                                                                       \
        HcclResult __ret = ProfilerBase::AddOpData(tag, count, src, dst, dataType, rootId, group, reduceType); \
        if (UNLIKELY(__ret != 0)) {                                        \
            HCCL_ERROR("profiler add opData error[%d]", __ret);               \
            return HCCL_E_INTERNAL;                                        \
        }                                                                  \
    } while (0)

#define HCCL_PROFILER_DEL_OPDATA(tag)                                  \
    do {                                                            \
        HcclResult __ret = ProfilerBase::DelOpData(tag);           \
        if (UNLIKELY(__ret != 0)) {                             \
            HCCL_ERROR("profiler delete opData error[%d]", __ret); \
            return HCCL_E_INTERNAL;                             \
        }                                                       \
    } while (0)

#define HCCL_PROFILER_ADD_GROUPRANK(group, rankSize, rankId)                        \
    do {                                                                       \
        HcclResult __ret = ProfilerBase::AddGroupRankInfo(group, rankSize, rankId); \
        if (UNLIKELY(__ret != 0)) {                                        \
            HCCL_ERROR("profiler add groupRank error[%d]", __ret);               \
            return HCCL_E_INTERNAL;                                        \
        }                                                                  \
    } while (0)

#define HCCL_PROFILER_ADD_GROUPRANK_SENDRECV(group, rankSize, rankId, remoteRankId)                        \
    do {                                                                       \
        HcclResult __ret = ProfilerBase::AddGroupRankInfo(group, rankSize, rankId, true, remoteRankId); \
        if (UNLIKELY(__ret != 0)) {                                        \
            HCCL_ERROR("profiler add groupRank error[%d]", __ret);               \
            return HCCL_E_INTERNAL;                                        \
        }                                                                  \
    } while (0)

#define HCCL_PROFILER_DEL_GROUPRANK(group)                                  \
    do {                                                            \
        HcclResult __ret = ProfilerBase::DelGroupRankInfo(group);           \
        if (UNLIKELY(__ret != 0)) {                             \
            HCCL_ERROR("profiler delete groupRank error[%d]", __ret); \
            return HCCL_E_INTERNAL;                             \
        }                                                       \
    } while (0)

#define HCCL_PROFILER_ADD_GROUP_UDI(group, udi)                   \
    do {                                                            \
        HcclResult __ret = ProfilerBase::AddGroupUdi(group, udi);           \
        if (UNLIKELY(__ret != 0)) {                             \
            HCCL_ERROR("profiler add groupUdi error[%d]", __ret); \
            return HCCL_E_INTERNAL;                             \
        }                                                       \
    } while (0)

#define HCCL_PROFILER_DEL_GROUP_UDI(group)                          \
    do {                                                            \
        HcclResult __ret = ProfilerBase::DelGroupUdi(group);           \
        if (UNLIKELY(__ret != 0)) {                             \
            HCCL_ERROR("profiler del groupUdi error[%d]", __ret); \
            return HCCL_E_INTERNAL;                             \
        }                                                       \
    } while (0)

class ProfilerBase {
public:
    /* * 输出文本时, 获取op, dataType的字符串以及单位数据长度的数组 */
    static const std::array<uint32_t, HCCL_REDUCE_RESERVED> opString;
    static const std::array<uint32_t, HCCL_DATA_TYPE_RESERVED> dataTypeString;
    static const std::array<s32, HCCL_DATA_TYPE_RESERVED> sizeOf;

    explicit ProfilerBase(u32 deviceLogicId);
    virtual ~ProfilerBase();

    virtual HcclResult Run(const StepData &stepData) = 0;
    virtual HcclResult Flush() = 0;
    static HcclResult AddStream(s32 streamID, const std::string &tag, s32 planeID, AlgType algType);
    static HcclResult DelStream(s32 streamID);
    static HcclResult AddTag(const std::string &tag, const std::string &group, const HcclWorkflowMode &workFlowMode,
        bool isSendRecv = false);
    static HcclResult DelTag(const std::string &tag);
    static HcclResult AddOpData(const std::string &tag, u64 count, const void *src, const void *dst,
        HcclDataType dataType, u32 rootId, const std::string &group, HcclReduceOp reduceType = HCCL_REDUCE_RESERVED);
    static HcclResult DelOpData(const std::string &tag);
    static HcclResult AddGroupRankInfo(const std::string &group, u32 rankSize, u32 rankId, bool isSendRecv = false,
        u32 remoteRankId = INVALID_VALUE_RANKSIZE);
    static HcclResult DelGroupRankInfo(const std::string &tag);
    static HcclResult GetTagByStream(u32 &streamID, std::string &tag);
    static HcclResult GetAlgTypeByStream(u32 &streamID, AlgType &algType);
    static HcclResult GetGroupNameByTag(const std::string &tag, std::string &group);
    static HcclResult GetRankInfoByGroup(const std::string &group, GroupRankInfo &groupRankInfo);
    static HcclResult GetOpDataInfoByTag(const std::string &tag, OpDataInfo &opDataInfo);
    static HcclResult AddGroupUdi(const std::string &group, const std::string &udi);
    static HcclResult DelGroupUdi(const std::string &group);
    static HcclResult GetUdiByGroup(const std::string &group, std::string &udi);
    static void GetSubmittedOpCnt(u32 &index);
    virtual HcclResult Save(u32 &streamID, u32 &taskID, TaskType &taskType, const TaskParaDMA &para) = 0;
    virtual HcclResult Save(u32 &streamID, u32 &taskID, TaskType &taskType, const TaskParaReduce &para) = 0;
    virtual HcclResult Save(u32 &streamID, u32 &taskID, TaskType &taskType, const TaskParaNotify &para) = 0;
    virtual HcclResult Save(u32 &streamID, u32 &taskID) = 0;
    virtual HcclResult SaveToLog(const TaskParaHost &paraHost) = 0;

protected:
    static std::array<std::map<s32, s32>, MAX_MODULE_DEVICE_NUM> streamPlaneMap_;
    static std::array<std::map<s32, const std::string>, MAX_MODULE_DEVICE_NUM> streamTagMap_;
    static std::array<std::map<s32, AlgType>, MAX_MODULE_DEVICE_NUM> streamAlgTypeMap_;
    static std::array<std::map<const std::string, const std::string>, MAX_MODULE_DEVICE_NUM> tagGroupMap_;
    static std::array<std::map<const std::string, const HcclWorkflowMode>, MAX_MODULE_DEVICE_NUM> tagModeMap_;
    static std::array<std::mutex, MAX_MODULE_DEVICE_NUM> streamMutex_;
    static std::array<std::map<const std::string, GroupRankInfo>, MAX_MODULE_DEVICE_NUM> groupRankMap_;
    static std::array<std::map<const std::string, OpDataInfo>, MAX_MODULE_DEVICE_NUM> tagOpDataMap_;
    static std::array<std::map<const std::string, u32>, MAX_MODULE_DEVICE_NUM> groupIndexMap_;
    static std::array<std::map<const std::string, u32>, MAX_MODULE_DEVICE_NUM> sendRecvGroupIndexMap_;
    static std::array<std::map<const std::string, std::string>, MAX_MODULE_DEVICE_NUM> groupUdiMap_;
    const u32 deviceLogicId_;
    static bool isSendRecv_[MAX_MODULE_DEVICE_NUM];
    static u32 index_[MAX_MODULE_DEVICE_NUM];

private:
};
} // namespace hccl

#endif /* PROFILER_BASE_PUB_H */
