/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: TransportPub 头文件
 */

#ifndef TRANSPORT_PUB_H
#define TRANSPORT_PUB_H

#include <initializer_list>
#include <hccl/hccl_types.h>
#include "hccl_common.h"
#include "sal_pub.h"
#include "adapter_pub.h"
#include "stream_pub.h"
#include "dispatcher.h"
#include "hccl_socket.h"
#include "notify_pool.h"
#include "local_notify.h"
#include "remote_notify.h"


struct HcclQpInfoV2 {
    u64 qpPtr;
    u32 sqIndex;
    u32 dbIndex;

    HcclQpInfoV2() : qpPtr(0), sqIndex(0), dbIndex(0)
    {}
    HcclQpInfoV2(const HcclQpInfoV2 &other) : qpPtr(other.qpPtr), sqIndex(other.sqIndex), dbIndex(other.dbIndex)
    {}
    HcclQpInfoV2(HcclQpInfoV2 &&other) : qpPtr(other.qpPtr), sqIndex(other.sqIndex), dbIndex(other.dbIndex)
    {}
    HcclQpInfoV2 &operator=(const HcclQpInfoV2 &other)
    {
        if(&other != this) {
            qpPtr = other.qpPtr;
            sqIndex = other.sqIndex;
            dbIndex = other.dbIndex;
        }
        return *this;
    }
    HcclQpInfoV2 &operator=(HcclQpInfoV2 &&other)
    {
        if(&other != this) {
            qpPtr = other.qpPtr;
            sqIndex = other.sqIndex;
            dbIndex = other.dbIndex;
        }
        return *this;
    }
};

struct AddrKey {
    u64 addr;
    u32 key;
};

struct MemDetails {
    u64 size = 0;
    u64 addr = 0;
    u32 key = 0;

    MemDetails()
    {}
    MemDetails(const MemDetails &that) : size(that.size), addr(that.addr),
        key(that.key)
    {}

    MemDetails(MemDetails &&that) : size(that.size), addr(that.addr),
        key(that.key)
    {}
    MemDetails &operator=(const MemDetails &that)
    {
        if (&that != this) {
            size = that.size;
            addr = that.addr;
            key = that.key;
        }
        return *this;
    }

    MemDetails operator=(MemDetails &&that)
    {
        if (&that != this) {
            size = that.size;
            addr = that.addr;
            key = that.key;
        }
        return *this;
    }
};

