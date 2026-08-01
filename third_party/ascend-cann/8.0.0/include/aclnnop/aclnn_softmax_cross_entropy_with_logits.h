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
#ifndef OP_API_INC_LEVEL2_ACLNN_SOFTMAX_CROSS_ENTROPY_WITH_H_
#define OP_API_INC_LEVEL2_ACLNN_SOFTMAX_CROSS_ENTROPY_WITH_H_

#include "aclnn/aclnn_base.h"
#include "aclnn_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief aclnnSoftmaxCrossEntropyWithLogits的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_infer
 * 功能描述：计算softmax和cross entropy的交叉熵损失，并给出对输入logits的反向梯度。
 * 计算公式：  
 * loss = -Σ (y_i * log(softmax(x_i)))
 * backprop = softmax(x_i) - y_i
 * 其中，$x_i$对应输入的features，$y_i$对应输入的labels。
 * @param [in] features: 输入Tensor，数据类型支持FLOAT，FLOAT16，BFLOAT16。支持非连续Tensor，数据格式支持ND。
 * @param [in] labels: 输入Tensor，数据类型支持FLOAT，FLOAT16，BFLOAT16。支持非连续Tensor，数据格式支持ND。
 * @param [in] loss: 输出Tensor，数据类型支持FLOAT，FLOAT16，BFLOAT16。支持非连续Tensor，数据格式支持ND。
 * @param [in] backprop: 输出Tensor，数据类型支持FLOAT，FLOAT16，BFLOAT16。支持非连续Tensor，数据格式支持ND。
 */
ACLNN_API aclnnStatus aclnnSoftmaxCrossEntropyWithLogitsGetWorkspaceSize(const aclTensor* features, aclTensor* labels, aclTensor* loss, aclTensor* backprop, uint64_t* workspaceSize,
                                                aclOpExecutor** executor);

/**
 * @brief aclnnSoftmaxCrossEntropyWithLogits的第二段接口，用于执行计算。
 * @param [in] workspace: 在npu device侧申请的workspace内存起址。
 * @param [in] workspace_size: 在npu device侧申请的workspace大小，由第一段接口aclnnGeluGetWorkspaceSize获取。
 * @param [in] executor: op执行器，包含了算子计算流程。
 * @param [in] stream: acl stream流。
 * @return aclnnStatus: 返回状态码。
 */
ACLNN_API aclnnStatus aclnnSoftmaxCrossEntropyWithLogits(void* workspace, uint64_t workspaceSize, aclOpExecutor* executor,
                                const aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif  // OP_API_INC_LEVEL2_ACLNN_SOFTMAX_CROSS_ENTROPY_WITH_H_