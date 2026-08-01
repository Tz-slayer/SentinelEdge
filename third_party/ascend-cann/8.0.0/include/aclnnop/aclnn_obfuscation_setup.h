/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef OP_API_INC_OBFUSCATION_SETUP_H_
#define OP_API_INC_OBFUSCATION_SETUP_H_

#include "aclnn/aclnn_base.h"
#include "aclnn_util.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief aclnnObfuscationSetup的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_infer
 * 
 * 算子功能： PMCC项目初始化。
 * @param [in] fdToClose: host侧的int32_t，待关闭的socket连接符，cmd为3时填写本算子在cmd为1时返回的fd，否则填0。
 * @param [in] dataType: host侧的int32_t，代表Tensor数据类型的编号，仅在cmd设置为1或2时需要填写有效值，否则填0。
 * 昇腾310P AI处理器：在{0, 1}中选择，0表示ACL_FLOAT、1表示ACL_FLOAT16
 * 昇腾910B AI处理器：在{0, 1, 2, 27}中选择，0表示ACL_FLOAT、1表示ACL_FLOAT16、2表示ACL_INT8、27表示ACL_BF16
 * @param [in] hiddenSize: host侧的int32_t，隐藏层维度，仅在cmd设置为1或2时需要填写有效值，否则填0。
 * @param [in] tpRank: host侧的int32_t，TP Rank，仅在cmd设置为1或2时需要填写有效值，否则填0。
 * @param [in] modelObfSeedId: host侧的int32_t，模型混淆因子id，用于TA从TEE KMC查询模型混淆因子，仅在cmd设置为1或2时需要填写有效值，否则填0。
 * @param [in] dataObfSeedId: host侧的int32_t，数据混淆因子id，用于TA从TEE KMC查询数据混淆因子，仅在cmd设置为1或2时需要填写有效值，否则填0。
 * @param [in] cmd: host侧的int32_t，setup指令编号，在{1, 2, 3}中选择，设置为1时进行普通模式资源初始化、为2时进行高精度模式资源初始化，设置为3时进行资源释放。
 * @param [in] threadNum: host侧的int32_t，CA/TA进行混淆处理使用的线程数。在{1, 2, 3, 4, 5, 6}中选择，仅在cmd设置为1或2时需要填写有效值，否则填0。
 * @param [out] fd: npu device侧的aclTensor，表示socket连接符，在{1, 2, 16}中选择，设置为1时进行普通模式资源初始化、为2时进行高精度模式资源初始化，设置为16时进行资源释放。
 * shape为(1)。数据格式支持ND。
 * @param [out] workspaceSize: 返回用户需要在npu device侧申请的workspace大小。
 * @param [out] executor: 返回op执行器，包含算子计算流程。
 * @return aclnnStatus: 返回状态码。
 */
 ACLNN_API aclnnStatus aclnnObfuscationSetupGetWorkspaceSize(int32_t fdToClose, int32_t dataType, 
                                                             int32_t hiddenSize, int32_t tpRank, 
                                                             int32_t modelObfSeedId, int32_t dataObfSeedId, 
                                                             int32_t cmd, int32_t threadNum, 
                                                             aclTensor* fd, uint64_t* workspaceSize,
                                                             aclOpExecutor** executor);

/**
 * @brief aclnnObfuscationSetup的第二段接口，用于执行计算
 * @param [in] workspace: 在npu device侧申请的workspace内存地址
 * @param [in] workspaceSize: 在npu device侧申请的workspace大小，由第一段接口aclnnObfuscationSetupGetWorkspaceSize获取
 * @param [in] executor: 返回op执行器，包含算子计算流程
 * @param [in] stream: acl stream流
 * @return aclnnStatus: 返回状态码
 */
ACLNN_API aclnnStatus aclnnObfuscationSetup(void* workspace, uint64_t workspaceSize, aclOpExecutor* executor, aclrtStream stream);

#ifdef __cplusplus
}
#endif
#endif  // OP_API_INC_OBFUSCATION_SETUP_H_
