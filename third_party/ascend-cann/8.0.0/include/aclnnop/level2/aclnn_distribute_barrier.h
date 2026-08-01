/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
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
#ifndef OP_API_INC_DISTRIBUTE_BARRIER_H_
#define OP_API_INC_DISTRIBUTE_BARRIER_H_

#include <string>

#include "aclnn/aclnn_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 算子功能：实现全卡同步
 * @brief
 * aclnnDistributeBarrier的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_infer
 * @param [in] xRef: 计算输入，占位未使用，Tensor，支持bfloat16, float16,
 * float32, bool, int8, int16, int32, int64, uint8, uint16, uint32, uint64。
 * @param [in] group: 计算输入，str。通信域名称，专家并行的通信域。
 * @param [in] worldSize: 计算输入，int64_t。通信域size。
 * @param [out] workspaceSize: 出参，返回需要在npu device侧申请的workspace大小。
 * @param [out] executor: 出参，返回op执行器，包含了算子计算流程。
 * @return aclnnStatus: 返回值，返回状态码。
 *
 */
__attribute__((visibility("default"))) aclnnStatus
aclnnDistributeBarrierGetWorkspaceSize(aclTensor* xRef, const char* group,
                                       int64_t worldSize,
                                       uint64_t* workspaceSize,
                                       aclOpExecutor** executor);

/**
 * @brief aclnnDistributeBarrier的第二段接口，用于执行计算。
 * @param [in] workspace: 在npu device侧申请的workspace内存起址。
 * @param [in] workspace_size: 在npu
 * device侧申请的workspace大小，由第一段接口aclnnDistributeBarrierGetWorkspaceSize获取。
 * @param [in] executor: op执行器，包含了算子计算流程。
 * @param [in] stream: acl stream流。
 * @return aclnnStatus: 返回状态码
 */
__attribute__((visibility("default"))) aclnnStatus aclnnDistributeBarrier(
    void* workspace, uint64_t workspaceSize, aclOpExecutor* executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif  // OP_API_INC_DISTRIBUTE_BARRIER_H_