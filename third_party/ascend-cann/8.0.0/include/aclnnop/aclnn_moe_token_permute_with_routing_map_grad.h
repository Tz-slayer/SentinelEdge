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
#ifndef OP_API_INC_MOE_TOKEN_PERMUTE_WITH_ROUTING_MAP_GRAD_H_
#define OP_API_INC_MOE_TOKEN_PERMUTE_WITH_ROUTING_MAP_GRAD_H_

#include "aclnn/aclnn_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief aclnnMoeTokenPermuteWithRoutingMapGrad的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_train
 * @param [in] permutedTokenOutputGrad: 计算输入，Tensor，数据类型float16、bfloat16、float。仅支持2维，数据格式支持ND。输入的token梯度数据。
 * @param [in] permutedProbsOutputGradOptional: 计算可选输入，Tensor，数据类型float16、bfloat16、float，必须为2维，数据格式支持ND。输入的prob梯度数据。
 * @param [in] sortedIndices: 计算输入，Tensor，数据类型int32，仅支持1维，数据格式支持ND。
 * @param [in] routingMapOptional:  计算可选输入，Tensor，数据类型int8、bool，数据格式支持ND。代表token到expert的映射关系。
 * @param [in] expertsNum: 计算输入，int。表示参与运算的专家数量。
 * @param [in] tokensNum: 计算输入，int。表示参与运算的token数量。
 * @param [in] dropAndPad: 计算输入，bool。 表示是否开启dropAndPad模式。
 * @param [out] tokensGradOut: 计算输出，Tensor，必选输出，数据类型支持float16、bfloat16、float，仅支持2维，数据格式支持ND。根据routingMap处理后的token特征。
 * @param [out] probsGradOutOptional: 计算输出，Tensor，可选输出，数据类型float16、bfloat16、float，仅支持1维，数据格式支持ND,根据routingMap处理后的prob梯度。
 * @param [out] workspaceSize: 出参，返回需要在npu device侧申请的workspace大小。
 * @param [out] executor: 出参，返回op执行器，包含了算子计算流程。
 * @return aclnnStatus: 返回值，返回状态码
 *
 */
__attribute__((visibility("default"))) aclnnStatus aclnnMoeTokenPermuteWithRoutingMapGradGetWorkspaceSize(
    const aclTensor* permutedTokenOutputGrad, const aclTensor* permutedProbsOutputGradOptional,
    const aclTensor* sortedIndices, const aclTensor* routingMapOptional, int64_t expertsNum, int64_t tokensNum,
    bool dropAndPad, aclTensor* tokensGradOut, aclTensor* probsGradOutOptional, uint64_t* workspaceSize,
    aclOpExecutor** executor);

/**
 * @brief aclnnMoeTokenPermuteWithRoutingMapGrad的第二段接口，用于执行计算。
 * @param [in] workspace: 在npu device侧申请的workspace内存起址。
 * @param [in] workspaceSize: 在npu device侧申请的workspace大小，由第一段接口aclnnMoeTokenPermuteWithRoutingMapGradGetWorkspaceSize获取。
 * @param [in] exector: op执行器，包含了算子计算流程。
 * @param [in] stream: acl stream流。
 * @return aclnnStatus: 返回状态码
 */
__attribute__((visibility("default"))) aclnnStatus aclnnMoeTokenPermuteWithRoutingMapGrad(void* workspace,
                                                                                          uint64_t workspaceSize,
                                                                                          aclOpExecutor* executor,
                                                                                          aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif