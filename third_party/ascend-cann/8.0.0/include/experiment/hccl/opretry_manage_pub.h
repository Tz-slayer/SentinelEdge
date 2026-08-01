/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: OpRetryManager 封装
 */

#ifndef HCCL_OPRETRY_MANAGER_PUB_H
#define HCCL_OPRETRY_MANAGER_PUB_H

#include <map>
#include "hccl/hccl_types.h"
#include "hdc_pub.h"
#include "hccl_op_retry_pub.h"
#include "notify_pool.h"
#include "hccl_socket.h"

namespace hccl {
class OpRetryManager;
class OpRetryManagerPub {
public:
    OpRetryManagerPub();
    ~OpRetryManagerPub();

    HcclResult RegisterOpRetryMachine(const std::string &group, u32 rankId, u32 rankSize, const HcclIpAddress &serverIp,
        s32 serverDevId, const HcclIpAddress &localIp, bool isRoot, std::shared_ptr<HcclSocket> agentConnection,
        std::map<u32, std::shared_ptr<HcclSocket> > &serverConnections, std::shared_ptr<HDCommunicate> h2dPtr,
        std::shared_ptr<HDCommunicate> d2hPtr, std::shared_ptr<HcclOpStreamRes> opStreamPtr,
        OpRetryResetNotifyCallback notifyResetCallback, OpRetrySetTransprotStatusCallback setTransprotStatusCallback,
        s32 deviceLogicId, bool isEnableBackupLink);
    HcclResult UnRegisterOpRetryManager(const std::string& group);

    static HcclResult AddLinkInfoByIdentifier(s32 deviceLogicID, const std::string &identifier, 
        const std::string &newTag, std::vector<u32> &remoteRankList, bool incre = false);
    static HcclResult GetLinkInfoByIdentifier(s32 deviceLogicID, const std::string &identifier, 
        const std::string &newTag, std::vector<u32> &remoteRankList);
    static HcclResult DeleteLinkInfoByIdentifier(s32 deviceLogicID, const std::string &identifier);
private:
    std::unique_ptr<OpRetryManager> pimpl_;
};

} // namespace hccl

#endif