namespace hccl {

class TransportBase;
struct RxMemoryInfo;
struct TxMemoryInfo;
enum class UserMemType;
class DeviceMem;
class MemNameRepository;

enum class MachineType {
    MACHINE_SERVER_TYPE,
    MACHINE_CLIENT_TYPE,
    MACHINE_RESERVED_TYPE
};
enum class LinkMode {
    LINK_SIMPLEX_MODE,
    LINK_DUPLEX_MODE,
    LINK_RESERVED_MODE
};

// signal record 使用的value的内存信息
// 使用SDMDA或者RDMA进行notify record时需要将该内存copy到远端 notify 寄存器
using HcclSignalRecordBuff = struct HcclSignalRecordBuffDef {
    u64 address{0};     //  signal 地址
    u64 length{0};      //  signal 长度
};

constexpr u32 HCCL_TRANSPORT_RELATIONSHIP_SAME_CHIP = 0x1U << 0;        // transport 的两端rank位于同一个NPU芯片内
constexpr u32 HCCL_TRANSPORT_RELATIONSHIP_SAME_SERVER = 0x1U << 1;      // transport 的两端rank位于同一个服务器内
constexpr u32 HCCL_TRANSPORT_RELATIONSHIP_SAME_SUPERPOD = 0x1U << 2;    // transport 的两端rank位于同一个超节点内

// transport 使用的基础信息，包括链路类型、和远端的位置关系等
using TransportAttr = struct TransportAttrDef {
    hccl::LinkType linkType{hccl::LinkType::LINK_RESERVED};  // 链路类型，HCCS,
    u32 relationship{0}; // 和remote的位置关系{同芯片，同节点，跨节点}
    HcclSignalRecordBuff signalRecordBuff;
};

// 传参数的时候都填充，link自己使用的时候区分
// RDMA:  machineType+serverId+local_rank_id+remote_rank_id+collectiveId
// TCP:   machineType+serverId+local_rank_id+remote_rank_id+collectiveId
// PCIE:  local_rank_id+remote_rank_id+collectiveId+localDeviceId+remoteDeviceId
using MachinePara = struct TagMachinePara {
public:
    MachineType machineType{MachineType::MACHINE_RESERVED_TYPE};  // client或者server
    LinkMode linkMode{LinkMode::LINK_RESERVED_MODE};
    std::string collectiveId{""};  // 本节点所在的通信域ID
    std::string tag{""};
    std::string serverId;                       // 本端server id

    HcclIpAddress localIpAddr;     // 本端rank ip
    HcclIpAddress remoteIpAddr;    // 对端rank ip

    u32 localSocketPort;
    u32 remoteSocketPort;

    s32 localDeviceId{-1};                      // 本端device physical id
    s32 remoteDeviceId{-1};                     // 对端device physical id
    s32 deviceLogicId;

    u32 localUserrank{INVALID_VALUE_RANKID};    // 本端user rank
    u32 remoteUserrank{INVALID_VALUE_RANKID};   // 对端user rank

    u32 localWorldRank{INVALID_VALUE_RANKID};   // 本端world group rank
    u32 remoteWorldRank{INVALID_VALUE_RANKID};  // 对端world group rank

    NICDeployment nicDeploy{NICDeployment::NIC_DEPLOYMENT_DEVICE};
    DevType deviceType{DevType::DEV_TYPE_COUNT};

    std::vector<std::shared_ptr<HcclSocket> > sockets;

    DeviceMem inputMem{DeviceMem()};
    DeviceMem outputMem{DeviceMem()};

    // link特性位图: bit0:0x1支持WRITE操作（源端发起数据传输，优选项）
    // bit1:0x2支持READ操作（目的端发起数据传输）。如果同时支持link优先选用目的端发起数据传输
    u64 linkAttribute{0x1}; // 初始设置为WRITE操作，从源端发起数据传输;

    bool supportDataReceivedAck{false};
    bool isAicpuModeEn{false};
    std::vector<u32> srcPorts; // 多qp配置的源端口号
    TagMachinePara() {}

    TagMachinePara(const struct TagMachinePara &that)
    {
        machineType = (that.machineType);
        linkMode = (that.linkMode);
        serverId = (that.serverId);
        localIpAddr = (that.localIpAddr);
        remoteIpAddr = (that.remoteIpAddr);
        localDeviceId = (that.localDeviceId);
        remoteDeviceId = (that.remoteDeviceId);
        localUserrank = (that.localUserrank);
        remoteUserrank = (that.remoteUserrank);
        localWorldRank = (that.localWorldRank);
        remoteWorldRank = (that.remoteWorldRank);
        collectiveId = (that.collectiveId);
        deviceType = (that.deviceType);
        tag = (that.tag);
        inputMem = (that.inputMem);
        outputMem = (that.outputMem);
        linkAttribute = (that.linkAttribute);
        sockets = (that.sockets);
        supportDataReceivedAck = (that.supportDataReceivedAck);
        nicDeploy = (that.nicDeploy);
        localSocketPort = that.localSocketPort;
        remoteSocketPort = that.remoteSocketPort;
        isAicpuModeEn = that.isAicpuModeEn;
        deviceLogicId = that.deviceLogicId;
        srcPorts = that.srcPorts;
    }

    struct TagMachinePara &operator=(struct TagMachinePara &that)
    {
        if (&that != this) {
            machineType = (that.machineType);
            linkMode = (that.linkMode);
            serverId = (that.serverId);
            localIpAddr = (that.localIpAddr);
            remoteIpAddr = (that.remoteIpAddr);
            localDeviceId = (that.localDeviceId);
            remoteDeviceId = (that.remoteDeviceId);
            localUserrank = (that.localUserrank);
            remoteUserrank = (that.remoteUserrank);
            localWorldRank = (that.localWorldRank);
            remoteWorldRank = (that.remoteWorldRank);
            collectiveId = (that.collectiveId);
            deviceType = (that.deviceType);
            tag = (that.tag);
            inputMem = (that.inputMem);
            outputMem = (that.outputMem);
            linkAttribute = (that.linkAttribute);
            sockets = (that.sockets);
            supportDataReceivedAck = (that.supportDataReceivedAck);
            localSocketPort = that.localSocketPort;
            remoteSocketPort = that.remoteSocketPort;
            isAicpuModeEn = that.isAicpuModeEn;
            deviceLogicId = that.deviceLogicId;
            srcPorts = that.srcPorts;
        }

        return *this;
    }
};

struct TransportPara {
    std::chrono::milliseconds timeout;
    NICDeployment nicDeploy;
    u32 localDieID;
    u32 dstDieID;
    HcclIpAddress* selfIp;
    HcclIpAddress* peerIp;
    u32 peerPort;
    u32 selfPort;
    const void* transportResourceInfoAddr;
    size_t transportResourceInfoSize;
    u32 index;
    bool isRootRank;
    u32 devLogicId;
    u32 proxyDevLogicId;
    s32 qpMode = 0;
    bool isHdcMode = false;
    bool remoteIsHdc = false;
    bool isESPs = false;
    bool virtualFlag = false;
};

struct TransportDeviceP2pData {
    void *inputBufferPtr;
    void *outputBufferPtr;
    std::shared_ptr<LocalIpcNotify> ipcPreWaitNotify;
    std::shared_ptr<LocalIpcNotify> ipcPostWaitNotify;
    std::shared_ptr<RemoteNotify> ipcPreRecordNotify;
    std::shared_ptr<RemoteNotify> ipcPostRecordNotify;
    TransportAttr transportAttr;

