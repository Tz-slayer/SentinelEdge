/**
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/*!
 * \file noramlize.h
 * \brief Given mean and variance, calculate rstd and output.
 */
#ifndef LIB_NORMALIZATION_NORMALIZE_H
#define LIB_NORMALIZATION_NORMALIZE_H
#if __CCE_AICORE__ == 200 || __CCE_AICORE__ == 220
#include "kernel_tensor.h"
#include "../../impl/normalization/normalize/normalize_common_impl.h"

namespace AscendC {
#pragma begin_pipe(V)

template <typename U, typename T, bool isReuseSource = false, const NormalizeConfig& config = NLCFG_NORM>
__aicore__ inline void Normalize(const LocalTensor<T>& output, const LocalTensor<float>& outputRstd,
    const LocalTensor<float>& inputMean, const LocalTensor<float>& inputVariance, const LocalTensor<T>& inputX,
    const LocalTensor<U>& gamma, const LocalTensor<U>& beta, const LocalTensor<uint8_t>& sharedTmpBuffer,
    const float epsilon, const NormalizePara& para)
{
    if ASCEND_IS_AIC {
        return;
    }
    NormalizeImpl<U, T, isReuseSource, config>(output, outputRstd, inputMean, inputVariance, inputX, gamma, beta,
        sharedTmpBuffer, epsilon, para);
}

template <typename U, typename T, bool isReuseSource = false, const NormalizeConfig& config = NLCFG_NORM>
__aicore__ inline void Normalize(const LocalTensor<T>& output, const LocalTensor<float>& outputRstd,
    const LocalTensor<float>& inputMean, const LocalTensor<float>& inputVariance, const LocalTensor<T>& inputX,
    const LocalTensor<U>& gamma, const LocalTensor<U>& beta, const float epsilon, const NormalizePara& para)
{
    if ASCEND_IS_AIC {
        return;
    }
    NormalizeImpl<U, T, isReuseSource, config>(output, outputRstd, inputMean, inputVariance, inputX, gamma, beta,
        epsilon, para);
}
#pragma end_pipe
} // namespace AscendC
#endif
#endif // LIB_NORMALIZATION_NORMALIZE_H
