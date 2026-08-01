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

 #ifndef OP_API_INC_NPU_FORMAT_CAST_H_
 #define OP_API_INC_NPU_FORMAT_CAST_H_
 
 #include "aclnn/aclnn_base.h"
 #include "aclnn_util.h"
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /**
  * 算子功能：实现ND到指定C0大小的FRACTAL_NZ格式的转换，C0为FRACTAL_NZ格式最后一维的大小
  * @brief aclnnNpuFormatCast的第一段接口，用于获取计算所需worksapce大小以及包含了算子计算流程的执行器。
  * @domain aclnn_ops_infer
  * @param [in] srcTensor: 输入的数据，数据格式支持ND。不支持非连续的Tensor,支持的数据类型：INT8、INT32、FLOAT16、BFLOAT16、FLOAT。
  * @param [in] dstTensor：转换后的目标数据，数据格式支持ACL_FORMAT_FRACTAL_NZ(29)、ACL_FORMAT_FRACTAL_NZ_C0_16(50)、ACL_FORMAT_FRACTAL_NZ_C0_32(51)。数据类型支持INT8、INT32、FLOAT16、BFLOAT16、FLOAT。
  * @param [in] workspaceSize：需要在Device侧申请的worksapce的大小。
  * @param [in] executor：包含算子计算流程的op执行器。
  * @return aclnnStatus: 返回状态码
  */
 ACLNN_API aclnnStatus aclnnNpuFormatCastGetWorkspaceSize(const aclTensor* srcTensor, aclTensor* dstTensor,
                                                          uint64_t* workspaceSize, aclOpExecutor** executor);
 
 /**
  * @brief aclnnNpuFormatCast的第二段接口，用于执行计算。
  * @param [in] workspace: 在npu device侧申请的workspace内存起址。
  * @param [in] workspace_size: 在npu device侧申请的workspace大小，由第一段接口aclnnNpuFormatCastGetWorkspaceSize获取。
  * @param [in] exector: op执行器，包含了算子计算流程。
  * @param [in] stream: 指定执行任务的AscendCL Stream流。
  * @return aclnnStatus: 返回状态码
  */
 
 ACLNN_API aclnnStatus aclnnNpuFormatCast(void* workspace, uint64_t workspaceSize, aclOpExecutor* executor,
                                          aclrtStream stream);
 
 /**
  * @brief 用于计算转换目标数据的shape和数据格式
  * @param [in] srcTensor: 输入的数据，数据格式支持ND。支持的数据类型：INT8、INT32、FLOAT16、BFLOAT16、FLOAT。
  * @param [in] dstFormat: 转换的目标数据格式。支持的数据格式：ACL_FORMAT_FRACTAL_NZ(29)。
  * @param [in] additionalDtype: 转换成NZ格式推断C0大小时，使用的数据类型的枚举值。支持的数据类型：ACL_FLOAT16(1)、ACL_BF16(27)、INT8(2)、ACL_FLOAT(0)。
  * @param [in] dstShape: 指向目标数组Shape数组的指针，在非报错场景下指向的内存由调用者释放。
  * @param [in] dstShapeSize: 指向目标数据Shape数组的大小的指针。
  * @param [in] actualFormat: 指向目标数据数据格式的指针。仅支持ACL_FORMAT_FRACTAL_NZ(29)、ACL_FORMAT_FRACTAL_NZ_C0_16(50)、ACL_FORMAT_FRACTAL_NZ_C0_32(51)。
  * @return aclnnStatus: 返回状态码
  */
 
 ACLNN_API aclnnStatus aclnnNpuFormatCastCalculateSizeAndFormat(const aclTensor* srcTensor, const int dstFormat,
                                                                const int additionalDtype, int64_t** dstShape,
                                                                uint64_t* dstShapeSize, int* actualFormat);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif  // OP_API_INC_NPU_FORMAT_CAST_H_