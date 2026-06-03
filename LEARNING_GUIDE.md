# 即时通讯项目 — 学习路线

## 当前进度

```
里程碑 1 (理解现状 + 协议)   ████████████ ✅ 完成
里程碑 2 (多客户端)          ████████████ ✅ 完成
里程碑 3 (登录/登出)         ████████████ ✅ 完成
里程碑 4 (好友管理)          ████████████ ✅ 完成
里程碑 5 (单聊消息)          ████████████ ✅ 完成
里程碑 6 (群聊消息)          ████████████ ✅ 完成
里程碑 7 (离线消息/聊天记录)  ████████████ ✅ 完成
里程碑 8 (文件传输)          ████████████ ✅ 完成
```

---

## 里程碑 4：好友管理

### 4.1 客户端 — 发送 FRIEND_ADD

用户在输入框输入 `/addfriend <用户名>`，点击"添加好友"按钮。

`OnBnClickedButtonAddFriend` 已搭好框架，缺的是构造 JSON 并发送：

```cpp
std::string json = JsonMakeObject({
    JsonSetInt("user_id", m_userId),
    JsonSetString("friend_username", friendName)
});
auto data = ProtocolEncode(MsgType::FRIEND_ADD, json);
m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
```

### 4.2 服务端 — FRIEND_ADD handler

1. 用 `m_dbWrapper.iGetUserId(friendName)` 查对方 ID
2. 不存在 → 返回 ACK 失败
3. 存在 → `m_dbWrapper.bAddFriend(userId, friendId)`
4. 对方在线 → 通知对方更新好友列表

### 4.3 验证

- 启动两个客户端 Alice 和 Bob
- Alice 输入 `/addfriend Bob` 点添加好友
- Bob 收到好友更新 → 列表出现 Alice

---

## 里程碑 5：单聊消息

### 5.1 客户端 — 发送 TEXT

`OnBnClickedButtonSend` 中根据 `m_selectedFriendId` 构造消息（需要先补 `LBN_SELCHANGE` 处理）。

### 5.2 服务端 — TEXT handler

解析 sender_id / receiver_id / content → 存 DB → 在线转发 / 离线存储。

### 5.3 客户端 — TEXT handler

解析 sender_id / content → 调用 `OnMessageReceived(name, content, "")`。

---

## 里程碑 6：群聊消息

### 6.1 客户端 — 选中"全体"时发送 GROUP_TEXT

`m_selectedFriendId == 0` → 发送 `MsgType::GROUP_TEXT`。

### 6.2 服务端 — GROUP_TEXT handler

遍历 `m_userIdToSocket`，广播给所有在线用户（不发给自己）。

---

## 里程碑 7：离线消息 + 聊天记录

- 登录时推送离线消息（`OFFLINE_MSGS`）
- 支持 `HISTORY` 查询历史记录

---

## 里程碑 8：文件传输

- FTP 上传 → 发送 `FILE_REQUEST` 通知对方
- 对方收到 → 从 FTP 下载

---

---

# 附录：关键代码模板

## 服务端 handler 模板

```cpp
m_dispatcher.on(MsgType::XXXX, [this](const std::string& json) {
    // 1. 解析 JSON
    // 2. 查 DB
    // 3. 回复 / 转发 / 广播
});
```

## 客户端 handler 模板

```cpp
m_dispatcher.on(MsgType::XXXX, [this](const std::string& json) {
    // 1. 解析 JSON
    // 2. 调用对应的 UI 回调
});
```

## 发消息模板

```cpp
std::string json = JsonMakeObject({
    JsonSetInt("key1", intVal),
    JsonSetString("key2", strVal)
});
auto data = ProtocolEncode(MsgType::XXXX, json);
socket.Send(data.data(), static_cast<int>(data.size()));
```

## JSON 工具（Common/JsonUtils.h）

| 函数 | 作用 |
|------|------|
| `JsonGetString(json, key)` | 从 JSON 取字符串值 |
| `JsonGetInt(json, key)` | 从 JSON 取整数值 |
| `JsonSetString(key, val)` | 构造 `"key":"val"` |
| `JsonSetInt(key, val)` | 构造 `"key":123` |
| `JsonMakeObject({...})` | 拼成 `{...}` |

## 核心概念参考 → `KNOWLEDGE_BASE.md`

TCP 粘包/半包、字节序、HostToNet32、CAsyncSocket 异步模型、RecvBuffer 源码分析、msgType 协议表。

---

## 扩展方向（已完成 8 个里程碑后可选的下一步）

下面列出可继续深入的方向，按"实现难度 / 教学价值"排序。每个方向只点出主题，不展开实现 —— 跟里程碑 1-8 一样，按你自己的节奏探索。

### 协议与可靠性

- **TLS/SSL 加密** — 当前 IM 通信是明文 TCP，传输层加 TLS 即可在不改业务代码的情况下解决中间人窃听
- **心跳保活** — 客户端定时发 PING，服务端回 PONG；超时则主动断开。替代依赖 TCP keepalive
- **消息已读回执** — 给 TEXT 协议加一个 `READ_ACK` 类型，接收方读完消息后回执
- **消息撤回** — 新增 `REVOKE` 消息类型，服务端广播撤回
- **多端同步** — 同一账号多点登录时消息互相同步

### 文件传输

- **断点续传** — FTP 加 `REST` 命令支持，传输中断后可从断点继续
- **大文件分块 + 进度条** — `FtpHelper` 加进度回调，UI 显示百分比
- **P2P 直连** — 当前是"上传到服务端 FTP → 接收方下载"，可改为发送方和接收方直接 STUN/TURN 打洞直连
- **图片/视频预览** — 缩略图生成 + 消息体携带缩略图

### 群组与社交

- **群组成员管理** — 当前 `receiver_id=0` 是"全体"，可改为 `GROUP_TEXT { group_id }` + `GROUP_CREATE/JOIN/LEAVE` 协议
- **好友备注名** — FRIEND 表加 `alias` 字段，UI 优先显示备注
- **黑名单** — 屏蔽某用户的消息

### 性能与服务端

- **线程池** — 当前每连接一个 `CWinThread`，高并发下改用 `IOCP` 或 `epoll` 模拟
- **优雅退出** — `OnBnClickedButtonStop` 依赖 5 秒 `WaitForSingleObject` 超时，可改成"通知所有线程主动退出 + join"
- **DB 索引** — `messages` 表按 `(sender_id, receiver_id, timestamp)` 加索引加速历史查询
- **DDoS 防护** — 同一 IP 连接频率限制 + 黑名单

### 安全

- **密码哈希** — 当前 USER/PASS 是明文（教学项目无所谓，生产环境必须 bcrypt/argon2）
- **SQL 注入** — `SQLiteWrapper` 用参数化查询即可，当前已是
- **文件类型校验** — 上传时校验 magic number，防止伪装成图片的 .exe

每个方向都是一个独立的"里程碑 9+"，建议按兴趣挑 1-2 个深入做。