    TransportDeviceP2pData() {}
    TransportDeviceP2pData(void *inputBufferPtr,
                           void *outputBufferPtr,
                           std::shared_ptr<LocalIpcNotify> ipcPreWaitNotify,
                           std::shared_ptr<LocalIpcNotify> ipcPostWaitNotify,
                           std::shared_ptr<RemoteNotify> ipcPreRecordNotify,
                           std::shared_ptr<RemoteNotify> ipcPostRecordNotify,
                           TransportAttr &transportAttr)
        : inputBufferPtr(inputBufferPtr),
          outputBufferPtr(outputBufferPtr),
          ipcPreWaitNotify(ipcPreWaitNotify),
          ipcPostWaitNotify(ipcPostWaitNotify),
          ipcPreRecordNotify(ipcPreRecordNotify),
          ipcPostRecordNotify(ipcPostRecordNotify),
          transportAttr(transportAttr)
    {}
};

struct TransportDeviceIbverbsData {
    void *inputBufferPtr;
    void *outputBufferPtr;
    MemDetails localInputMem;
    MemDetails localOutputMem;
    std::shared_ptr<LocalIpcNotify> ackNotify;
    std::shared_ptr<LocalIpcNotify> dataAckNotify;
    std::shared_ptr<LocalIpcNotify> dataNotify;
    uint64_t localNotifyValueAddr;
    uint64_t remoteAckNotifyAddr;
    uint64_t remoteDataNotifyAddr;
    uint64_t remoteDataAckNotifyAddr;
    uint32_t notifyValueKey;
    uint32_t remoteNotifyKey;
    struct HcclQpInfoV2 qpInfo;
    uint32_t remoteInputKey;
    uint32_t remoteOutputKey;
    uint32_t notifySize;
    s64 chipId;

