#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace sentinel {

/**
 * @brief AscendCL 运行时引用计数会话。
 *
 * 该类统一管理进程内 `aclInit`、`aclrtSetDevice`、Context 创建和最终释放，
 * 避免 DVPP 预处理和 AscendCL 推理各自初始化/释放 ACL 运行时导致状态冲突。
 * 当前实现只允许同一进程绑定一个 Device，适合本项目单路摄像头学习阶段。
 */
class AclRuntimeSession {
public:
    /**
     * @brief 释放本会话持有的运行时引用。
     */
    ~AclRuntimeSession();

    AclRuntimeSession(const AclRuntimeSession&) = delete;
    AclRuntimeSession& operator=(const AclRuntimeSession&) = delete;

    AclRuntimeSession(AclRuntimeSession&&) noexcept;
    AclRuntimeSession& operator=(AclRuntimeSession&&) noexcept;

    /**
     * @brief 获取一个 AscendCL 运行时会话。
     * @param device_id Ascend 设备编号。
     * @param error 失败时写入可读错误文本。
     * @return 成功返回会话对象；失败返回空指针。
     */
    static std::unique_ptr<AclRuntimeSession> acquire(int device_id, std::string& error);

    /**
     * @brief 返回当前会话绑定的设备编号。
     * @return Ascend 设备编号。
     */
    int device_id() const noexcept;

private:
    /**
     * @brief 保存设备编号并标记该对象持有一个引用。
     * @param device_id Ascend 设备编号。
     */
    explicit AclRuntimeSession(int device_id);

    /**
     * @brief 释放当前对象持有的引用。
     */
    void release() noexcept;

    int device_id_{0};
    bool owns_reference_{false};
};

/**
 * @brief 构造 AscendCL 错误上下文文本。
 * @param action 失败的 AscendCL API 名称。
 * @param error AscendCL 返回码。
 * @return 可读错误文本。
 */
std::string make_acl_error(std::string_view action, int error);

} // namespace sentinel
