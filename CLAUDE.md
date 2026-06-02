# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 代码注释规范（全项目统一）

所有 C++ 源文件必须使用 **Doxygen 风格注释**（`/** */`），覆盖：

- 所有 `public` 方法（含文件作用域自由函数）
- 关键 `private` / `protected` 方法
- 类 / 结构体定义（含 `brief` 说明）
- 重要成员变量（含用途说明）

### 格式模板

```cpp
/**
 * @brief  简要说明这个函数做什么
 * @param  param1  参数1的用途（标注 [in]/[out]/[in,out]）
 * @param  param2  参数2的用途
 * @return 返回值说明（void 时省略）
 * @note   可选：注意事项或特殊行为
 */
void exampleFunction(int param1, const std::string& param2);
```

### 示例（正确 vs 错误）

```cpp
// WRONG -- natural language single-line comment, lacks structure
// This is the function to send messages, need to check if user is logged in first

// CORRECT -- Doxygen style
/**
 * @brief  Send a text message to the selected friend or group chat
 * @param  strMsg  Message content (UTF-8), timestamp constructed by sender
 * @return void
 * @note   When receiver_id=0 sends GROUP_TEXT, otherwise sends TEXT
 */
```

### 注释语言

优先使用**英文**，保持国际化和 AI 可读性。已有中文注释的源文件（如 NetworkServerDlg.cpp）保留中文，不强制迁移。

## 项目概述

即时通讯（IM）客户端/服务端学习项目，基于 MFC + Windows Socket。教学用，含好友管理、单聊、群聊、文件传输模块。

```
NetworkServer/   服务端（MFC 对话框程序）
NetworkClient/   客户端（MFC 对话框程序）
Common/          共享：Protocol.h（协议）、JsonUtils.h（JSON工具）
```

## 构建

使用 Visual Studio 打开 `ComputerNetworkProgramming.sln`，选择 x64/Debug 或 x64/Release 构建。

```
# 命令行构建（PowerShell / Developer Command Prompt）
msbuild ComputerNetworkProgramming.sln /p:Configuration=Debug /p:Platform=x64
```

## 协议架构

### 消息格式（二进制头部 + UTF-8 JSON）

```
[0..3]   uint32_t  消息类型（大端序）
[4..7]   uint32_t  消息体长度（大端序，不含头部 8 字节）
[8..n]   uint8_t[] UTF-8 JSON 消息体
```

### 消息类型（Protocol.h）

| 方向 | 类型 | 说明 |
|------|------|------|
| C→S | LOGIN | 登录 `{"username":"..."}` |
| S→C | LOGIN_RESP | 响应含 user_id、friends、offline_msgs |
| C↔S | TEXT | 单聊 `{"sender_id", "receiver_id", "content", "timestamp"}` |
| C↔S | GROUP_TEXT | 群聊（receiver_id=0 表示全体） |
| C→S | FRIEND_ADD / FRIEND_DEL / FRIEND_ACCEPT / FRIEND_REJECT | 好友操作 |
| S→C | FRIEND_ADD_RESP | 好友请求响应 `{"success":"true"/"waiting"/"false",...}` |
| S→C | STATUS_ONLINE / STATUS_OFFLINE | 好友上下线通知 |
| C→S | HISTORY | 查询历史 `{"user_id", "other_user_id", "limit"}` |
| S→C | OFFLINE_MSGS | 登录时推送离线消息 |
| C↔S | FILE_REQUEST / FILE_RESP | 文件传输（FTP） |

### 收发模式（RecvBuffer + ProtocolDispatcher）

```cpp
// 接收 -- RecvBuffer 自动处理粘包/半包
char buf[4096];
int n = socket.Receive(buf, sizeof(buf));
m_recvBuf.append(buf, n);
uint32_t type, length; std::string json;
while (m_recvBuf.next(type, length, json)) {
    m_dispatcher.dispatch(static_cast<MsgType>(type), json);
}

// 发送
std::string json = JsonMakeObject({JsonSetInt("id", 1), ...});
auto data = ProtocolEncode(MsgType::TEXT, json);
socket.Send(data.data(), static_cast<int>(data.size()));
```

