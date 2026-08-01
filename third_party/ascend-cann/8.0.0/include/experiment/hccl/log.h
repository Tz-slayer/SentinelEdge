/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
 * Description: 公共常量及宏
 */

#ifndef LOG_H
#define LOG_H

#include <hccl/base.h>
#include <hccl/hccl_types.h>
#include <securec.h>
#include <iostream>
#include <sstream>
#include <sys/syscall.h>
#include <unistd.h>
#include <string>

#ifndef T_DESC
#define T_DESC(_msg, _y) ((_y) ? true : false)
#endif

#if T_DESC("日志处理适配", true)

constexpr u32 HCCL_LOG_DEBUG = 0x0;
constexpr u32 HCCL_LOG_INFO = 0x1;
constexpr u32 HCCL_LOG_WARN = 0x2;
constexpr u32 HCCL_LOG_ERROR = 0x3;
constexpr u32 HCCL_LOG_NULL = 0x4;
constexpr u32 HCCL_LOG_OPLOG = 0x6;
constexpr u32 HCCL_LOG_RUN_INFO = 0xff;
constexpr u32 HCCL_LOG_RUN_WARNING = 0xfe;

enum class HcclSubModuleID {
    LOG_SUB_MODULE_ID_HCCL = 0,
    LOG_SUB_MODULE_ID_HCOM = 1,
    LOG_SUB_MODULE_ID_CLTM = 2,
    LOG_SUB_MODULE_ID_CUSTOM_OP = 3
};

/* 设置日志的commid和rankid */
// int LogGetLevelFromType(LogType type);              // 根据HCCL内部定义日志级别获取对应slog的日志级别
#ifndef LIKELY
#define LIKELY(x) (static_cast<bool>(__builtin_expect(static_cast<bool>(x), 1)))
#define UNLIKELY(x) (static_cast<bool>(__builtin_expect(static_cast<bool>(x), 0)))
#endif

/* 每一条日志的长度,超过该长度会申请堆内存 */
constexpr s32 LOG_TMPBUF_SIZE = 512;

void CallDlogInvalidType(int level, int errCode, std::string file, int line);
void CallDlogNoSzFormat(int level, int errCode, std::string file, int line);
void CallDlogMemError(int level, std::string file, int line);
void CallDlogPrintError(int level, std::string file, int line);
void CallDlog(int level, int sysCallBack, const char* buffer, std::string file, int line);

bool CheckDebugLogLevel();

bool CheckInfoLogLevel();

int HcclCheckLogLevel(int logLevel);

void SetErrToWarnSwitch(bool flag); // 设置True时修改日志级别：ERROR -> RUN_WARNING，设置False恢复日志级别

