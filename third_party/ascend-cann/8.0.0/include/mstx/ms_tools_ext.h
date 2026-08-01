/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef MSTX_TOOLS_EXT
#define MSTX_TOOLS_EXT

#if defined(MSTX_NO_IMPL)
#define MSTX_DECLSPEC
#else
#define MSTX_DECLSPEC inline static
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define MSTX_SUCCESS 0
#define MSTX_FAIL 1

typedef uint64_t mstxRangeId;
typedef void* aclrtStream;

/**
 * @ingroup MSTX
 * @brief mstx mark
 *
 * @param message [IN]    message for mark, cannot be null
 * @param stream  [IN]    stream used by mark, which can be set to null if not needed.
 */
MSTX_DECLSPEC void mstxMarkA(const char *message, aclrtStream stream);

/**
 * @ingroup MSTX
 * @brief mstx range start
 *
 * @param message [IN]    message to mark range, cannot be null
 * @param stream  [IN]    stream used by range, which can be set to null if not needed.
 * @retval mstxRangeId  the range id used for mstxRangeEnd
 * @retval return 0 if range start failed
 */
MSTX_DECLSPEC mstxRangeId mstxRangeStartA(const char *message, aclrtStream stream);

/**
 * @ingroup MSTX
 * @brief mstx range end
 *
 * @param id [IN]    the range id return by range start api
 */
MSTX_DECLSPEC void mstxRangeEnd(mstxRangeId id);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#define MSTX_IMPL_GUARD // Ensure other headers cannot include directly

#include "mstx_detail/mstx_types.h"

#ifndef MSTX_NO_IMPL
#include "mstx_detail/mstx_impl.h"
#endif // MSTX_NO_IMPL

#undef MSTX_IMPL_GUARD
#endif // MSTX_TOOLS_EXT
