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
 * \file matmul_intf.h
 * \brief
 */
#ifndef LIB_MATMUL_MATMUL_INTF_H
#define LIB_MATMUL_MATMUL_INTF_H
#if __CCE_AICORE__ == 220
#include "../impl/kfc/kernel_kfc.h"
#else
#include "lib/matmul/matmul.h"
#endif

#define REGIST_MATMUL_OBJ_STATIC REGIST_MATMUL_OBJ
#ifdef ASCENDC_CPU_DEBUG
#if __CCE_AICORE__ == 220
#ifdef ASCENDC_CUBE_ONLY
namespace Gemm {
template <class A_TYPE, class B_TYPE, class C_TYPE, class BIAS_TYPE = C_TYPE, const auto& MM_CFG = CFG_NORM,
    class MM_CB = MatmulCallBackFunc<nullptr, nullptr, nullptr>, MATMUL_POLICY_VARIADIC_TEMPLATE_OF(MATMUL_POLICY)>
using Matmul = MatmulImpl<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, MM_CFG, MM_CB, MATMUL_POLICY...>;
}
template <class T, class... Args> __aicore__ static T* GetCurTiling(T* t, Args&&... b)
{
    return t;
}

template <class T, class... Args>
__aicore__ inline void InitCurObjSkip(AscendC::TPipe* tpipe, T* a,
    Args&&... b)
{
    InitCurObj(tpipe, b...);
}

template <class T, class... Args>
__aicore__ inline void InitCurObj(AscendC::TPipe* tpipe, T& a, Args&&... b)
{
    ASSERT(tpipe != nullptr && "tpipe cannot be nullptr");
    if constexpr (sizeof...(b) == 0) {
        SetTPipe(a, tpipe);
    } else {
        auto tiling = GetCurTiling(b...);
        a.SetSubBlockIdx(0);
        a.Init(tiling, tpipe);
        if constexpr (sizeof...(b) > 1) {
            InitCurObjSkip(tpipe, b...);
        }
    }
}

#define REGIST_MATMUL_OBJ(tpipe, workspace, ...) \
    InitCurObj(tpipe, __VA_ARGS__)

#else
namespace Gemm {
template <class A_TYPE, class B_TYPE, class C_TYPE, class BIAS_TYPE = C_TYPE, const auto& MM_CFG = CFG_NORM,
    class MM_CB = MatmulCallBackFunc<nullptr, nullptr, nullptr>, MATMUL_POLICY_VARIADIC_TEMPLATE_OF(MATMUL_POLICY)>
using Matmul = MatmulClient<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, MM_CFG, MM_CB, MATMUL_POLICY...>;
}
#define REGIST_MATMUL_OBJ(tpipe, workspace, ...)              \
    if ASCEND_IS_AIC {                                        \
        AscendC::KfcServer server;                            \
        server.Init(workspace);                               \
        server.InitObj(tpipe, __VA_ARGS__);                   \
        while (server.isRun()) {                              \
            server.Run(__VA_ARGS__);                          \
        };                                                    \
        server.Quit();                                        \
        return;                                               \
    }                                                         \
    AscendC::KfcCommClient __kfcClient__(workspace, AscendC::GetSubBlockIdx()); \
    AscendC::g_kfcClient = &__kfcClient__;                             \
    AscendC::SetMatrixKfc(tpipe, &__kfcClient__, 0, workspace, __VA_ARGS__)
#endif

#else

template <class T, class... Args> __aicore__ static T* GetCurTiling(T* t, Args&&... b)
{
    return t;
}

template <class T, class... Args>
__aicore__ inline void InitCurObjSkip(AscendC::TPipe* tpipe, T* a,
    Args&&... b)
{
    InitCurObj(tpipe, b...);
}

template <class T, class... Args>
__aicore__ inline void InitCurObj(AscendC::TPipe* tpipe, T& a, Args&&... b)
{
    ASSERT(tpipe != nullptr && "tpipe cannot be nullptr");
    if constexpr (sizeof...(b) == 0) {
        SetTPipe(a, tpipe);
    } else {
        auto tiling = GetCurTiling(b...);
        a.Init(tiling, tpipe);
        if constexpr (sizeof...(b) > 1) {
            InitCurObjSkip(tpipe, b...);
        }
    }
}

#define REGIST_MATMUL_OBJ(tpipe, workspace, ...) \
    InitCurObj(tpipe, __VA_ARGS__)
