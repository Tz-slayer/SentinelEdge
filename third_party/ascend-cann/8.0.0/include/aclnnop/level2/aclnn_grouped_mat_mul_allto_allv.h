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
#ifndef OP_API_INC_GROUPED_MATMUL_ALL_TO_ALLV_
#define OP_API_INC_GROUPED_MATMUL_ALL_TO_ALLV_

#include <string>

#include "aclnn/aclnn_base.h"
#include "aclnn_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 算子功能：实现grouped matmul + alltoallv 融合计算
 * @brief aclnnGroupedMatMulAlltoAllv的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_infer
 * @param [in] gmmX: 计算输入，Tensor，数据类型支持float16，bfloat16。该输入进行AllToAll通信，仅支持二维,
 * 数据格式支持ND，通信后结果作为GroupedMatMul计算的左矩阵
 * @param [in] gmmWeight: 计算输入，Tensor，数据类型支持float16, bfloat16，类型需与x保持一致，仅支持三维,
 * 数据格式支持ND，GroupedMatMul计算的右矩阵
 * @param [in] sendCountsTensorOptional: 可选入参，计算输入，Tensor，数据类型支持int32,
 * int64，类型需与x保持一致，数据格式支持ND
 * @param [in] recvCountsTensorOptional: 可选入参，计算输入，Tensor，数据类型支持int32,
 * int64，类型需与x保持一致，数据格式支持ND
 * @param [in] mmXOptional: 可选入参，计算输入，并行进行的共享专家matmul计算中的左矩阵。*当前暂未支持
 * @param [in] mmWeightOptional: 可选入参，计算输入，并行进行的共享专家matmul计算中的右矩阵。*当前暂未支持
 * @param [in] group: 计算输入，str。ep通信域名称，专家并行的通信域
 * @param [in] epWorldSize: 计算输入，int。ep通信域size
 * @param [in] sendCounts: 计算输入，list int。通信发送的数据量
 * @param [in] recvCounts: 计算输入，list int。通信接受的数据量
 * @param [in] transGmmWeight: 可选入参，计算输入。表明gmm的右矩阵是否需要转置，默认为false
 * @param [in] transMmWeight: 可选入参，计算输入。表明共享专家mm的右矩阵是否需要转置。*当前暂未支持
 * @param [out] y: 计算输出，Tensor，数据类型支持float16, bfloat16。最终计算结果，数据类型与输入gmmX保持一致
 * @param [out] mmYOptional: 可选输出，计算输出，Tensor，数据类型支持float16,
 * bfloat16，共享专家matmul的输出，仅当传入mmX与mmWeight才输出，数据类型与mmX保持一致。*当前暂未支持
 * @param [out] workspaceSize: 出参，返回需要在npu device侧申请的workspace大小。
 * @param [out] executor: 出参，返回op执行器，包含了算子计算流程。
 * @return aclnnStatus: 返回值，返回状态码。
 *
 * 因为集合通信及BatchMatMul计算所需，输入输出shape需满足以下数学关系：（其中ep=epWorldSize，tp=tpWorldSize）
 * gmmX: (BSK, H);
 * gmmWeight: (e, H, N1);
 * groupList: (e);
 * mmX: (BS, H);
 * mmWeight: (H, N2);
 * y: (A, N1);
 * mmY: (BS, N2);
 *
 * 数据关系说明：
 * e表示单卡上的专家数量;
 * A = recvCounts的累加和;
 */
ACLNN_API aclnnStatus aclnnGroupedMatMulAlltoAllvGetWorkspaceSize(
    const aclTensor* gmmX, const aclTensor* gmmWeight, const aclTensor* sendCountsTensorOptional,
    const aclTensor* recvCountsTensorOptional, const aclTensor* mmXOptional, const aclTensor* mmWeightOptional,
    const char* group, int64_t epWorldSize, const aclIntArray* sendCounts, const aclIntArray* recvCounts,
    bool transGmmWeight, bool transMmWeight, aclTensor* y, aclTensor* mmYOptional, uint64_t* workspaceSize,
    aclOpExecutor** executor);

/**
 * @brief aclnnGroupedMatMulAlltoAllv的第二段接口，用于执行计算。
 * @param [in] workspace: 在npu device侧申请的workspace内存起址。
 * @param [in] workspace_size: 在npu
 * device侧申请的workspace大小，由第一段接口aclnnGroupedMatMulAlltoAllvGetWorkspaceSize获取。
 * @param [in] executor: op执行器，包含了算子计算流程。
 * @param [in] stream: acl stream流。
 * @return aclnnStatus: 返回状态码
 */
ACLNN_API aclnnStatus aclnnGroupedMatMulAlltoAllv(void* workspace, uint64_t workspaceSize, aclOpExecutor* executor,
                                                  aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif  // OP_API_INC_GROUPED_MATMUL_ALL_TO_ALLV_