/**
 * @file   Protocol.h
 * @brief  即时通讯应用层协议定义（Windows + Winsock）
 * @author Yan Runxin
 * @date   2026-05-25
 *
 * ## 消息格式（二进制头部 + JSON 消息体）
 *
 *   [0..3]  uint32_t  消息类型（大端/网络字节序）
 *   [4..7]  uint32_t  消息体长度（大端/网络字节序），不含头部 8 字节
 *   [8..n]  uint8_t[] 消息体（UTF-8 编码的 JSON）
 *
 * ## 为什么这样设计
 *   1. TCP 是字节流，必须有长度字段来做消息"分帧"
 *   2. 网络字节序（大端）是跨平台通信的标准
 *   3. JSON 便于调试和扩展，适合学习阶段
 *
 * ## 网络层你需要实现的收发逻辑
 *
 *   // —— 发送 ——
 *   std::string json = R"({"username":"Alice"})";
 *   auto data = ProtocolEncode(MsgType::LOGIN, json);
 *   m_socket.Send(data.data(), data.size());
 *
 *   // —— 接收（在 OnReceive 回调里） ——
 *   char tmp[4096];
 *   int n = m_socket.Receive(tmp, sizeof(tmp));
 *   m_recvBuf.append(tmp, n);
 *
 *   uint32_t type, length;  std::string json;
 *   while (m_recvBuf.next(type, length, json)) {
 *       m_dispatcher.dispatch(static_cast<MsgType>(type), json);
 *   }
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// 可移植的 32 位主机序↔网络序转换（不依赖 winsock2.h / arpa/inet.h）
// ============================================================================
inline uint32_t HostToNet32(uint32_t host32) {
    return ((host32 & 0x000000FF) << 24) |
           ((host32 & 0x0000FF00) << 8)  |
           ((host32 & 0x00FF0000) >> 8)  |
           ((host32 & 0xFF000000) >> 24);
}
inline uint32_t NetToHost32(uint32_t net32) {
    return HostToNet32(net32);  // 32 位大端↔小端，两次操作回到原值
}

// ============================================================================
// 消息类型枚举
// ============================================================================
enum class MsgType : uint32_t
{
    // ---- 登录/登出 ----
    LOGIN           = 0x0001,   // 客户端→服务端：{"username":"..."}
    LOGIN_RESP      = 0x0002,   // 服务端→客户端：{"success":bool,"user_id":int,"reason":"","friends":[...],"offline_msgs":[...]}
    LOGOUT          = 0x0003,   // 客户端→服务端：{"user_id":int}

    // ---- 即时消息 ----
    TEXT            = 0x0010,   // 客户端↔服务端→客户端：{"sender_id":int,"receiver_id":int,"content":"..."}
    GROUP_TEXT      = 0x0011,   // 客户端→服务端→多个客户端：receiver_id=0 表示全体

    // ---- 文件传输 ----
    FILE_REQUEST    = 0x0020,   // 客户端→服务端→客户端：{"sender_id":int,"receiver_id":int,"filename":"","filesize":int,"ftp_path":""}
    FILE_RESP       = 0x0021,   // 客户端→服务端→客户端：{"sender_id":int,"receiver_id":int,"file_id":int,"accepted":bool}

    // ---- 好友管理 ----
    FRIEND_ADD      = 0x0030,   // 客户端→服务端：{"user_id":int,"friend_username":"..."}
    FRIEND_DEL      = 0x0031,   // 客户端→服务端：{"user_id":int,"friend_id":int}
    FRIEND_LIST     = 0x0032,   // 客户端→服务端→客户端：请求 {"user_id":int}，响应 {"friends":[...]}

    // ---- 状态通知 ----
    STATUS_ONLINE   = 0x0040,   // 服务端→客户端：{"user_id":int,"username":"..."}
    STATUS_OFFLINE  = 0x0041,   // 服务端→客户端：{"user_id":int,"username":"..."}

    // ---- 消息管理 ----
    HISTORY         = 0x0050,   // 客户端→服务端→客户端：请求 {"user_id":int,"other_user_id":int,"limit":int}
    OFFLINE_MSGS    = 0x0051,   // 服务端→客户端（登录后推送）：{"messages":[...]}
    OFFLINE_READ    = 0x0052,   // 客户端→服务端：{"user_id":int,"msg_ids":[int,...]}

    // ---- 通用 ----
    ACK             = 0xFFFF    // 通用确认：{"original_type":int,"success":bool,"reason":""}
};

// ============================================================================
// 协议常量
// ============================================================================
constexpr size_t   HEADER_SIZE   = 8;            // type(4) + length(4)
constexpr uint32_t MAX_MSG_SIZE  = 10 * 1024 * 1024;  // 10 MB 上限（防止恶意数据）

// ============================================================================
// 编码 / 解码
// ============================================================================

/**
 * @brief  将消息类型和 JSON 负载编码为网络字节流
 * @return 完整的消息字节数组（可直接传给 send）
 */
