/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef MSTX_IMPL_GUARD
#error Do not include this file directly, please include msToolsExt.h instead(except when MSTX_NO_IMPL is defined).
#endif

MSTX_DECLSPEC void mstxMarkA(const char *message, aclrtStream stream)
{
#ifndef MSTX_DISABLE
    mstxMarkAFunc local = g_mstxContext.mstxMarkAPtr;
    if (local != 0) {
        (*local)(message, stream);
    }
#endif // MSTX_DISABLE
}

MSTX_DECLSPEC mstxRangeId mstxRangeStartA(const char *message, aclrtStream stream)
{
#ifndef MSTX_DISABLE
    mstxRangeStartAFunc local = g_mstxContext.mstxRangeStartAPtr;
    if (local != 0) {
        return (*local)(message, stream);
    } else {
        return (mstxRangeId)0;
    }
#else
    return (mstxRangeId)0;
#endif // MSTX_DISABLE
}

MSTX_DECLSPEC void mstxRangeEnd(mstxRangeId id)
{
#ifndef MSTX_DISABLE
    mstxRangeEndFunc local = g_mstxContext.mstxRangeEndPtr;
    if (local != 0) {
        (*local)(id);
    }
#endif // MSTX_DISABLE
}