#define LOG_FUNC(moudle, level, fmt, ...) do { \
        if (DlogRecord == nullptr) { \
            DlogInner(moudle, level, fmt, ##__VA_ARGS__); \
        } else { \
            DlogRecord(moudle, level, fmt, ##__VA_ARGS__); \
        } \
    } while (0)

#define LOG_PRINT(logType, szFormat, ...) do {                        \
    if (UNLIKELY(HcclCheckLogLevel(logType) == 1)) { \
        char stackLogBuffer[LOG_TMPBUF_SIZE]; /* 使用栈中的buffer, 小而快 */        \
        if (szFormat == nullptr) {                                            \
            CallDlogNoSzFormat(HCCL_LOG_ERROR, HCCL_ERROR_CODE(HCCL_E_INTERNAL), \
                __FILE__, __LINE__); \
        } else {                                                                  \
            if (memset_s(stackLogBuffer, LOG_TMPBUF_SIZE, 0, sizeof(stackLogBuffer)) != EOK) {  \
                CallDlogMemError(HCCL_LOG_ERROR, __FILE__, __LINE__); \
            } else if ((snprintf_s(stackLogBuffer, sizeof(stackLogBuffer),        \
                (sizeof(stackLogBuffer) - 1), szFormat, ##__VA_ARGS__) == -1) &&  \
                (stackLogBuffer[0] == 0)) {                                       \
                CallDlogPrintError(HCCL_LOG_ERROR, __FILE__, __LINE__);                   \
            } else {                                                              \
                /* 如果collectiveID和rankID都为空，则默认输出为PID和TID */           \
                CallDlog(logType, \
                    syscall(SYS_gettid), stackLogBuffer, __FILE__, __LINE__);     \
            }                                                                     \
        }                                                                         \
    } \
} while (0)

/* 当前日志级别，为了优化性能，日志EVENT  判断在宏入口检查 */
/* 使用宏记录日志, 以便获取日志在代码中的位置 */
#define MODULE_DEBUG(format, ...) do { \
    LOG_PRINT(HCCL_LOG_DEBUG, format, ##__VA_ARGS__); \
} while (0)

#define MODULE_INFO(format, ...) do { \
    LOG_PRINT(HCCL_LOG_INFO, format, ##__VA_ARGS__); \
} while (0)

#define MODULE_WARNING(format, ...) do { \
    LOG_PRINT(HCCL_LOG_WARN, format, ##__VA_ARGS__); \
} while (0)

#define MODULE_ERROR(format, ...) do { \
    LOG_PRINT(HCCL_LOG_ERROR, format, ##__VA_ARGS__); \
} while (0)


/* 运行日志，记录在run目录下 */
#define MODULE_RUN_INFO(format, ...) do { \
    LOG_PRINT(HCCL_LOG_RUN_INFO, format, ##__VA_ARGS__); \
} while (0)

#define MODULE_RUN_WARNING(format, ...) do { \
    LOG_PRINT(HCCL_LOG_RUN_WARNING, format, ##__VA_ARGS__); \
} while (0)

// 错误码
const u64 SYSTEM_RESERVE_ERROR = 0;
const u64 HCCL_MODULE_ID = 5;

/* 预定义日志宏, 便于使用 */
#define HCCL_DEBUG(...) MODULE_DEBUG(__VA_ARGS__)
#define HCCL_INFO(...) MODULE_INFO(__VA_ARGS__)
#define HCCL_WARNING(...) MODULE_WARNING(__VA_ARGS__)
#define HCCL_ERROR(...) MODULE_ERROR(__VA_ARGS__)
/* 运行日志 */
#define HCCL_RUN_INFO(...) MODULE_RUN_INFO(__VA_ARGS__)
#define HCCL_RUN_WARNING(...) MODULE_RUN_WARNING(__VA_ARGS__)
#define HCCL_USER_CRITICAL_LOG(...) MODULE_RUN_INFO(__VA_ARGS__)


#define HCCL_ERROR_CODE(error) ((SYSTEM_RESERVE_ERROR << 32) + (HCCL_MODULE_ID << 24) + \
    ((static_cast<u64>(HcclSubModuleID::LOG_SUB_MODULE_ID_HCCL)) << 16) + static_cast<u64>(error))
#define HCOM_ERROR_CODE(error) ((SYSTEM_RESERVE_ERROR << 32) + (HCCL_MODULE_ID << 24) + \
    ((static_cast<u64>(HcclSubModuleID::LOG_SUB_MODULE_ID_HCOM)) << 16) + static_cast<u64>(error))
#endif

#if T_DESC("公共代码宏", true)

