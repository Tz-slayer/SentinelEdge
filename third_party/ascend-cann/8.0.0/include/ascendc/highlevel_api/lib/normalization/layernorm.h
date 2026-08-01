/**
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/* !
 * \file layernorm.h
 * \brief
 */
#ifndef LIB_NORMALIZATION_LAYERNORM_H
#define LIB_NORMALIZATION_LAYERNORM_H
#if __CCE_AICORE__ == 200 || __CCE_AICORE__ == 220
#include "kernel_tensor.h"
#include "../../impl/normalization/layernorm/layernorm_common_impl.h"
#include "kernel_tiling/kernel_tiling.h"
namespace AscendC {
#pragma begin_pipe(V)
/*!
 * \brief Applies Layer Normalization over a mini-batch of inputs as described in the paper Layer Normalization.
 * For details about the interface description, see
 * https://pytorch.org/docs/1.10/generated/torch.nn.LayerNorm.html.
 *
 * \note support data type: half and float
 *
 * \param [out] output, output LocalTensor, shape is [B, S, H]
 * \param [out] outputMean, output LocalTensor, shape is [B, S]
 * \param [out] outputVariance, output LocalTensor, shape is [B, S]
 * \param [in] inputX, input LocalTensor, shape is [B, S, H]
 * \param [in] gamma, input LocalTensor, shape is [H]
 * \param [in] beta, input LocalTensor, shape is [H]
 * \param [in] sharedTmpBuffer, input local temporary Tensor
 * \param [in] epsilon, weighting factor
 * \param [in] tiling, layernormtiling
 */
template <typename T, bool isReuseSource = false>
__aicore__ inline void LayerNorm(const LocalTensor<T>& output, const LocalTensor<T>& outputMean,
    const LocalTensor<T>& outputVariance, const LocalTensor<T>& inputX, const LocalTensor<T>& gamma,
    const LocalTensor<T>& beta, const LocalTensor<uint8_t>& sharedTmpBuffer, const T epsilon, LayerNormTiling& tiling)
{
    LayerNormImpl<T, isReuseSource>(output, outputMean, outputVariance, inputX, gamma, beta, sharedTmpBuffer, epsilon,
        tiling);
}

/*!
 * \brief Applies Layer Normalization over a mini-batch of inputs as described in the paper Layer Normalization.
 *
 * \note support data type: half and float
 *
 * \param [out] output, output LocalTensor, shape is [B, S, H]
 * \param [out] outputMean, output LocalTensor, shape is [B, S]
 * \param [out] outputVariance, output LocalTensor, shape is [B, S]
 * \param [in] inputX, input LocalTensor, shape is [B, S, H]
 * \param [in] gamma, input LocalTensor, shape is [H]
 * \param [in] beta, input LocalTensor, shape is [H]
 * \param [in] epsilon, weighting factor
 * \param [in] tiling, layernormtiling
 */
template <typename T, bool isReuseSource = false>
__aicore__ inline void LayerNorm(const LocalTensor<T>& output, const LocalTensor<T>& outputMean,
    const LocalTensor<T>& outputVariance, const LocalTensor<T>& inputX, const LocalTensor<T>& gamma,
    const LocalTensor<T>& beta, const T epsilon, LayerNormTiling& tiling)
{
    LayerNormImpl<T, isReuseSource>(output, outputMean, outputVariance, inputX, gamma, beta, epsilon, tiling);
}

template <typename U, typename T, bool isReuseSource = false, const LayerNormConfig& config = LNCFG_NORM>
__aicore__ inline void LayerNorm(const LocalTensor<T>& output,  const LocalTensor<float>& outputMean,
    const LocalTensor<float>& outputRstd, const LocalTensor<T>& inputX, const LocalTensor<U>& gamma,
    const LocalTensor<U>& beta, const float epsilon, const LayerNormPara& para, const LayerNormSeparateTiling& tiling)
{
    if ASCEND_IS_AIC {
        return;
    }
    LayerNormImpl<U, T, isReuseSource, config>(output, outputMean, outputRstd, inputX, gamma, beta, epsilon, para,
        tiling);
}

template <typename U, typename T, bool isReuseSource = false, const LayerNormConfig& config = LNCFG_NORM>
__aicore__ inline void LayerNorm(const LocalTensor<T>& output,  const LocalTensor<float>& outputMean,
    const LocalTensor<float>& outputRstd, const LocalTensor<T>& inputX, const LocalTensor<U>& gamma,
    const LocalTensor<U>& beta, const float epsilon, const LocalTensor<uint8_t>& sharedTmpBuffer,
    const LayerNormPara& para, const LayerNormSeparateTiling& tiling)
{
    if ASCEND_IS_AIC {
        return;
    }
    LayerNormImpl<U, T, isReuseSource, config>(output, outputMean, outputRstd, inputX, gamma, beta, epsilon,
        sharedTmpBuffer, para, tiling);
}

template <typename T, typename U, bool isReuseSource = false, const WelfordUpdateConfig& config = WFUPDATE_DEFAULT_CFG>
__aicore__ inline void WelfordUpdate(const LocalTensor<U>& outputMean, const LocalTensor<U>& outputVariance,
    const LocalTensor<U>& inputMean, const LocalTensor<U>& inputVariance, const LocalTensor<T>& inputX,
    const WelfordUpdateParam& para)
{
    if ASCEND_IS_AIC {
        return;
    }
    WelfordUpdateImpl<T, U, isReuseSource, config>(outputMean, outputVariance, inputMean, inputVariance, inputX, para);
}

template <typename T, typename U, bool isReuseSource = false, const WelfordUpdateConfig& config = WFUPDATE_DEFAULT_CFG>
__aicore__ inline void WelfordUpdate(const LocalTensor<U>& outputMean, const LocalTensor<U>& outputVariance,
    const LocalTensor<U>& inputMean, const LocalTensor<U>& inputVariance, const LocalTensor<T>& inputX,
    const LocalTensor<uint8_t>& sharedTmpBuffer, const WelfordUpdateParam& para)
{
    if ASCEND_IS_AIC {
        return;
    }
    WelfordUpdateImpl<T, U, isReuseSource, config>(outputMean, outputVariance, inputMean, inputVariance, inputX,
        sharedTmpBuffer, para);
}

#pragma end_pipe
} // namespace AscendC
#endif
#endif // LIB_NORMALIZATION_LAYERNORM_H