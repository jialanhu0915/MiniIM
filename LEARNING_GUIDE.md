# 即时通讯项目 — 学习路线

## 当前进度

```
里程碑 1 (理解现状 + 协议)   ████████████ ✅ 完成
里程碑 2 (多客户端)          ████████████ ✅ 完成
里程碑 3 (登录/登出)         ████████████ ✅ 完成
里程碑 4 (好友管理)          ░░░░░░░░░░░░  进行中
里程碑 5 (单聊消息)          ░░░░░░░░░░░░  未开始
里程碑 6 (群聊消息)          ░░░░░░░░░░░░  未开始
里程碑 7 (离线消息/聊天记录)  ░░░░░░░░░░░░  未开始
里程碑 8 (文件传输)          ░░░░░░░░░░░░  未开始
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