// 检查C++11的智能指针, 若为空, 则记录日志, 并返回错误
#define CHK_SMART_PTR_NULL(smart_ptr)                                                            \
    do {                                                                                                    \
        if (UNLIKELY(!(smart_ptr))) {                                                   \
            HCCL_ERROR("errNo[0x%016llx] ptr [%s] is nullptr, return HCCL_E_PTR", \
                HCCL_ERROR_CODE(HCCL_E_PTR), \
                #smart_ptr);                                                                                \
            return HCCL_E_PTR;                                                                              \
        }                                                                                                   \
    } while (0)

// 检查C++11的智能指针, 若为空, 则记录日志, 并返回
#define CHK_SMART_PTR_RET_NULL(smart_ptr)                       \
    do {                                                        \
        if (UNLIKELY(!(smart_ptr))) {                           \
            HCCL_ERROR("errNo[0x%016llx] smart_ptr is nullptr.",   \
            HCCL_ERROR_CODE(HCCL_E_PTR));                       \
            return;                                             \
        }                                                       \
    } while (0)

/* 检查指针, 若指针为NULL, 则记录日志, 并返回错误 */
#define CHK_PTR_NULL(ptr)                                                                               \
    do {                                                                                                           \
        if (UNLIKELY((ptr) == nullptr)) {                  \
            HCCL_ERROR("errNo[0x%016llx] ptr [%s] is nullptr, return HCCL_E_PTR", \
            HCCL_ERROR_CODE(HCCL_E_PTR), #ptr); \
            return HCCL_E_PTR;                                                                                     \
        }                                                                                                          \
    } while (0)

/* 检查函数返回值, 记录指定日志, 并返回指定错误码 */
#define CHK_PRT_RET(result, exeLog, retCode) \
    do {                                      \
        if (UNLIKELY(result)) {                         \
            exeLog;                           \
            return retCode;                   \
        }                                     \
    } while (0)

/* 检查函数返回值, 记录指定日志, 函数不返回 */
#define CHK_PRT_CONT(result, exeLog) \
    do {                                      \
        if (UNLIKELY(result)) {               \
            exeLog;                           \
        }                                     \
    } while (0)

/* 检查函数返回值, 并返回指定错误码 */
#define CHK_RET(call)                                 \
    do {                                              \
        HcclResult hcclRet = call;                        \
        if (UNLIKELY(hcclRet != HCCL_SUCCESS)) {                    \
            if (hcclRet == HCCL_E_AGAIN) {                \
                HCCL_WARNING("call trace: hcclRet -> %d", hcclRet); \
            } else {                                  \
                HCCL_ERROR("call trace: hcclRet -> %d", hcclRet); \
            }                                         \
            return hcclRet;                               \
        }                                             \
    } while (0)

/* 检查函数返回值, 返错时打印函数名及通信域标识 */
#define CHK_RET_AND_PRINT_IDE(call, identifier)         \
    do {                                              \
        HcclResult hcclRet = call;                        \
        if (UNLIKELY(hcclRet != HCCL_SUCCESS)) {                    \
            HCCL_RUN_INFO("[HCCL_TRACE]%s identifier[%s]", __func__, identifier); \
            if (hcclRet == HCCL_E_AGAIN) {                \
                HCCL_WARNING("call trace: hcclRet -> %d", hcclRet); \
            } else {                                  \
                HCCL_ERROR("call trace: hcclRet -> %d", hcclRet); \
            }                                         \
            return hcclRet;                               \
        }                                             \
    } while (0)

/* 检查函数返回值, 并返回空 */
#define CHK_RET_NULL(call)                                 \
    do {                                              \
        HcclResult ret = call;                        \
        if (UNLIKELY(ret != HCCL_SUCCESS)) {                    \
            if (ret == HCCL_E_AGAIN) {                \
                HCCL_WARNING("call trace: ret -> %d", ret); \
            } else {                                  \
                HCCL_ERROR("call trace: ret -> %d", ret); \
            }                                         \
            return;                               \
        }                                             \
    } while (0)

/* 检查函数返回值, 打印错误码, 函数不返回 */
#define CHK_PRT(call)                                 \
    do {                                              \
        HcclResult ret = call;                        \
        if (UNLIKELY(ret != HCCL_SUCCESS)) {                    \
            if (ret == HCCL_E_AGAIN) {                \
                HCCL_WARNING("call trace: ret -> %d", ret); \
            } else {                                  \
                HCCL_ERROR("call trace: ret -> %d", ret); \
            }                                         \
        }                                             \
    } while (0)

/* 检查函数返回值, 并返回HCCL_E_INTERNAL错误码 */
#define CHK_SAFETY_FUNC_RET(call)                                 \
    do {                                              \
        s32 ret = call;                        \
        if (UNLIKELY(ret != EOK)) {                    \
            HCCL_ERROR("call trace: safety func err ret -> %d", ret); \
            return HCCL_E_INTERNAL;                               \
        }                                             \
    } while (0)

/* 检查result. 若错误, 则设置错误并break */
#define CHK_PRT_BREAK(result, exeLog, exeCmd) \
    if (UNLIKELY(result)) {                              \
        exeLog;                                \
        exeCmd;                                \
        break;                                 \
    }

#define EXECEPTION_CATCH(expression, retExp)                     \
    do {                                                         \
        try {                                                    \
            expression;                                          \
        } catch (std::exception& e) {                            \
            HCCL_ERROR("Failed, exception caught:%s", e.what()); \
            retExp;                                              \
        }                                                        \
    } while (0)

/* 若new失败, 则捕获异常返回 */
#ifndef NEW_NOTHROW
#define NEW_NOTHROW(pointer, constructor, retExp)        \
    do {                                                 \
        pointer = new(std::nothrow) constructor;         \
        if (pointer == nullptr) {                        \
            HCCL_ERROR("Memory application failed.");     \
            retExp;                                      \
        }                                                \
    } while (0)
#endif

template <typename T>
void ArrayToStringAndPrint(T arr[], int size, const char *name)
{
    if (arr == nullptr) {
        HCCL_ERROR("Array Data, %s: data is nullptr", name);
        return;
    }

    constexpr int NUM_IN_ROW = 50;
    std::stringstream ss;
    for (int i = 0; i < size; i++) {
        ss << arr[i] << " ";
        if ((i + 1) % NUM_IN_ROW == 0) {
            HCCL_DEBUG("Array Data, %s: %s", name, ss.str().c_str());
            ss.clear();
            ss.str(std::string());
        }
    }

    HCCL_DEBUG("Array Data, %s: %s", name, ss.str().c_str());
}

#ifdef DUMP_DATA
#define PRINT_ARRAY(arr, cnt, name) \
    ArrayToStringAndPrint(arr, cnt, name)

#else
#define PRINT_ARRAY(arr, cnt, name)
#endif

#endif

#endif // LOG_H