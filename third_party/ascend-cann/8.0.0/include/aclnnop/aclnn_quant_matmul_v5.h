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
#ifndef OP_API_INC_QUANT_MATMUL_V5_
#define OP_API_INC_QUANT_MATMUL_V5_

#include <string>

#include "aclnn/aclnn_base.h"
#include "aclnn_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief aclnnQuantMatmulV5的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_infer
 * 算子功能：实现量化场景的矩阵乘。
 * @param [in] x1: matmul左矩阵，数据类型支持：float4_e2m1, float4_e1m2, int8, float8_e4m3fn, float8_e5m2, hifloat8。
 * @param [in] x2: matmul右矩阵，数据类型支持：float4_e2m1, float4_e1m2, int8, float8_e4m3fn, float8_e5m2, hifloat8。
 * @param [in] x1Scale: x1量化参数，数据类型支持：float8_e8m0, float32。
 * @param [in] x2Scale: x2量化参数，数据类型支持：float8_e8m0, bfloat16, float32, int64, uint64。
 * @param [in] yScale: y量化参数，数据类型支持：int64、uint64。
 * @param [in] x1Offset: 预留参数，当前接口不支持该参数。
 * @param [in] x2Offset: 量化参数，数据类型支持：float32。
 * @param [in] yOffset: 预留参数，当前接口不支持该参数。
 * @param [in] bias: 偏置，数据类型支持：int32, bfloat16, float16, float32。
 * @param [in] transposeX1: x1矩阵是否转置。
 * @param [in] transposeX2: x2矩阵是否转置。
 * @param [in] groupSize: 量化参数，数据类型支持：int64。
 * @param [out] out: 计算结果，数据类型：int8，float16, bfloat16, float32, float8_e4m3fn, hifloat8。
 * @param [out] workspaceSize: 返回需要在npu device侧申请的workspace大小。
 * @param [out] executor: 返回op执行器，包含了算子计算流程。
 * @return aclnnStatus: 返回状态码
 */
ACLNN_API aclnnStatus aclnnQuantMatmulV5GetWorkspaceSize(const aclTensor *x1, const aclTensor *x2,
                                                         const aclTensor *x1Scale, const aclTensor *x2Scale,
                                                         const aclTensor *yScale, const aclTensor *x1Offset,
                                                         const aclTensor *x2Offset, const aclTensor *yOffset,
                                                         const aclTensor *bias, bool transposeX1, bool transposeX2,
                                                         int64_t groupSize, aclTensor *out, uint64_t *workspaceSize,
                                                         aclOpExecutor **executor);

/**
 * @brief aclnnQuantMatmulV5的第二段接口，用于执行计算。
 * @param [in] workspace: 在npu device侧申请的workspace内存起址。
 * @param [in] workspace_size: 在npu device侧申请的workspace大小，由第一段接口aclnnQuantMatmulV5GetWorkspaceSize获取。
 * @param [in] exector: op执行器，包含了算子计算流程。
 * @param [in] stream: acl stream流。
 * @return aclnnStatus: 返回状态码
 */
ACLNN_API aclnnStatus aclnnQuantMatmulV5(void* workspace, uint64_t workspaceSize, aclOpExecutor* executor,
                                         aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif  // OP_API_INC_QUANT_MATMUL_V5_
