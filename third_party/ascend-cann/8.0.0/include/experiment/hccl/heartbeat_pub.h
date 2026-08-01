/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: Heartbeat 封装
 */

#ifndef HCCL_HEARTBEAT_PUB_H
#define HCCL_HEARTBEAT_PUB_H

#include <thread>
#include <map>

#include "hccl/hccl_types.h"
#include "hccl_common.h"


namespace hccl {
// 通信域内rank信息
using HbRankInfo = struct TagHbRankInfo {
public:
    /* * 用户的原始属性 */
    u32 userRank{INVALID_UINT};                                           // 当前rank的user rank ID
    u32 worldRank{INVALID_UINT};                                          // 当前rank的global user rank ID
    s32 devicePhyId{-1};                                                  // 当前rank在操作的设备物理编号
    std::vector<HcclIpAddress> nicIp;                                     // 当前rank所归属网卡的IP值(实际建链所用网卡IP)
    std::vector<HcclIpAddress> backupNicIp;                               // 当前rank所在卡备用网卡的IP值
    NICDeployment nicDeploy{NICDeployment::NIC_DEPLOYMENT_DEVICE};        // 参数平面位置 0:host 1:device other:reserve
    std::string serverId{""};                                           // 当前rank所在服务器的唯一标识值 (rank_table中的server id)
    u32 superDeviceId{INVALID_UINT};                                      // 当前rank所在的超节点内的device id（sdid）
    bool useSuperPodMode{false};                                          // 是否使用超节点模式
    std::string superPodId{""};                                         // 超结点ID 
};

class HeartbeatPub {
public:
    // 集合通信用
    static HcclResult RegisterToHeartBeat(s32 deviceLogicID, u32 userRank, DevType devType,
        std::vector<HbRankInfo> &rankInfoList, const std::string &commIdentifier, bool isUsedRdmaOuter,
        bool retryEnable = false);
    // 点对点通信用,  使用 commIdentifier+tag作为心跳内部索引的key。
    static HcclResult RegisterToHeartBeat(s32 deviceLogicID, u32 userRank, DevType devType,
        std::vector<HbRankInfo> &rankInfoList, u32 peerRankId, const std::string &commIdentifier,
        const std::string &tag, bool isUsedRdmaOuter, bool retryEnable = false);
    static void UnRegisterToHeartBeat(s32 deviceLogicID, DevType devType, const std::string &commIdentifier);
    static void UnRegisterToHeartBeat(s32 deviceLogicID, DevType devType, const std::string &commIdentifier,
        const std::string &tag);
    static void SetRankPortInfo(s32 deviceLogicID, bool isUseRankPort, std::vector<u32> &ranksPort);
    static HcclResult CheckErrorCqe(s32 deviceLogicID, const std::string &identifier, HcclResult &result);
};

} // namespace hccl

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
void RegisterStuckDetectCallBack(HcclResult (*p1)(const s32&, std::pair<int32_t, int32_t>&));
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // HCCL_HEARTBEAT_H