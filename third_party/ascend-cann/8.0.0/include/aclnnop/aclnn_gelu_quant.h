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
#ifndef OP_API_INC_LEVEL2_ACLNN_GELU_QUANT_H_
#define OP_API_INC_LEVEL2_ACLNN_GELU_QUANT_H_

#include "aclnn/aclnn_base.h"
#include "aclnn_util.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief aclnnGeluQuant的第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_infer
 * 
 * @param [in] self: 待进行GeluQuant计算的入参。npu device侧的aclTensor，
 * 数据类型支持float16、 bfloat16、float,数据格式支持ND，支持非连续的Tensor。
 * @param [in] inputScaleOptional: npu device侧的aclTensor，数据类型支持float16、 bfloat16、float
 * @param [in] inputOffsetOptional: npu device侧的aclTensor，数据类型支持float16、 bfloat16、float
 * @param [in] approximate:  host侧的aclScalar，数据类型string，仅支持 "none","tanh"
 * @param [in] quantMode:  host侧的aclScalar，数据类型string，仅支持 "static","dynamic"
 * @param [in] roundMode:  host侧的aclScalar，数据类型string，支持 "rint","round","hybrid","floor","ceil","trunc"
 * @param [in] dstType:  host侧的aclScalar, 数据类型int, 输入范围为{2, 34, 35, 36}，分别对应输出y的数据类型为{2: INT8, 34: HIFLOAT8, 35: FLOAT8_E5M2, 36: FLOAT8_E4M3FN}
 * @param [in] y: GeluQuant计算的出参。npu device侧的aclTensor，
 * 数据类型支持int8, hifloat8, float8_e4m3fn, float8_e5m2, 数据格式支持ND，不支持非连续的Tensor。
 * @param [in] outScaleOptional: GeluQuant计算的出参。npu device侧的aclTensor，
 * 数据类型支持float32, 数据格式支持ND，支持非连续的Tensor。
 * @param [out] workspaceSize: 返回用户需要在npu device侧申请的workspace大小。
 * @param [out] executor: 返回op执行器，包含算子计算流程。
 * @return aclnnStatus: 返回状态码。
 */
ACLNN_API aclnnStatus aclnnGeluQuantGetWorkspaceSize(
    const aclTensor* self, const aclTensor* inputScaleOptional, const aclTensor* inputOffsetOptional, const char* approximate, const char* quantMode, const char* roundMode, int64_t dstType, const aclTensor* y, const aclTensor* outScaleOptional,
    uint64_t* workspaceSize, aclOpExecutor** executor);

/**
 * @brief aclnnGeluQuant的第二段接口，用于执行计算。
 * @param [in] workspace: 在npu device侧申请的workspace内存起址。
 * @param [in] workspaceSize: 在npu device侧申请的workspace大小，由第一段接口aclnnGeluQuant获取。
 * @param [in] executor: op执行器，包含了算子计算流程。
 * @param [in] stream: acl stream流。
 * @return aclnnStatus: 返回状态码。
 */
ACLNN_API aclnnStatus aclnnGeluQuant(void* workspace, uint64_t workspaceSize,
                                     aclOpExecutor* executor, aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif  // OP_API_INC_LEVEL2_ACLNN_GELU_QUANT_H_
