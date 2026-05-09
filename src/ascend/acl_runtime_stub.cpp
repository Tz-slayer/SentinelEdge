#include "sentinel/ascend/acl_runtime.hpp"

namespace sentinel {

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
{
}

/**
 * @brief 释放空实现会话。
 */
AclRuntimeSession::~AclRuntimeSession() = default;

/**
 * @brief 移动构造空实现会话。
 * @param other 被移动的会话对象。
 */
AclRuntimeSession::AclRuntimeSession(AclRuntimeSession&& other) noexcept
    : device_id_(other.device_id_)
    , owns_reference_(other.owns_reference_)
{
    other.owns_reference_ = false;
}

/**
 * @brief 移动赋值空实现会话。
 * @param other 被移动的会话对象。
 * @return 当前对象引用。
 */
AclRuntimeSession& AclRuntimeSession::operator=(AclRuntimeSession&& other) noexcept
{
    if (this != &other) {
        device_id_ = other.device_id_;
        owns_reference_ = other.owns_reference_;
        other.owns_reference_ = false;
    }
    return *this;
}

/**
 * @brief 报告当前构建未启用 AscendCL。
 * @param device_id Ascend 设备编号。
 * @param error 失败时写入可读错误文本。
 * @return 固定返回空指针。
 */
std::unique_ptr<AclRuntimeSession> AclRuntimeSession::acquire(int device_id, std::string& error)
{
    static_cast<void>(device_id);
    error = "AscendCL runtime is not enabled at build time";
    return nullptr;
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
 * @brief 空实现无需释放任何资源。
 */
void AclRuntimeSession::release() noexcept
{
}

} // namespace sentinel
