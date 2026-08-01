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
#ifndef OP_API_INC_OBFUSCATION_CALCULATE_H_
#define OP_API_INC_OBFUSCATION_CALCULATE_H_

#include "aclnn/aclnn_base.h"
#include "aclnn_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief aclnnObfuscationCalculate的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_infer
 * 
 * 算子功能： PMCC项目计算实现。
 * @param [in] fd: npu device侧的aclTensor，表示socket连接符，shape为(1)。数据类型支持INT32。数据格式支持ND。
 * @param [in] x: npu device侧的aclTensor，shape为( , *, ... , hiddenSize)，即最后一维的size是hiddenSize， 数据格式支持ND。
 * 昇腾310P AI处理器：Tensor数据类型支持ACL_FLOAT、ACL_FLOAT16、ACL_INT8。
 * 昇腾910B AI处理器：Tensor数据类型支持ACL_FLOAT、ACL_FLOAT16、ACL_INT8、ACL_BF16。
 * @param [in] param: host侧的int32_t，预留的参数字段。
 * @param [in] cmd: host侧的int32_t，混淆算子指令编号。目前仅支持1。
 * @param [out] y: npu device侧的aclTensor，混淆处理后的张量，数据类型及Shape与x相同。数据格式支持ND。
 * @param [out] workspaceSize: 返回用户需要在npu device侧申请的workspace大小。
 * @param [out] executor: 返回op执行器，包含算子计算流程。
 * @return aclnnStatus: 返回状态码。
 */
 ACLNN_API aclnnStatus aclnnObfuscationCalculateGetWorkspaceSize(int32_t fd, const aclTensor* x,
    int32_t param, int32_t cmd, aclTensor* y, uint64_t* workspaceSize, aclOpExecutor** executor);

/**
 * @brief aclnnObfuscationCalculate的第二段接口，用于执行计算
 * @param [in] workspace: 在npu device侧申请的workspace内存地址
 * @param [in] workspaceSize: 在npu device侧申请的workspace大小，由第一段接口aclnnObfuscationCalculateGetWorkspaceSize获取
 * @param [in] executor: 返回op执行器，包含算子计算流程
 * @param [in] stream: acl stream流
 * @return aclnnStatus: 返回状态码
 */
ACLNN_API aclnnStatus aclnnObfuscationCalculate(void* workspace, uint64_t workspaceSize, aclOpExecutor* executor, aclrtStream stream);

#ifdef __cplusplus
}
#endif
#endif  // OP_API_INC_OBFUSCATION_CALCULATE_H_