inline std::vector<uint8_t> ProtocolEncode(MsgType type, const std::string& json) {
    const uint32_t typeVal   = static_cast<uint32_t>(type);
    const uint32_t length    = static_cast<uint32_t>(json.size());
    const uint32_t typeNet   = HostToNet32(typeVal);
    const uint32_t lengthNet = HostToNet32(length);

    std::vector<uint8_t> buf(HEADER_SIZE + length);
    memcpy(buf.data(),       &typeNet,   4);
    memcpy(buf.data() + 4,   &lengthNet, 4);
    memcpy(buf.data() + 8,   json.data(), length);
    return buf;
}

/**
 * @brief  从 8 字节头部解析消息类型和长度
 * @return 成功返回 true
 */
inline bool ProtocolDecodeHeader(const uint8_t* header,
                                  uint32_t& outType,
                                  uint32_t& outLength) {
    if (!header) return false;

    uint32_t typeNet, lengthNet;
    memcpy(&typeNet,   header,     4);
    memcpy(&lengthNet, header + 4, 4);

    outType   = NetToHost32(typeNet);
    outLength = NetToHost32(lengthNet);
    return true;
}

// ============================================================================
// RecvBuffer — 接收缓冲区，处理 TCP 粘包 / 半包
// ============================================================================

/**
 * @brief 从字节流中切出完整消息
 *
 * TCP 是字节流，一次 recv 可能：
 * - 收到半个消息（半包）→ 需要继续等待
 * - 收到一个半消息（粘包）→ 需要切出第一个，保留剩余的
 *
 * RecvBuffer 帮你处理这一切。
 *
 * 用法：
 *   RecvBuffer m_rbuf;
 *   // 在 OnReceive 里：
 *   char tmp[4096];
 *   int n = sock.Receive(tmp, sizeof(tmp));
 *   m_rbuf.append(tmp, n);
 *   uint32_t type, length;  std::string json;
 *   while (m_rbuf.next(type, length, json)) {
 *       // 完整消息到手 → 分发
 *   }
 */
class RecvBuffer {
public:
    void append(const char* data, size_t len) {
        m_buf.insert(m_buf.end(), data, data + len);
    }

    /**
     * @brief  尝试取出一个完整消息
     * @return true 表示成功取出，false 表示数据不够或出错
     */
    bool next(uint32_t& outType, uint32_t& outLength, std::string& outJson) {
        if (m_buf.size() < HEADER_SIZE) return false;

        uint32_t typeNet, lengthNet;
        memcpy(&typeNet,   m_buf.data(),     4);
        memcpy(&lengthNet, m_buf.data() + 4, 4);

        const uint32_t type   = NetToHost32(typeNet);
        const uint32_t length = NetToHost32(lengthNet);

        if (length > MAX_MSG_SIZE) {
            m_buf.clear();  // 异常数据，清空缓冲区
            return false;
        }

        if (m_buf.size() < HEADER_SIZE + length) return false;  // 数据不够

        outType   = type;
        outLength = length;
        outJson.assign(reinterpret_cast<const char*>(m_buf.data() + HEADER_SIZE),
                       length);

        // 从缓冲区移除已取出的消息
        m_buf.erase(m_buf.begin(), m_buf.begin() + HEADER_SIZE + length);
        return true;
    }

    size_t pending() const { return m_buf.size(); }
    void clear() { m_buf.clear(); }

private:
    std::vector<uint8_t> m_buf;
};

// ============================================================================
// ProtocolDispatcher — 消息分发器
// ============================================================================

/**
 * @brief 把网络层收到的消息路由到业务处理函数
 *
 * 用法：
 *   ProtocolDispatcher m_disp;
 *   // 初始化时注册：
 *   m_disp.on(MsgType::TEXT, [this](const std::string& json) { ... });
 *   // 收到消息时调用：
 *   m_disp.dispatch(type, json);
 */
class ProtocolDispatcher {
public:
    struct MsgTypeHash {
        size_t operator()(MsgType t) const noexcept {
            return static_cast<size_t>(t);
        }
    };

    using Handler = std::function<void(const std::string& json)>;

    void on(MsgType type, Handler handler) {
        m_handlers[type] = std::move(handler);
    }

    bool dispatch(MsgType type, const std::string& json) const {
        auto it = m_handlers.find(type);
        if (it != m_handlers.end()) {
            it->second(json);
            return true;
        }
        return false;
    }

private:
    std::unordered_map<MsgType, Handler, MsgTypeHash> m_handlers;
};