    TransportDeviceIbverbsData() {}
    TransportDeviceIbverbsData(void *inputBufferPtr,
                               void *outputBufferPtr,
                               MemDetails localInputMem,
                               MemDetails localOutputMem,
                               std::shared_ptr<LocalIpcNotify> ackNotify,
                               std::shared_ptr<LocalIpcNotify> dataAckNotify,
                               std::shared_ptr<LocalIpcNotify> dataNotify,
                               uint64_t localNotifyValueAddr,
                               uint64_t remoteAckNotifyAddr,
                               uint64_t remoteDataNotifyAddr,
                               uint64_t remoteDataAckNotifyAddr,
                               uint32_t notifyValueKey,
                               uint32_t remoteNotifyKey,
                               struct HcclQpInfoV2 qpInfo,
                               uint32_t remoteInputKey,
                               uint32_t remoteOutputKey,
                               uint32_t notifySize,
                               s64 chipId)
        : inputBufferPtr(inputBufferPtr),
          outputBufferPtr(outputBufferPtr),
          localInputMem(localInputMem),
          localOutputMem(localOutputMem),
          ackNotify(ackNotify),
          dataAckNotify(dataAckNotify),
          dataNotify(dataNotify),
          localNotifyValueAddr(localNotifyValueAddr),
          remoteAckNotifyAddr(remoteAckNotifyAddr),
          remoteDataNotifyAddr(remoteDataNotifyAddr),
          remoteDataAckNotifyAddr(remoteDataAckNotifyAddr),
          notifyValueKey(notifyValueKey),
          remoteNotifyKey(remoteNotifyKey),
          qpInfo(qpInfo),
          remoteInputKey(remoteInputKey),
          remoteOutputKey(remoteOutputKey),
          notifySize(notifySize),
          chipId(chipId)
    {}

    TransportDeviceIbverbsData(const TransportDeviceIbverbsData &that)
        : inputBufferPtr(that.inputBufferPtr),
          outputBufferPtr(that.outputBufferPtr),
          localInputMem(that.localInputMem),
          localOutputMem(that.localOutputMem),
          ackNotify(that.ackNotify),
          dataAckNotify(that.dataAckNotify),
          dataNotify(that.dataNotify),
          localNotifyValueAddr(that.localNotifyValueAddr),
          remoteAckNotifyAddr(that.remoteAckNotifyAddr),
          remoteDataNotifyAddr(that.remoteDataNotifyAddr),
          remoteDataAckNotifyAddr(that.remoteDataAckNotifyAddr),
          notifyValueKey(that.notifyValueKey),
          remoteNotifyKey(that.remoteNotifyKey),
          qpInfo(that.qpInfo),
          remoteInputKey(that.remoteInputKey),
          remoteOutputKey(that.remoteOutputKey),
          notifySize(that.notifySize),
          chipId(that.chipId)
    {}
};

class Transport {
public:
    Transport() {};
    explicit Transport(TransportBase *pimpl): pimpl_(pimpl) {};
    Transport(TransportType type, TransportPara& para,
              const HcclDispatcher dispatcher,
              const std::unique_ptr<NotifyPool> &notifyPool,
              MachinePara &machinePara,
              const TransportDeviceP2pData &transDevP2pData = TransportDeviceP2pData(),
              const TransportDeviceIbverbsData &transDevIbverbsData = TransportDeviceIbverbsData());

    ~Transport();

    HcclResult Stop();
    HcclResult Resume();
    HcclResult Init();
    HcclResult DeInit();

    HcclResult TxDataSignal(Stream &stream);
    HcclResult RxDataSignal(Stream &stream);

    HcclResult TxAsync(UserMemType dstMemType, u64 dstOffset, const void *src, u64 len, Stream &stream);
    HcclResult TxAsync(std::vector<TxMemoryInfo>& txMems, Stream &stream);

    HcclResult TxWithReduce(UserMemType dstMemType, u64 dstOffset, const void *src, u64 len,
                                    const HcclDataType datatype, HcclReduceOp redOp, Stream &stream);
    HcclResult TxWithReduce(const std::vector<TxMemoryInfo> &txWithReduceMems, const HcclDataType datatype,
        HcclReduceOp redOp, Stream &stream);
    HcclResult RxWithReduce(UserMemType recvSrcMemType, u64 recvSrcOffset, void *recvDst, u64 recvLen,
        void *reduceSrc, void *reduceDst, u64 reduceDataCount, HcclDataType reduceDatatype,
        HcclReduceOp reduceOp, Stream &stream, const u64 reduceAttr);
    HcclResult RxWithReduce(const std::vector<RxWithReduceMemoryInfo> &rxWithReduceMems,
        HcclDataType reduceDatatype, HcclReduceOp reduceOp, Stream &stream,
        const u64 reduceAttr);
    bool IsSupportTransportWithReduce();

