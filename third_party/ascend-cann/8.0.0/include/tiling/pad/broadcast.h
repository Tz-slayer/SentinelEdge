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
 * \file broadcast.h
 * \brief
 */
#ifndef LIB_PAD_BROADCAST_H
#define LIB_PAD_BROADCAST_H

#include "kernel_tensor.h"
#include "kernel_operator_intf.h"
#include "../../impl/pad/broadcast/broadcast_common_impl.h"

#if __CCE_AICORE__ >= 200

namespace AscendC {
#pragma begin_pipe(V)
constexpr uint32_t HALF_ONE_BLK_SIZE = 16;
/*
 * @ingroup BroadCast, now only support dim=1 or dim=2
 * @brief https://numpy.org.cn/user/basics/broadcasting.html
 * @param [out] dstTensor, output LocalTensor
 * @param [in] srcTensor, input LocalTensor
 * @param [in] dstShape, the shape of dst tensor
 * @param [in] srcShape, the shape of src tensor
 * @param [in] sharedTmpBuffer input local temporary Tensor
 */
template <typename T, int32_t dim, int32_t axis, bool isReuseSource = false>
__aicore__ inline void BroadCast(LocalTensor<T> &dstLocal, const LocalTensor<T> &srcLocal, const uint32_t dstShape[dim],
    const uint32_t srcShape[dim], LocalTensor<uint8_t> &sharedTmpBuffer)
{
    if ASCEND_IS_AIC {
        return;
    }
    TRACE_START(TraceId::BroadCast);
    if constexpr (sizeof(T) == 1) {
        LocalTensor<half> tmpBuffer = sharedTmpBuffer.ReinterpretCast<half>();
        uint32_t srcSize = 1;
        uint32_t dstSize = 1;
        for (uint32_t i = 0; i < dim; i++) {
            srcSize *= srcShape[i];
            dstSize *= dstShape[i];
        }
        auto srcTempBuffer = tmpBuffer;
        const uint32_t alignSrcSize =  ((srcSize + HALF_ONE_BLK_SIZE - 1) / HALF_ONE_BLK_SIZE) * HALF_ONE_BLK_SIZE;
        const uint32_t alignDstSize =  ((dstSize + HALF_ONE_BLK_SIZE - 1) / HALF_ONE_BLK_SIZE) * HALF_ONE_BLK_SIZE;
        auto dstTempBuffer = tmpBuffer[alignSrcSize];
        auto tempTempBuffer = dstTempBuffer[alignDstSize];
        SetMaskCount();
        SetVectorMask<T, MaskMode::COUNTER>(srcSize);
        Cast<half, T, false>(srcTempBuffer,
            srcLocal,
            RoundMode::CAST_NONE,
            MASK_PLACEHOLDER,
            1,
            {1, 1, DEFAULT_REPEAT_STRIDE, HALF_DEFAULT_REPEAT_STRIDE});
        PipeBarrier<PIPE_V>();
        // After BroadCastCompute, Reset to Counter model
        BroadCastCompute<half, dim, axis, isReuseSource>(
            dstTempBuffer, srcTempBuffer, dstShape, srcShape, tempTempBuffer);
        SetVectorMask<T, MaskMode::COUNTER>(dstSize);
        Cast<T, half, false>(dstLocal,
            dstTempBuffer,
            RoundMode::CAST_NONE,
            MASK_PLACEHOLDER,
            1,
            {1, 1, HALF_DEFAULT_REPEAT_STRIDE, DEFAULT_REPEAT_STRIDE});
        PipeBarrier<PIPE_V>();
        SetMaskNorm();
        ResetMask();
    } else {
        LocalTensor<T> tmpBuffer = sharedTmpBuffer.ReinterpretCast<T>();
        SetMaskCount();
        BroadCastCompute<T, dim, axis, isReuseSource>(dstLocal, srcLocal, dstShape, srcShape, tmpBuffer);
        SetMaskNorm();
        ResetMask();
    }
    TRACE_STOP(TraceId::BroadCast);
}

/*
 * @ingroup BroadCast, now only support dim=1 or dim=2
 * @brief https://numpy.org.cn/user/basics/broadcasting.html
 * @param [out] dstTensor, output LocalTensor
 * @param [in] srcTensor, input LocalTensor
 * @param [in] dstShape, the shape of dst tensor
 * @param [in] srcShape, the shape of src tensor
 */
template <typename T, int32_t dim, int32_t axis, bool isReuseSource = false>
__aicore__ inline void BroadCast(LocalTensor<T> &dstLocal, const LocalTensor<T> &srcLocal, const uint32_t dstShape[dim],
    const uint32_t srcShape[dim])
{
    // Only for AI Vector Core.
    if ASCEND_IS_AIC {
        return;
    }
    LocalTensor<uint8_t> sharedTmpBuffer;
    bool ans = PopStackBuffer<uint8_t, TPosition::LCM>(sharedTmpBuffer);
    ASCENDC_ASSERT((ans), { KERNEL_LOG(KERNEL_ERROR, "PopStackBuffer Error!"); });
    BroadCast<T, dim, axis, isReuseSource>(dstLocal, srcLocal, dstShape, srcShape, sharedTmpBuffer);
}

#pragma end_pipe
}  // namespace AscendC

#endif

#endif  // LIB_PAD_BROADCAST_H