### JSON 工具（JsonUtils.h）

`JsonGetString(json, key)` / `JsonGetInt(json, key)` / `JsonSetString(key, val)` / `JsonSetInt(key, val)` / `JsonMakeObject({...})` / `JsonGetArray(json, key)` -- 手写实现，无外部依赖。

## 服务端架构（BSD Socket + AfxBeginThread 多线程）

```
主线程（UI）
├─ ListenThread: 阻塞 accept() → 每 accept → new CConnectSocket → ClientThread
├─ ClientThread1: 阻塞 recv() → RecvBuffer → dispatch
├─ ClientThread2: 阻塞 recv() → RecvBuffer → dispatch
└─ ...

共享数据用 CCriticalSection m_csData 保护
UI 更新通过 PostMessage(WM_USER_UPDATE_UI) 投递到 UI 线程
```

`NetworkServerDlg` 持有：
- `m_listenSocket`（CListenSocket）-- 监听 socket + accept 线程
- `m_connectSockets` -- 当前所有客户端连接
- `m_userIdToSocket` / `m_socketToUserId` -- userId ↔ socket 映射
- `m_userIdToName` -- userId → 用户名
- `m_dbWrapper`（SQLiteWrapper）-- DB 操作

每客户端对应一个 `CConnectSocket` 实例 + 一个 `CWinThread` 线程，线程在 `RecvLoop` 中自行 `delete this`。

thread-local 变量 `g_pCurrentSocket`（文件作用域）传递当前处理的 socket 上下文给 handler。

### ⚠️ 两把锁注意

服务端存在**两把独立的锁**，各自保护不同资源：

- `m_csData`（`NetworkServerDlg`）-- 保护内存 map / socket 列表
- `m_csDb`（`SQLiteWrapper`）-- 保护 sqlite3 句柄

**规则：持有 `m_csData` 期间绝不调用 `m_dbWrapper.*`**（会导致嵌套锁死锁）。正确模式：

```cpp
// 错误（嵌套锁风险）
{
    CSingleLock lock(&m_csData, TRUE);
    m_dbWrapper.bSaveMessage(...);  // ← m_csDb 在内部又被申请，死锁
}

// 正确
m_dbWrapper.bSaveMessage(...);  // ← m_csDb 独立加锁，不影响 m_csData
{
    CSingleLock lock(&m_csData, TRUE);
    m_userIdToSocket[id] = pSocket;
}
```

SQLiteWrapper 所有方法已按 `Xxx()` → `XxxImpl()` 模式拆分，公有方法内部加 `m_csDb` 锁，_impl 方法不加锁供内部调用。

## 客户端架构

```
UI 线程 ── CAsyncSocket 异步回调 ── RecvBuffer ── ProtocolDispatcher ── handler 回调
```

`CNetworkClientDlg` 持有：
- `m_connectSocket`（CAsyncSocket）-- 到服务端的连接
- `m_recvBuf`（RecvBuffer）-- 接收缓冲区
- `m_dispatcher`（ProtocolDispatcher）-- 消息分发器
- `m_friendMap`（unordered_map）-- userId → FriendInfo
- `m_userId` / `m_username` -- 登录状态
- `m_selectedFriendId` -- 当前选中的好友（0 = 全体群聊）
- `m_bConnecting` -- 防止重复点击连接

## 关键坑点

### WSL 写入 Windows 文件系统的编码（高频踩坑，必读）

在 `/mnt/b/`（Windows NTFS 分区）下编辑文件时：
- WSL 默认写入 **UTF-8 without BOM + LF**
- MSVC 期望 **UTF-8 with BOM + CRLF**（MFC 头文件尤其严格）
- 含中文注释的文件**只要缺 BOM** 就会出现 C4819 警告甚至「类定义解析失败」级联报 100+ 错误

