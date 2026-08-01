/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/*!
 * \file aclnn_norm_rope_concat.h
 * \brief
 */
#ifndef ACLNN_NORM_ROPE_CONCAT_H_
#define ACLNN_NORM_ROPE_CONCAT_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnNormRopeConcatGetWorkspaceSize
 * parameters :
 * executor : executor context(output).
 */
__attribute__((visibility("default"))) aclnnStatus aclnnNormRopeConcatGetWorkspaceSize(
    const aclTensor *query, const aclTensor *key, const aclTensor *value, const aclTensor *encoderQuery,
    const aclTensor *encoderKey, const aclTensor *encoderValue, const aclTensor *normQueryWeight,
    const aclTensor *normQueryBias, const aclTensor *normKeyWeight, const aclTensor *normKeyBias,
    const aclTensor *normAddedQueryWeight, const aclTensor *normAddedQueryBias, const aclTensor *normAddedKeyWeight,
    const aclTensor *normAddedKeyBias, const aclTensor *ropeSin, const aclTensor *ropeCos, int64_t normType,
    int64_t normAddedType, int64_t ropeType, int64_t concatOrder, double eps, bool isTraining,
    const aclTensor *queryOutput, const aclTensor *keyOutput, const aclTensor *valueOutput,
    const aclTensor *normQueryMean, const aclTensor *normQueryRstd, const aclTensor *normKeyMean,
    const aclTensor *normKeyRstd, const aclTensor *normAddedQueryMean, const aclTensor *normAddedQueryRstd,
    const aclTensor *normAddedKeyMean, const aclTensor *normAddedKeyRstd, uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnNormRopeConcat
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default"))) aclnnStatus aclnnNormRopeConcat(void *workspace, uint64_t workspaceSize,
                                                                       aclOpExecutor *executor, aclrtStream stream);
#ifdef __cplusplus
}
#endif

#endif