namespace Gemm {
template <class A_TYPE, class B_TYPE, class C_TYPE, class BIAS_TYPE = C_TYPE, const auto& MM_CFG = CFG_NORM,
    class MM_CB = MatmulCallBackFunc<nullptr, nullptr, nullptr>, MATMUL_POLICY_VARIADIC_TEMPLATE_OF(MATMUL_POLICY)>
using Matmul = MatmulImpl<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, MM_CFG, MM_CB, MATMUL_POLICY...>;
}

#endif

#else

#ifdef __DAV_C220_CUBE__
#ifdef ASCENDC_CUBE_ONLY
template <class T, class... Args> __aicore__ static T* GetCurTiling(T* t, Args&&... b)
{
    return t;
}

template <class T, class... Args>
__aicore__ inline void InitCurObjSkip(AscendC::TPipe* tpipe, T* a,
    Args&&... b)
{
    InitCurObj(tpipe, b...);
}

template <class T, class... Args>
__aicore__ inline void InitCurObj(AscendC::TPipe* tpipe, T& a, Args&&... b)
{
    ASSERT(tpipe != nullptr && "tpipe cannot be nullptr");
    if constexpr (sizeof...(b) == 0) {
        SetTPipe(a, tpipe);
    } else {
        auto tiling = GetCurTiling(b...);
        a.SetSubBlockIdx(0);
        a.Init(tiling, tpipe);
        if constexpr (sizeof...(b) > 1) {
            InitCurObjSkip(tpipe, b...);
        }
    }
}

#ifdef ASCENDC_TIME_STAMP_ON
#define REGIST_MATMUL_OBJ(tpipe, workspace, ...) \
    InitCurObj(tpipe, __VA_ARGS__); \
    AscendC::AscendCTimeStamp(static_cast<uint32_t>(AscendC::TimeStampId::TIME_STAMP_MATMUL_SERVER_OBJ))
#else
#define REGIST_MATMUL_OBJ(tpipe, workspace, ...) \
    InitCurObj(tpipe, __VA_ARGS__)
#endif

#define REGIST_MATMUL_OBJ_REMOTE(tpipe, workspace, ...)
namespace Gemm {
template <class A_TYPE, class B_TYPE, class C_TYPE, class BIAS_TYPE = C_TYPE, const auto& MM_CFG = CFG_NORM,
    class MM_CB = MatmulCallBackFunc<nullptr, nullptr, nullptr>, MATMUL_POLICY_VARIADIC_TEMPLATE_OF(MATMUL_POLICY)>
using Matmul = MatmulImpl<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, MM_CFG, MM_CB, MATMUL_POLICY...>;
}
#else
#ifdef ASCENDC_TIME_STAMP_ON
#define REGIST_MATMUL_OBJ(tpipe, workspace, ...) \
    if ASCEND_IS_AIC {           \
        AscendC::KfcServer server;               \
        AscendC::AscendCTimeStamp(static_cast<uint32_t>(AscendC::TimeStampId::TIME_STAMP_MATMUL_SERVER)); \
        server.Init(workspace);                  \
        AscendC::AscendCTimeStamp(static_cast<uint32_t>(AscendC::TimeStampId::TIME_STAMP_MATMUL_SERVER_INIT)); \
        server.InitObj(tpipe, __VA_ARGS__);      \
        AscendC::AscendCTimeStamp(static_cast<uint32_t>(AscendC::TimeStampId::TIME_STAMP_MATMUL_SERVER_OBJ)); \
        while (server.isRun()) {                 \
            server.Run(__VA_ARGS__);             \
        };                                       \
        server.Quit();                           \
        return;                                  \
    }
#else
#define REGIST_MATMUL_OBJ(tpipe, workspace, ...) \
    if ASCEND_IS_AIC {           \
        AscendC::KfcServer server;               \
        server.Init(workspace);                  \
        server.InitObj(tpipe, __VA_ARGS__);      \
        while (server.isRun()) {                 \
            server.Run(__VA_ARGS__);             \
        };                                       \
        server.Quit();                           \
        return;                                  \
    }
#endif
namespace Gemm {
template <class A_TYPE, class B_TYPE, class C_TYPE, class BIAS_TYPE = C_TYPE, const auto& MM_CFG = CFG_NORM,
    class MM_CB = MatmulCallBackFunc<nullptr, nullptr, nullptr>, MATMUL_POLICY_VARIADIC_TEMPLATE_OF(MATMUL_POLICY)>
using Matmul = MatmulServiceAux<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, MM_CFG, MM_CB, MATMUL_POLICY...>;
}
#endif

