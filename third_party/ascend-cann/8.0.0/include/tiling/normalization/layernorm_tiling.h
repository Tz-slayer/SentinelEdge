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
 * \file layernorm_tiling.h
 * \brief
 */
#ifndef LIB_NORMALIZATION_LAYERNORM_TILING_H
#define LIB_NORMALIZATION_LAYERNORM_TILING_H
#include "graph/tensor.h"
#include "layernorm_tilingdata.h"
namespace AscendC {
void GetLayerNormMaxMinTmpSize(const ge::Shape& srcShape, const uint32_t typeSize, const bool isReuseSource,
    uint32_t& maxValue, uint32_t& minValue);

void GetLayerNormNDTillingInfo(const ge::Shape& srcShape, const uint32_t stackBufferSize, const uint32_t typeSize,
    const bool isReuseSource, optiling::LayerNormTiling& tilling);

void GetWelfordUpdateMaxMinTmpSize(const ge::Shape& srcShape, const uint32_t typeSizeT, const uint32_t typeSizeU,
    const bool isReuseSource, const bool isInplace, uint32_t& maxValue, uint32_t& minValue);

void GetLayerNormMaxMinTmpSize(const ge::Shape& srcShape, const uint32_t typeSize, const bool isReuseSource,
    const bool isComputeRstd, const bool isOnlyOutput, uint32_t& maxValue, uint32_t& minValue);

void GetLayerNormNDTilingInfo(const ge::Shape& srcShape, const uint32_t stackBufferSize, const uint32_t typeSize,
    const bool isReuseSource, const bool isComputeRstd, optiling::LayerNormSeparateTiling& tiling);

}
#endif // LIB_NORMALIZATION_LAYERNORM_TILING_H
