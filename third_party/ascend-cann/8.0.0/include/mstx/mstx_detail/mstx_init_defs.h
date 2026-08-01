/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef MSTX_IMPL_GUARD
#error Do not include this file directly, please include msToolsExt.h instead.
#endif

MSTX_INNER_FUNC_DEFINE void mstxMarkAInit(const char* message, aclrtStream stream)
{
    mstxInitOnce();
    return mstxMarkA(message, stream);
}

MSTX_INNER_FUNC_DEFINE mstxRangeId mstxRangeStartAInit(const char* message, aclrtStream stream)
{
    mstxInitOnce();
    return mstxRangeStartA(message, stream);
}

MSTX_INNER_FUNC_DEFINE void mstxRangeEndInit(mstxRangeId id)
{
    mstxInitOnce();
    return mstxRangeEnd(id);
}