#elif defined(__DAV_C220_VEC__)
#ifdef ASCENDC_CUBE_ONLY
#define REGIST_MATMUL_OBJ(tpipe, workspace, ...) \
    return
#else
#ifdef ASCENDC_TIME_STAMP_ON
#define REGIST_MATMUL_OBJ(tpipe, workspace, ...)                             \
    AscendC::KfcCommClient __kfcClient__(workspace, GetSubBlockIdx());       \
    g_kfcClient = &__kfcClient__;                                            \
    AscendC::AscendCTimeStamp(static_cast<uint32_t>(AscendC::TimeStampId::TIME_STAMP_MATMUL_CLIENT_KFC)); \
    AscendC::SetMatrixKfc(tpipe, &__kfcClient__, 0, workspace, __VA_ARGS__); \
    AscendC::AscendCTimeStamp(static_cast<uint32_t>(AscendC::TimeStampId::TIME_STAMP_MATMUL_MATRIX_KFC)); \
    AscendC::WaitEvent(matmul::WORKSPACE_SYNC_ID);                                                          \
    AscendC::AscendCTimeStamp(static_cast<uint32_t>(AscendC::TimeStampId::TIME_STAMP_MATMUL_WAIT_EVE))
#else
#define REGIST_MATMUL_OBJ(tpipe, workspace, ...)                             \
    AscendC::KfcCommClient __kfcClient__(workspace, AscendC::GetSubBlockIdx());       \
    AscendC::g_kfcClient = &__kfcClient__;                                            \
    AscendC::SetMatrixKfc(tpipe, &__kfcClient__, 0, workspace, __VA_ARGS__); \
    AscendC::WaitEvent(matmul::WORKSPACE_SYNC_ID)
#endif
#endif
namespace Gemm {
template <class A_TYPE, class B_TYPE, class C_TYPE, class BIAS_TYPE = C_TYPE, const auto& MM_CFG = CFG_NORM,
    class MM_CB = MatmulCallBackFunc<nullptr, nullptr, nullptr>, MATMUL_POLICY_VARIADIC_TEMPLATE_OF(MATMUL_POLICY)>
using Matmul = MatmulClient<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, MM_CFG, MM_CB, MATMUL_POLICY...>;
}
#else

template <class T, class... Args> __aicore__ static T* GetCurTiling(T* t, Args&&... b)
{
    return t;
}

template <class T, class... Args>
__aicore__ inline void InitCurObjSkip(AscendC::TPipe* tpipe, T* a,
    Args&&... b)
{
    InitCurObj(tpipe, b...);
}

template <class T, class... Args>
__aicore__ inline void InitCurObj(AscendC::TPipe* tpipe, T& a, Args&&... b)
{
    ASSERT(tpipe != nullptr && "tpipe cannot be nullptr");
    if constexpr (sizeof...(b) == 0) {
        SetTPipe(a, tpipe);
    } else {
        auto tiling = GetCurTiling(b...);
        a.Init(tiling, tpipe);
        if constexpr (sizeof...(b) > 1) {
            InitCurObjSkip(tpipe, b...);
        }
    }
}

#ifdef ASCENDC_TIME_STAMP_ON
#define REGIST_MATMUL_OBJ(tpipe, workspace, ...) \
    InitCurObj(tpipe, __VA_ARGS__); \
    AscendC::AscendCTimeStamp(static_cast<uint32_t>(AscendC::TimeStampId::TIME_STAMP_MATMUL_OBJ))
#else
#define REGIST_MATMUL_OBJ(tpipe, workspace, ...) \
    InitCurObj(tpipe, __VA_ARGS__)
#endif
namespace Gemm {
template <class A_TYPE, class B_TYPE, class C_TYPE, class BIAS_TYPE = C_TYPE, const auto& MM_CFG = CFG_NORM,
    class MM_CB = MatmulCallBackFunc<nullptr, nullptr, nullptr>, MATMUL_POLICY_VARIADIC_TEMPLATE_OF(MATMUL_POLICY)>
using Matmul = MatmulImpl<A_TYPE, B_TYPE, C_TYPE, BIAS_TYPE, MM_CFG, MM_CB, MATMUL_POLICY...>;
} //namespace Gemm
#endif
#endif
#endif