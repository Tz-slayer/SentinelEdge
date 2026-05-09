#include "sentinel/ascend/acl_runtime.hpp"

#include <mutex>

#include <acl/acl.h>
#include <acl/acl_rt.h>

namespace sentinel {
namespace {

/**
 * @brief 进程级 AscendCL 运行时状态。
 */
struct RuntimeState {
    std::mutex mutex;
    int ref_count{0};
    int device_id{-1};
    aclrtContext context{nullptr};
    bool acl_initialized{false};
    bool device_set{false};
};

/**
 * @brief 获取进程级运行时状态单例。
 * @return 可变运行时状态引用。
 */
RuntimeState& runtime_state()
{
    static RuntimeState state;
    return state;
}

/**
 * @brief 判断 AscendCL 返回码是否表示成功。
 * @param error AscendCL 返回码。
 * @return 成功返回 `true`。
 */
bool acl_ok(aclError error) noexcept
{
    return error == ACL_SUCCESS;
}

} // namespace

/**
 * @brief 构造 AscendCL 错误上下文文本。
 * @param action 失败的 AscendCL API 名称。
 * @param error AscendCL 返回码。
 * @return 可读错误文本。
 */
std::string make_acl_error(std::string_view action, int error)
{
    return std::string(action) + " failed, aclError=" + std::to_string(error);
}

/**
 * @brief 保存设备编号并标记该对象持有一个引用。
 * @param device_id Ascend 设备编号。
 */
AclRuntimeSession::AclRuntimeSession(int device_id)
    : device_id_(device_id)
    , owns_reference_(true)
{
}

/**
 * @brief 释放本会话持有的运行时引用。
 */
AclRuntimeSession::~AclRuntimeSession()
{
    release();
}

/**
 * @brief 移动构造运行时会话。
 * @param other 被移动的会话对象。
 */
AclRuntimeSession::AclRuntimeSession(AclRuntimeSession&& other) noexcept
    : device_id_(other.device_id_)
    , owns_reference_(other.owns_reference_)
{
    other.owns_reference_ = false;
}

/**
 * @brief 移动赋值运行时会话。
 * @param other 被移动的会话对象。
 * @return 当前对象引用。
 */
AclRuntimeSession& AclRuntimeSession::operator=(AclRuntimeSession&& other) noexcept
{
    if (this != &other) {
        release();
        device_id_ = other.device_id_;
        owns_reference_ = other.owns_reference_;
        other.owns_reference_ = false;
    }
    return *this;
}

/**
 * @brief 获取一个 AscendCL 运行时会话。
 * @param device_id Ascend 设备编号。
 * @param error 失败时写入可读错误文本。
 * @return 成功返回会话对象；失败返回空指针。
 */
std::unique_ptr<AclRuntimeSession> AclRuntimeSession::acquire(int device_id, std::string& error)
{
    auto& state = runtime_state();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (state.ref_count > 0) {
        if (state.device_id != device_id) {
            error = "AscendCL runtime already uses device " + std::to_string(state.device_id) +
                    ", requested device " + std::to_string(device_id);
            return nullptr;
        }
        ++state.ref_count;
        error.clear();
        return std::unique_ptr<AclRuntimeSession>(new AclRuntimeSession(device_id));
    }

    auto ret = aclInit(nullptr);
    if (!acl_ok(ret)) {
        error = make_acl_error("aclInit", static_cast<int>(ret));
        return nullptr;
    }
    state.acl_initialized = true;

    ret = aclrtSetDevice(device_id);
    if (!acl_ok(ret)) {
        error = make_acl_error("aclrtSetDevice", static_cast<int>(ret));
        aclFinalize();
        state.acl_initialized = false;
        return nullptr;
    }
    state.device_set = true;

    ret = aclrtCreateContext(&state.context, device_id);
    if (!acl_ok(ret)) {
        error = make_acl_error("aclrtCreateContext", static_cast<int>(ret));
        aclrtResetDevice(device_id);
        aclFinalize();
        state.device_set = false;
        state.acl_initialized = false;
        return nullptr;
    }

    state.device_id = device_id;
    state.ref_count = 1;
    error.clear();
    return std::unique_ptr<AclRuntimeSession>(new AclRuntimeSession(device_id));
}

/**
 * @brief 返回当前会话绑定的设备编号。
 * @return Ascend 设备编号。
 */
int AclRuntimeSession::device_id() const noexcept
{
    return device_id_;
}

/**
 * @brief 释放当前对象持有的引用。
 */
void AclRuntimeSession::release() noexcept
{
    if (!owns_reference_) {
        return;
    }

    auto& state = runtime_state();
    std::lock_guard<std::mutex> lock(state.mutex);
    owns_reference_ = false;

    if (state.ref_count <= 0) {
        return;
    }

    --state.ref_count;
    if (state.ref_count > 0) {
        return;
    }

    if (state.context != nullptr) {
        aclrtDestroyContext(state.context);
        state.context = nullptr;
    }
    if (state.device_set) {
        aclrtResetDevice(state.device_id);
        state.device_set = false;
    }
    if (state.acl_initialized) {
        aclFinalize();
        state.acl_initialized = false;
    }

    state.device_id = -1;
}

} // namespace sentinel