**所有 .h/.cpp 必须满足**：
- 首三字节为 `EF BB BF`（UTF-8 BOM）
- 行尾为 `CRLF`（`0D 0A`），不是 LF

**批量修复命令**（WSL 端，Python 3）：

```python
import os
BOM = b'\xef\xbb\xbf'
files = [
    'Common/JsonUtils.h', 'Common/Protocol.h', 'Common/FtpHelper.h',
    'NetworkClient/NetworkClient.h', 'NetworkClient/NetworkClient.cpp',
    'NetworkClient/NetworkClientDlg.h', 'NetworkClient/NetworkClientDlg.cpp',
    'NetworkClient/CConnectSocket.h', 'NetworkClient/CConnectSocket.cpp',
    'NetworkClient/CAddFriendDlg.h', 'NetworkClient/CAddFriendDlg.cpp',
    'NetworkServer/NetworkServer.h', 'NetworkServer/NetworkServer.cpp',
    'NetworkServer/NetworkServerDlg.h', 'NetworkServer/NetworkServerDlg.cpp',
    'NetworkServer/SQLiteWrapper.h', 'NetworkServer/SQLiteWrapper.cpp',
    'NetworkServer/CConnectSocket.h', 'NetworkServer/CConnectSocket.cpp',
    'NetworkServer/CListenSocket.h', 'NetworkServer/CListenSocket.cpp',
]
for p in files:
    with open(p, 'rb') as f: b = f.read()
    if b.startswith(BOM): b = b[3:]
    b = b.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    with open(p, 'wb') as f: f.write(BOM + b)
```

**验证**：`file <path>` 应输出 `UTF-8 (with BOM) text, with CRLF line terminators`。

**高危字符**：em-dash（`--`，U+2014，UTF-8 `E2 80 94`）和 en-dash（`-`）在 GBK 解析下会错位，后续 ASCII 标识符全部解析错误。**禁止在代码注释/字符串字面量中使用**。改用 ` - `、` -- ` 或中文标点「、」「（」「）」。

**经验教训**：补 Doxygen 时若 agent 一次改太多文件，最容易出问题的是 BOM/CRLF ---- **每次写完中文注释的 .h 都要立即 `file` 检查**。

### 服务端 JSON 构建（好友列表格式）

`JsonSetString/JsonSetInt` 返回**裸片段**，不含外层 `{}`：

```cpp
// 错误 -- 每个字段没有外层括号
result = JsonSetInt("user_id", 2) + "," + JsonSetString("username", "Bob");
// -> "user_id":2,"username":"Bob"  （缺少 {}）

// 正确 -- 用 JsonMakeObject 包裹
result = JsonMakeObject({
    JsonSetInt("user_id", 2),
    JsonSetString("username", "Bob"),
    JsonSetString("online", "false")
});
// -> {"user_id":2,"username":"Bob","online":"false"}
```

## 数据库

SQLite，`chat_history.db`（自动创建）。表结构由 `SQLiteWrapper::bOpenDb` 初始化：

- `users` -- id, username
- `messages` -- id, sender_id, receiver_id, content, timestamp, is_read
- `friends` -- user_id, friend_id, created_at
- `file_records` -- id, sender_id, receiver_id, filename, filesize, filepath, downloaded
- `offline_messages` -- id, sender_id, receiver_id, content, created_at, is_read

所有 DB 操作经 `SQLiteWrapper`，内部加 `m_csDb` 锁保护 sqlite3 句柄。

## 学习资料

- `LEARNING_GUIDE.md` -- 分里程碑学习路线（里程碑 4 已完成）
- `KNOWLEDGE_BASE.md` -- TCP 分帧、粘包/半包、字节序、CAsyncSocket 异步模型详解