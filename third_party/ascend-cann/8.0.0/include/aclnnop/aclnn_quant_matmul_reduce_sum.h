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
#ifndef OP_API_ACLNN_QUANT_MATMUL_REDUCE_SUM_H_
#define OP_API_ACLNN_QUANT_MATMUL_REDUCE_SUM_H_

#include "aclnn/aclnn_base.h"
#include "aclnn_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief aclnnQuantMatmulReduceSumWeightNz的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_infer
 * 
 * 算子功能：完成量化的分组矩阵计算，然后所有组的矩阵计算结果reduceSum后输出。
 *
 * @param [in] x1: 左矩阵，数据类型支持：int8。
 * @param [in] x2: 右矩阵，数据类型支持：int8。
 * @param [in] x1Scale: [pertoken_scale] 矩阵计算的反量化参数，数据类型支持：float32。
 * @param [in] x2Scale: [scale] 量化参数中的缩放因子，数据类型支持：bfloat16。
 * @param [in] yScale: 预留参数
 * @param [in] x1Offset: 预留参数
 * @param [in] x2Offset: 预留参数
 * @param [in] yOffset: 预留参数
 * @param [in] bias: 预留参数
 *
 * @param [in] transposeX1: 预留参数
 * @param [in] transposeX2: 预留参数
 * @param [in] groupSize: 预留参数
 * @param [in] dims: 一维数组，代表累加求和的维度，目前仅支持[0]
 * @param [in] keepDims: 预留参数
 *
 * @param [out] out: 计算结果，数据类型：bfloat16。
 * @param [out] workspaceSize: 返回需要在npu device侧申请的workspace大小。
 * @param [out] executor: 返回op执行器，包含了算子计算流程。
 * @return aclnnStatus: 返回状态码
 */
ACLNN_API aclnnStatus aclnnQuantMatmulReduceSumWeightNzGetWorkspaceSize(
    const aclTensor* x1, const aclTensor* x2, const aclTensor* x1Scale, const aclTensor* x2Scale,
    const aclTensor* yScale, const aclTensor* x1Offset, const aclTensor* x2Offset, const aclTensor* yOffset,
    const aclTensor* bias, bool transposeX1, bool transposeX2,
    int64_t groupSize, const aclIntArray* dims, bool keepDims,    
    aclTensor* out, uint64_t* workspaceSize, aclOpExecutor** executor);

/**
 * @brief aclnnQuantMatmulReduceSumWeightNz的第二段接口，用于执行计算。
 *
 * @param [in] workspace: 在npu device侧申请的workspace内存起址。
 * @param [in] workspace_size: 在npu device侧申请的workspace大小，由aclnnQuantMatmulReduceSumWeightNzGetWorkspaceSize获取。
 * @param [in] exector: op执行器，包含了算子计算流程。
 * @param [in] stream: acl stream流。
 * @return aclnnStatus: 返回状态码
 */
ACLNN_API aclnnStatus aclnnQuantMatmulReduceSumWeightNz(
    void* workspace, uint64_t workspaceSize, aclOpExecutor* executor, aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif  // OP_API_ACLNN_QUANT_MATMUL_REDUCE_SUM_H_
