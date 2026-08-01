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

#ifndef OP_API_INC_FUSED_LINEAR_CROSS_ENTROPY_LOSS_GRAD_H_
#define OP_API_INC_FUSED_LINEAR_CROSS_ENTROPY_LOSS_GRAD_H_

#include "aclnn/aclnn_base.h"
#include "aclnn_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief aclnnFusedLinearCrossEntropyLossGrad的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_train
 * @param [in] grad: npu device侧的aclTensor，数据类型支持FLOAT，支持非连续的Tensor，数据格式支持ND。
 * @param [in] input: npu device侧的aclTensor，数据类型支持FLOAT16、BFLOAT16，支持非连续的Tensor，数据格式支持ND。
 * @param [in] weight: npu device侧的aclTensor，数据类型支持FLOAT16、BFLOAT16，支持非连续的Tensor，数据格式支持ND。
 * @param [in] targetMask: npu device侧的aclTensor，数据类型支持BOOL、UINT8，支持非连续的Tensor，数据格式支持ND。
 * @param [in] maskedTarget: npu device侧的aclTensor，数据类型支持INT64、INT32，支持非连续的Tensor，数据格式支持ND。
 * @param [in] labelSmoothing: host侧的float类型，标签平滑系数。
 * @param [in] logitsMaxOptional: npu device侧的aclTensor，数据类型支持FLOAT，支持非连续的Tensor，数据格式支持ND。
 * @param [in] sumExpLogitsOptional: npu device侧的aclTensor，数据类型支持FLOAT，支持非连续的Tensor，数据格式支持ND。
 * @param [in] softmaxOptional: npu device侧的aclTensor，数据类型支持FLOAT，支持非连续的Tensor，数据格式支持ND。
 * @param [in] inputGradOut: npu device侧的aclTensor，数据类型支持FLOAT16、BFLOAT16，支持非连续的Tensor，数据格式支持ND。
 * @param [in] weightGradOut: npu device侧的aclTensor，数据类型支持FLOAT16、BFLOAT16，支持非连续的Tensor，数据格式支持ND。
 * @param [out] workspaceSize: 返回用户需要在npu device侧申请的workspace大小。
 * @param [out] executor: 返回op执行器，包含算子计算流程。
 * @return aclnnStatus: 返回状态码。
 */
ACLNN_API aclnnStatus aclnnFusedLinearCrossEntropyLossGradGetWorkspaceSize(
    const aclTensor *grad, const aclTensor *input, const aclTensor *weight, const aclTensor *targetMask, const aclTensor *maskedTarget,
    float labelSmoothing, const aclTensor *logitsMaxOptional, const aclTensor *sumExpLogitsOptional, const aclTensor *softmaxOptional,
    aclTensor *inputGradOut, aclTensor *weightGradOut, uint64_t *workspaceSize, aclOpExecutor **executor);

/**
 * @brief aclnnFusedLinearCrossEntropyLossGrad的第二段接口，用于执行计算。
 * @param [in] workspace: 在npu device侧申请的workspace内存起址。
 * @param [in] workspaceSize: 在npu device侧申请的workspace大小，由第一段接口aaclnnFusedLinearCrossEntropyLossGradGetWorkspaceSize获取。
 * @param [in] stream: acl stream流。
 * @param [in] executor: op执行器，包含了算子计算流程。
 * @return aclnnStatus: 返回状态码。
 */
ACLNN_API aclnnStatus aclnnFusedLinearCrossEntropyLossGrad(
    void* workspace, uint64_t workspaceSize, aclOpExecutor* executor, aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif  // OP_API_INC_FUSED_LINEAR_CROSS_ENTROPY_LOSS_GRAD_H_
