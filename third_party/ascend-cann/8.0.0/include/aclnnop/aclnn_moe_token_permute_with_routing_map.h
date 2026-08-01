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
#ifndef OP_API_INC_MOE_TOKEN_PERMUTE_WITH_ROUTING_MAP_H_
#define OP_API_INC_MOE_TOKEN_PERMUTE_WITH_ROUTING_MAP_H_

#include "aclnn/aclnn_base.h"
#include "aclnn_util.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief aclnnMoeTokenPermuteWithRoutingMap的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_train
 * @param [in] tokens: 计算输入，Tensor，数据类型float16，bfloat16,float。输入的token数据。
 * @param [in] routingMap: 计算输入，Tensor，数据类型int8,bool，数据格式支持ND。代表token到expert的映射关系。
 * @param [in] probsOptional: 计算可选输入，Tensor，数据类型float16，bfloat16,float，必须为2维，数据格式支持ND。输入的prob数据。
 * @param [in] numOutTokens: 计算可选输入，int。用于计算有效输出token数。
 * @param [in] dropAndPad: 计算可选输入，bool。 表示是否开启dropAndPad模式。
 * @param [out] permutedTokensOut: 计算输出，Tensor，必选输出，数据类型支持float16, bfloat16,float，仅支持2维，数据格式支持ND。根据routingMap处理后的token特征。
 * @param [out] sortedIndicesOut: 计算输出，Tensor，必选输出，数据类型int32，仅支持1维，数据格式支持ND。quantMode为0时输出为空。
 * @param [out] permuteProbsOutOptional: 计算输出，Tensor，可选输出，数据类型float16, bfloat16,float，仅支持1维，数据格式支持ND,根据routingMap处理后的prob。
 * @param [out] workspaceSize: 出参，返回需要在npu device侧申请的workspace大小。
 * @param [out] executor: 出参，返回op执行器，包含了算子计算流程。
 * @return aclnnStatus: 返回值，返回状态码
 *
 */
ACLNN_API aclnnStatus aclnnMoeTokenPermuteWithRoutingMapGetWorkspaceSize(const aclTensor* tokens,
                                                                         const aclTensor* routingMap,
                                                                         const aclTensor* probsOptional,
                                                                         int64_t numOutTokens,
                                                                         bool dropAndPad,
                                                                         aclTensor* permuteTokensOut,
                                                                         aclTensor* permuteProbsOutOptional,
                                                                         aclTensor* sortedIndicesOut,
                                                                         uint64_t* workspaceSize,
                                                                         aclOpExecutor** executor);
/**
 * @brief aclnnMoeTokenPermuteWithRoutingMap的第二段接口，用于执行计算。
 * @param [in] workspace: 在npu device侧申请的workspace内存起址。
 * @param [in] workspace_size: 在npu device侧申请的workspace大小，由第一段接口aclnnMoeTokenPermuteWithRoutingMapGetWorkspaceSize获取。
 * @param [in] exector: op执行器，包含了算子计算流程。
 * @param [in] stream: acl stream流。
 * @return aclnnStatus: 返回状态码
 */
ACLNN_API aclnnStatus aclnnMoeTokenPermuteWithRoutingMap(void* workspace, uint64_t workspaceSize,
                                                         aclOpExecutor *executor, aclrtStream stream);


#ifdef __cplusplus
}
#endif

#endif
