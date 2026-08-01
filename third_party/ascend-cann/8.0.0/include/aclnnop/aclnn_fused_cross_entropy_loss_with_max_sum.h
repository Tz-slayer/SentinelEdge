/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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
/*!
 * \file aclnn_embedding_bag.h
 * \brief
 */
#ifndef OP_API_INC_FUSED_CROSS_ENTROPY_LOSS_WITH_MAX_SUM_H_
#define OP_API_INC_FUSED_CROSS_ENTROPY_LOSS_WITH_MAX_SUM_H_

#include "aclnn/aclnn_base.h"
#include "aclnn_util.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief aclnnFusedCrossEntropyLossWithMaxSum第一段接口，根据具体的计算流程，计算workspace大小。
 * @domain aclnn_ops_infer
 * 算子功能： 本算子是词汇表并行场景下交叉熵计算模块的一部分，解决超大规模词汇表下的显存和计算效率问题，当前部分为计算loss与softMax的结果。
 * 计算公式： lossOut = log(sum_exp_logits) - predicted_logits
 *           softMaxOutOptional = exp(vocab_parallel_logits -logits_max.unsqueeze(dim = -1)) \ sum_exp_logits.unsqueeze(dim = -1)
 * @param [in] logitsMax: npu device侧的aclTensor，数据类型支持float32，必须是1D。
 * @param [in] sumExpLogits: npu device侧的aclTensor，数据类型支持float32。必须是1Dtensor，shape与logitsMax一致。
 * @param [in] predictedLogits: npu device侧的aclTensor，数据类型支持float32，必须是1Dtensor，shape与logitsMax一致。
 * @param [in] labelSmoothing: float类型，平滑系数，目前支支持0。
 * @param [in] inputOptional: npu device侧的aclTensor，数据类型支持float16，bfloat6，必须是1Dtensor。
 * @param [in] weightOptional: npu device侧的aclTensor，数据类型支持float16，bfloat6，必须是1Dtensor。
 * @param [in] vocabParallelLogitsOptional: npu device侧的aclTensor，数据类型支持float32，float16，bfloat6，必须是2Dtensor。
 * @param [out] lossOut: npu device侧的aclTensor，数据类型支持float32，必须是1Dtensor，shape与logitsMax一致。
 * @param [out] softMaxOutOptional: npu device侧的aclTensor，数据类型支持float32，必须是2Dtensor，shape与vocabParallelLogitsOptional一致。
 * @param [out] workspaceSize: 返回用户需要在npu侧申请的workspace大小。
 * @param [out] executor: 返回op执行器。
 * @return aclnnStatus: 返回状态码。
 */
ACLNN_API aclnnStatus aclnnFusedCrossEntropyLossWithMaxSumGetWorkspaceSize(const aclTensor* logitsMax, const aclTensor* sumExpLogits,
                                                        const aclTensor* predictedLogits, float labelSmoothing, const aclTensor* inputOptional,
                                                        const aclTensor* weightOptional, const aclTensor* vocabParallelLogitsOptional, aclTensor* lossOut,
                                                        aclTensor* softMaxOutOptional, uint64_t* workspaceSize, aclOpExecutor** executor);

/**
 * @brief aclnnFusedCrossEntropyLossWithMaxSum的第二段接口，用于执行计算。
 *
 * @param [in] workspace: 在npu device侧申请的workspace内存起址。
 * @param [in] workspaceSize: 在npu device侧申请的workspace大小，由第一段接口aclnnInplaceIndexCopyGetWorkspaceSize获取。
 * @param [in] executor: op执行器，包含了算子计算流程。
 * @param [in] stream: acl stream流。
 * @return aclnnStatus: 返回状态码。
 */
ACLNN_API aclnnStatus aclnnFusedCrossEntropyLossWithMaxSum(void* workspace, uint64_t workspaceSize, aclOpExecutor* executor,
                                        aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif  // OP_API_INC_INDEX_COPY_H_