    HcclResult RxAsync(UserMemType srcMemType, u64 srcOffset, void *dst, u64 len, Stream &stream);
    HcclResult RxAsync(std::vector<RxMemoryInfo>& rxMems, Stream &stream);
    HcclResult DataReceivedAck(Stream &stream);

    HcclResult TxAck(Stream &stream);
    HcclResult RxAck(Stream &stream);

    HcclResult TxPrepare(Stream &stream);
    HcclResult RxPrepare(Stream &stream);

    HcclResult TxDone(Stream &stream);
    HcclResult RxDone(Stream &stream);

    HcclResult TxData(UserMemType dstMemType, u64 dstOffset, const void *src, u64 len, Stream &stream);
    HcclResult RxData(UserMemType srcMemType, u64 srcOffset, void *dst, u64 len, Stream &stream);

    // 保证send语义完成
    HcclResult TxWaitDone(Stream &stream);
    // 保证recv语义完成
    HcclResult RxWaitDone(Stream &stream);
    // TxWaitDone、RxWaitDone共同出现保证sendrecv语义完成

    HcclResult GetRemoteMem(UserMemType memType, void **remotePtr);
    HcclResult GetRemoteMemKey(UserMemType memType, uint32_t *remoteMemKey);
    HcclResult GetLocalRdmaNotify(std::vector<HcclSignalInfo> &rdmaNotify);
    HcclResult GetRemoteRdmaNotifyAddrKey(std::vector<AddrKey> &rdmaNotifyAddr);
    HcclResult GetLocalNotifyValueAddrKey(std::vector<AddrKey> &notifyValue);
    HcclResult GetLocalMemDetails(UserMemType memType, MemDetails &memDetails);
    HcclResult GetAiQpInfo(HcclQpInfoV2 &aiQpInfo);
    HcclResult GetChipId(s64 &chipId);
    virtual HcclResult GetRemoteMemSize(UserMemType memType, u64 &size);
    HcclResult GetTxAckDevNotifyInfo(HcclSignalInfo &notifyInfo);
    HcclResult GetRxAckDevNotifyInfo(HcclSignalInfo &notifyInfo);
    HcclResult GetTxDataSigleDevNotifyInfo(HcclSignalInfo &notifyInfo);
    HcclResult GetRxDataSigleDevNotifyInfo(HcclSignalInfo &notifyInfo);

    hccl::LinkType GetLinkType() const;
    bool IsSpInlineReduce() const;
    bool GetSupportDataReceivedAck() const;
    void SetSupportDataReceivedAck(bool supportDataReceivedAck);
    u32 GetRemoteRank();

    HcclResult ConnectAsync(u32& status);
    HcclResult ConnectQuerry(u32& status);
    void Break();

    void EnableUseOneDoorbell();

    bool GetUseOneDoorbellValue();

    HcclResult GetTransportAttr(TransportAttr &attr);

    HcclResult TxEnv(const void *ptr, const u64 len, Stream &stream);
    HcclResult RxEnv(Stream &stream);
    bool IsTransportRoce();

    HcclResult Write(
        const void *localAddr, UserMemType remoteMemType, u64 remoteOffset, u64 len, Stream &stream);
    HcclResult Read(
        const void *localAddr, UserMemType remoteMemType, u64 remoteOffset, u64 len, Stream &stream);

    HcclResult PostReady(Stream &stream);
    HcclResult WaitReady(Stream &stream);

    HcclResult PostFin(Stream &stream);
    HcclResult WaitFin(Stream &stream);

    HcclResult PostFinAck(Stream &stream);
    HcclResult WaitFinAck(Stream &stream);

    HcclResult SetStopFlag(bool value);

private:
    void CreateTransportRoce(TransportType type, TransportPara& para, const HcclDispatcher dispatcherPtr,
        const std::unique_ptr<NotifyPool> &notifyPool, MachinePara &machinePara);
    TransportBase *pimpl_;
};

using LINK = std::shared_ptr<Transport>;
}  // namespace hccl

#endif /* TRANSPORT_BASE_H */
