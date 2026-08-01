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
 * \file cumsum.h
 * \brief
 */
#ifndef LIB_MATH_CUMSUM_H
#define LIB_MATH_CUMSUM_H

#include "kernel_tensor.h"
#include "kernel_operator_intf.h"
#include "../../impl/math/cumsum/cumsum_common_impl.h"
#if ASCENDC_CPU_DEBUG
#include "kernel_log.h"
#endif
#if __CCE_AICORE__ >= 200

namespace AscendC {
#pragma begin_pipe(V)

constexpr CumSumConfig defaultCumSumConfig = {true, false, true};

/* !
 * \brief This function calculates the average based on the orientation of the last axis or fist axis.
 * For details about the interface description, see
 * https://pytorch.org/docs/stable/generated/torch.cumsum.html
 *
 * \note support data type: half and float
 *
 * \param [out] dstTensor, output LocalTensor
 * \param [out] lastRowTensor, the last row of the output LocalTensor
 * \param [in] srcTensor, input LocalTensor
 * \param [in] sharedTmpBuffer, input local temporary Tensor
 * \param [in] cumSumInfo, shape information of srcTensor
 */

template <typename T, const CumSumConfig &config = defaultCumSumConfig>
__aicore__ inline void CumSum(LocalTensor<T> &dstTensor, LocalTensor<T> &lastRowTensor, const LocalTensor<T> &srcTensor,
    LocalTensor<uint8_t> &sharedTmpBuffer, const CumSumInfo &cumSumInfo)
{
    if ASCEND_IS_AIC {
        return;
    }

#if ASCENDC_CPU_DEBUG
    bool ans = cumSumInfo.inner > 0 && (cumSumInfo.inner * sizeof(T) % ONE_BLK_SIZE == 0);
    ASCENDC_ASSERT(ans, { KERNEL_LOG(KERNEL_ERROR, "inner is %u, is not 32B aligned.", cumSumInfo.inner); });
    ans = srcTensor.GetSize() >= (cumSumInfo.inner * cumSumInfo.outter);
    ASCENDC_ASSERT(ans, { KERNEL_LOG(KERNEL_ERROR, "srcTensor size isn't enough!."); });
    ans = dstTensor.GetSize() >= (cumSumInfo.inner * cumSumInfo.outter);
    ASCENDC_ASSERT(ans, { KERNEL_LOG(KERNEL_ERROR, "dstTensor size isn't enough!."); });
    if (config.outputLastRow) {
        ans = lastRowTensor.GetSize() >= cumSumInfo.inner;
        ASCENDC_ASSERT(ans, { KERNEL_LOG(KERNEL_ERROR, "outputLastRow size isn't enough!."); });
    }
#endif

    if constexpr (config.isLastAxis) {
        uint32_t minCastTempBufferSize = 0;
        if constexpr (sizeof(T) == 2) {  // 2 is for half
            minCastTempBufferSize = cumSumInfo.inner * NCHW_CONV_ADDR_LIST_SIZE * sizeof(half);
        }
        const uint32_t minTmpBufferSize = minCastTempBufferSize + NCHW_CONV_ADDR_LIST_SIZE * cumSumInfo.inner *
                                                                      sizeof(T) * 2;  // 2次transpose均需要tempBuffer
        const uint32_t tmpBufferSize = sharedTmpBuffer.GetSize();
#if ASCENDC_CPU_DEBUG
        ASCENDC_ASSERT((tmpBufferSize >= minTmpBufferSize), {
            KERNEL_LOG(KERNEL_ERROR,
                "tmpBufferSize can't smaller than minTmpBufferSize, tmpBufferSize is %u, minTmpBufferSize is %u!",
                tmpBufferSize,
                minTmpBufferSize);
        });
#endif
        // 针对outter做for循环，每次最少处理16行的数据
        const uint32_t oneRepeateSize = tmpBufferSize / minTmpBufferSize * NCHW_CONV_ADDR_LIST_SIZE;
        const uint32_t rangeM = cumSumInfo.outter / oneRepeateSize;
        const uint32_t tailM = cumSumInfo.outter - oneRepeateSize * rangeM;
        uint32_t dstLocalOffset = 0;
        uint32_t srcLocalOffset = 0;
        LocalTensor<T> tmpBuffer = sharedTmpBuffer.ReinterpretCast<T>();
        for (uint32_t i = 0; i < rangeM; i++) {
            CumSumLastDim<T>(
                dstTensor[dstLocalOffset], srcTensor[srcLocalOffset], tmpBuffer, {oneRepeateSize, cumSumInfo.inner});
            dstLocalOffset += cumSumInfo.inner * oneRepeateSize;
            srcLocalOffset += cumSumInfo.inner * oneRepeateSize;
        }

        if (tailM != 0) {
            CumSumLastDim<T>(
                dstTensor[dstLocalOffset], srcTensor[srcLocalOffset], tmpBuffer, {tailM, cumSumInfo.inner});
        }
    } else {
        CumSumFirstDim<T>(dstTensor, srcTensor, sharedTmpBuffer, cumSumInfo);
    }

    if constexpr (config.outputLastRow) {
        SetMaskCount();
        SetVectorMask<T>(0, cumSumInfo.inner);
        Adds<T, false>(lastRowTensor,
            dstTensor[(cumSumInfo.outter - 1) * cumSumInfo.inner],
            0,
            MASK_PLACEHOLDER,
            1,
            {1, 1, DEFAULT_REPEAT_STRIDE, DEFAULT_REPEAT_STRIDE});
        PipeBarrier<PIPE_V>();
        SetMaskNorm();
        ResetMask();
    }
}

/* !
 * \brief This function calculates the average based on the orientation of the last axis or fist axis.
 * For details about the interface description, see
 * https://pytorch.org/docs/stable/generated/torch.cumsum.html
 *
 * \note support data type: half and float
 *
 * \param [out] dstTensor, output LocalTensor
 * \param [out] lastRowTensor, the last row of the output LocalTensor
 * \param [in] srcTensor, input LocalTensor
 * \param [in] cumSumInfo, shape information of srcTensor
 */

template <typename T, const CumSumConfig &config = defaultCumSumConfig>
__aicore__ inline void CumSum(LocalTensor<T> &dstTensor, LocalTensor<T> &lastRowTensor, const LocalTensor<T> &srcTensor,
    const CumSumInfo &cumSumInfo)
{
    if ASCEND_IS_AIC {
        return;
    }
    LocalTensor<uint8_t> sharedTmpBuffer;
    bool ans = PopStackBuffer<uint8_t, TPosition::LCM>(sharedTmpBuffer);
    ASCENDC_ASSERT((ans), { KERNEL_LOG(KERNEL_ERROR, "PopStackBuffer Error!"); });
    CumSum<T, config>(dstTensor, lastRowTensor, srcTensor, sharedTmpBuffer, cumSumInfo);
}

#pragma end_pipe
}  // namespace AscendC

#endif

#endif  // LIB_CUMSUM_H