/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef MSTX_TYPES_H
#define MSTX_TYPES_H
#ifndef MSTX_IMPL_GUARD
#error Do not include this file directly, please include msToolsExt.h instead.
#endif

typedef void (* mstxMarkAFunc)(const char* message, aclrtStream stream);
typedef mstxRangeId (* mstxRangeStartAFunc)(const char* message, aclrtStream stream);
typedef void (* mstxRangeEndFunc)(mstxRangeId id);

typedef enum mstxFuncModule {
    MSTX_API_MODULE_INVALID                 = 0,
    MSTX_API_MODULE_CORE                    = 1,
    MSTX_API_MODULE_SIZE,                   // end of the enum, new enum items must be added before this
    MSTX_API_MODULE_FORCE_INT               = 0x7fffffff
} mstxFuncModule;

typedef enum mstxImplCoreFuncId {
    MSTX_API_CORE_INVALID                 =  0,
    MSTX_API_CORE_MARK_A                   =  1,
    MSTX_API_CORE_RANGE_START_A             =  2,
    MSTX_API_CORE_RANGE_END                =  3,
    MSTX_API_CORE_SIZE,                   // end of the enum, new enum items must be added before this
    MSTX_API_CORE_FORCE_INT = 0x7fffffff
} mstxImplCoreFuncId;


typedef void (* mstxFuncPointer)(void);
typedef mstxFuncPointer** mstxFuncTable;

typedef int (* mstxGetModuleFuncTableFunc)(mstxFuncModule module, mstxFuncTable *outTable, unsigned int *outSize);
typedef int (* mstxInitInjectionFunc)(mstxGetModuleFuncTableFunc getFuncTable);
#endif // MSTX_TYPES_H
