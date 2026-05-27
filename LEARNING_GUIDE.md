# 即时通讯项目 — 分阶段学习与实现路线

## 当前状态

基础设施已完成：UI 对话框、SQLite 数据库封装、应用层协议（Protocol.h）、FTP 封装（FtpHelper）。
**网络通信代码（socket 收发）由你来写**，这是本计划的核心。

现有代码的关键约束：
- 服务端只有一个 `m_connectSocket`，只能同时服务**一个客户端**
- 客户端与服务端的消息收发目前只处理**原始字节**，没有协议封包
- 数据库已就绪但网络层完全没有调用

---

## 总体路线图

```
里程碑 1:  理解现状 + 单连接走通协议      [1天]
里程碑 2:  服务端支持多客户端              [2天]
里程碑 3:  登录 / 登出流程                 [2天]
里程碑 4:  好友管理（添加/删除/列表）       [1天]
里程碑 5:  单聊消息（在线转发）            [2天]
里程碑 6:  群聊消息（广播）                [1天]
里程碑 7:  离线消息 + 聊天记录查询          [2天]
里程碑 8:  文件传输（FTP 集成）             [2天]
```

---

## 里程碑 1：理解现状 + 单连接走通协议

### 1.1 你要先搞懂的概念

#### 为什么 TCP 需要"消息分帧"

TCP 是**字节流**协议，不是**消息流**协议。它只保证字节按顺序到达，不保证每次 `recv` 的边界和 `send` 的边界一致：

```
发送方调用：  send("ABC")  send("DEF")  send("GHI")
              |            |            |
网络传输：    ....ABCDEFGHI............
              |        |          |
接收方可能：  recv→"ABCD" recv→"EFG" recv→"HI"
```

这就是所谓的**粘包**（多条消息粘在一次 recv 里）和**半包**（一条消息分多次 recv 才收完）。

解决方案：在每条消息前面加上**长度字段**。接收方先读长度，再按长度读正文。Protocol.h 里的 `[4B type][4B length][N B body]` 就是这个设计。

#### 网络字节序

不同 CPU 架构存储多字节整数的顺序不同：
- **小端**（x86/x64）：低位字节在前。`0x12345678` 存为 `78 56 34 12`
- **大端**（网络字节序）：高位字节在前。`0x12345678` 存为 `12 34 56 78`

网络协议统一用大端。Protocol.h 里的 `HostToNet32()` / `NetToHost32()` 就是做这个转换。

#### CAsyncSocket 的异步模型

`CAsyncSocket` 是**非阻塞**的：
- `OnReceive` 回调触发时，数据可能只到了部分
- `Receive` 返回的字节数不确定（可能 1、可能 4096）
- 你必须把每次收到的数据**累积**到缓冲区，然后检查是否凑够一条完整消息

### 1.2 要写的代码

#### 客户端 — OnReceive（替换现在的裸字节版）

当前代码（NetworkClientDlg.cpp 约第 330 行）：
```cpp
void CNetworkClientDlg::OnReceive() {
    char szBuf[1024] = { 0 };
    int nRead = m_connectSocket.Receive(szBuf, sizeof(szBuf) - 1);
    if (nRead > 0) {
        szBuf[nRead] = '\0';
        UpdateLog(_T("[收到] ") + CString(szBuf));  // 直接当字符串显示，不对！
    }
}
```

替换为使用 Protocol.h 的 RecvBuffer：
```cpp
void CNetworkClientDlg::OnReceive() {
    char buf[4096];
    int n = m_connectSocket.Receive(buf, sizeof(buf));
    if (n > 0) {
        m_recvBuf.append(buf, n);           // ① 累积到缓冲区
        uint32_t type, length;
        std::string json;
        while (m_recvBuf.next(type, length, json)) {  // ② 循环切出完整消息
            m_dispatcher.dispatch(static_cast<MsgType>(type), json);  // ③ 路由
        }
    }
}
```

#### 客户端 — OnConnect（连接成功后发送 LOGIN）

当前代码（NetworkClientDlg.cpp 约第 310 行）：
```cpp
void CNetworkClientDlg::OnConnect() {
    SetDlgItemText(IDC_STATIC_STATUS, _T("已连接"));
    GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(TRUE);
    UpdateLog(_T("[连接] TCP 连接成功"));
}
```

加上登录消息发送：
```cpp
void CNetworkClientDlg::OnConnect() {
    UpdateStatus(_T("已连接"));
    GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(TRUE);
    UpdateLog(_T("[连接] TCP 连接成功"));

    // 发送登录消息（用 Protocol.h 的编码函数）
    std::string json = "{\"username\":\"" + m_username + "\"}";
    auto data = ProtocolEncode(MsgType::LOGIN, json);
    m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
}
```

#### 服务端 — OnReceive（同样的模式）

当前代码（NetworkServerDlg.cpp 约第 280 行）：
```cpp
void CNetworkServerDlg::OnReceive() {
    char szBuf[1024] = { 0 };
    int nRead = m_connectSocket.Receive(szBuf, sizeof(szBuf) - 1);
    if (nRead > 0) {
        szBuf[nRead] = '\0';
        UpdateLog(_T("[接收] ") + CString(szBuf));  // 直接当字符串显示，不对！
    }
}
```

替换为：
```cpp
void CNetworkServerDlg::OnReceive() {
    char buf[4096];
    int n = m_connectSocket.Receive(buf, sizeof(buf));
    if (n > 0) {
        m_recvBuf.append(buf, n);
        uint32_t type, length;
        std::string json;
        while (m_recvBuf.next(type, length, json)) {
            UpdateLog(_T("[收到] 类型=") + CString(std::to_string(type).c_str()) +
                      _T(" 内容=") + CString(json.c_str()));
            m_dispatcher.dispatch(static_cast<MsgType>(type), json);
        }
    }
}
```

### 1.3 验证方法

1. 编译两个项目（Debug x64）
2. 先启动 NetworkServer.exe，点击"启动监听"
3. 再启动 NetworkClient.exe，输入用户名，点击"连接"
4. **服务端日志应该显示**：
   ```
   [连接] TCP 连接成功
   [收到] 类型=1 内容={"username":"Alice"}
   ```
5. 在 `RegisterProtocolHandlers` 的 LOGIN handler 里打个断点，确认能命中

### 1.4 常见问题

**Q: `ProtocolEncode` 返回的 `vector<uint8_t>` 怎么传给 `Send`？**
```cpp
auto data = ProtocolEncode(MsgType::LOGIN, json);
// data.data() 是 const uint8_t*，Send 接受 const void* 或 const char*
m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
```

**Q: 服务端收到乱码？**
确认两端用的编码格式一致。`ProtocolDecodeHeader` 用 `NetToHost32` 解大端序，和 `ProtocolEncode` 的 `HostToNet32` 配对。

---

## 里程碑 2：服务端支持多客户端

### 2.1 问题分析

当前服务端的架构问题：

```cpp
// NetworkServerDlg.h 当前定义
CListenSocket   m_listenSocket;   // 监听 socket（只有一个，没问题）
CConnectSocket  m_connectSocket;  // 连接 socket（只有一个！）
```

当第一个客户端连上来时，`Accept` 将 `m_connectSocket` 与客户端绑定。当第二个客户端连上来时，再次 `Accept` 就会**覆盖** `m_connectSocket`，导致：
- 第一个客户端断连
- 服务端无法向第一个客户端发消息

### 2.2 你要先搞懂的概念

- **`Accept` 的工作方式**：`CAsyncSocket::Accept` 把你传入的空 socket 对象与远程客户端绑定。之后这个 socket 就专门用于和该客户端通信。
- **一个 socket 一个客户端**：服务端需要的不是"一个"连接 socket，而是一个**集合**。
- **动态分配与生命周期**：`new CConnectSocket` 创建的对象什么时候释放？答案是：客户端断开时。在 `OnClose` 回调中 `delete`。

### 2.3 修改方案

#### 步骤 1：修改 CConnectSocket（服务端版）

**CConnectSocket.h**（NetworkServer 项目）：
```cpp
class CConnectSocket : public CAsyncSocket
{
public:
    CConnectSocket();
    virtual ~CConnectSocket();

    CNetworkServerDlg* m_pDlg;
    RecvBuffer m_recvBuf;  // 每个客户端各有一个接收缓冲区

    virtual void OnReceive(int nErrorCode);
    virtual void OnClose(int nErrorCode);
};
```

**CConnectSocket.cpp** — OnReceive 改为自己拆包：
```cpp
void CConnectSocket::OnReceive(int nErrorCode) {
    if (nErrorCode != 0) return;

    char buf[4096];
    int n = Receive(buf, sizeof(buf));
    if (n <= 0) return;

    m_recvBuf.append(buf, n);
    uint32_t type, length;
    std::string json;
    while (m_recvBuf.next(type, length, json)) {
        if (m_pDlg) {
            // 把完整消息和 socket 指针一起传给 Dlg
            m_pDlg->OnMessageReceived(this,
                static_cast<MsgType>(type), json);
        }
    }
}

void CConnectSocket::OnClose(int nErrorCode) {
    if (m_pDlg) {
        m_pDlg->OnClientDisconnected(this);
        // 注意：不要在 OnClientDisconnected 里 delete this
        // 应该在 CAsyncSocket::OnClose 之后再处理
    }
    CAsyncSocket::OnClose(nErrorCode);
}
```

#### 步骤 2：修改服务端 Dlg

**NetworkServerDlg.h** — 添加新成员：
```cpp
#include <vector>
#include <unordered_map>

// 替换单个 m_connectSocket
// CConnectSocket m_connectSocket;  ← 删除这行

// 客户端管理
std::vector<CConnectSocket*> m_clientSockets;

// 新增公开方法
void OnMessageReceived(CConnectSocket* pSocket, MsgType type,
                       const std::string& json);
void OnClientDisconnected(CConnectSocket* pSocket);

// 辅助：向指定客户端发送消息
void SendToClient(CConnectSocket* pSocket, MsgType type,
                  const std::string& json);
```

**NetworkServerDlg.cpp** — OnAccept 改为动态创建：
```cpp
void CNetworkServerDlg::OnAccept() {
    CConnectSocket* pSocket = new CConnectSocket;
    pSocket->m_pDlg = this;

    if (m_listenSocket.Accept(*pSocket)) {
        m_clientSockets.push_back(pSocket);
        UpdateLog(_T("[连接] 新客户端接入"));
    } else {
        delete pSocket;
        UpdateLog(_T("[错误] Accept 失败"));
    }
}
```

**NetworkServerDlg.cpp** — 新增辅助方法：
```cpp
void CNetworkServerDlg::OnMessageReceived(CConnectSocket* pSocket,
                                           MsgType type,
                                           const std::string& json) {
    UpdateLog(_T("[收到] 类型=") +
              CString(std::to_string(static_cast<uint32_t>(type)).c_str()) +
              _T(" 内容=") + CString(json.c_str()));
    m_dispatcher.dispatch(type, json);
    // 注意：dispatch 时还不知道是哪个客户端发的！
    // 里程碑 3 会解决这个问题
}

void CNetworkServerDlg::OnClientDisconnected(CConnectSocket* pSocket) {
    // 从列表移除
    auto it = std::find(m_clientSockets.begin(),
                        m_clientSockets.end(), pSocket);
    if (it != m_clientSockets.end()) {
        m_clientSockets.erase(it);
    }
    pSocket->Close();
    delete pSocket;
    UpdateLog(_T("[断开] 客户端断开"));
}

void CNetworkServerDlg::SendToClient(CConnectSocket* pSocket,
                                      MsgType type,
                                      const std::string& json) {
    auto data = ProtocolEncode(type, json);
    pSocket->Send(data.data(), static_cast<int>(data.size()));
}
```

#### 步骤 3：清理旧代码

- `OnBnClickedButtonStop` 中关闭 `m_connectSocket` 改为遍历关闭 `m_clientSockets`
- `OnClose` 回调逻辑移到 `OnClientDisconnected`

### 2.4 验证方法

1. 启动服务端 → 启动客户端 A → 服务端显示"新客户端接入"
2. 再启动客户端 B → 服务端显示又一个"新客户端接入"
3. 客户端 A 和 B 都正常连接，不再互相踢掉
4. 关闭客户端 A → 服务端显示"客户端断开"

---

## 里程碑 3：登录 / 登出流程

### 3.1 你需要的数据结构

在 `NetworkServerDlg.h` 中添加：
```cpp
// 在线用户映射
std::unordered_map<int, CConnectSocket*>  m_userIdToSocket;  // userId → socket
std::unordered_map<CConnectSocket*, int>  m_socketToUserId;  // socket → userId
std::unordered_map<int, std::string>      m_userIdToName;    // userId → username
```

### 3.2 JSON 解析工具函数

在服务端和客户端都需要解析 JSON。先写一个极简工具（放在 `Common/` 目录或各自的 .cpp 文件顶部）：

```cpp
// 极简 JSON 字符串值提取器（只支持 "key":"value" 格式）
static std::string JsonGetString(const std::string& json,
                                  const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t end = json.find("\"", pos);
    return json.substr(pos, end - pos);
}

static int JsonGetInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.length();
    return std::stoi(json.substr(pos));
}

static std::string JsonSetString(const std::string& key,
                                  const std::string& value) {
    return "\"" + key + "\":\"" + value + "\"";
}

static std::string JsonSetInt(const std::string& key, int value) {
    return "\"" + key + "\":" + std::to_string(value);
}
```

> 进阶选项：引入 [nlohmann/json](https://github.com/nlohmann/json)（单个 `json.hpp` 头文件），用法见本文末尾附录。

### 3.3 服务端 LOGIN 处理

在 `RegisterProtocolHandlers()` 中实现：

```cpp
m_dispatcher.on(MsgType::LOGIN, [this](const std::string& json) {
    // 注意：这里还不知道是哪个 socket 发的！
    // 需要先解决"消息来源"的问题（见 3.4）
});
```

### 3.4 核心问题：Dispatcher 不知道消息来源

`m_dispatcher.dispatch(type, json)` 只传了消息类型和内容，**没有传 socket 指针**。服务端需要知道"谁发的消息"才能回复。

**解决方案**：修改 dispatcher 的调用方式，或者在消息处理前把 socket 存到上下文。

最简单的方法 — 在 `OnMessageReceived` 中先记下当前 socket，再 dispatch：

```cpp
// NetworkServerDlg.h 添加
CConnectSocket* m_currentSocket = nullptr;  // 当前正在处理消息的 socket

// OnMessageReceived 中：
void CNetworkServerDlg::OnMessageReceived(CConnectSocket* pSocket, ...) {
    m_currentSocket = pSocket;
    m_dispatcher.dispatch(type, json);
    m_currentSocket = nullptr;
}
```

然后在 LOGIN handler 里用 `m_currentSocket`：

```cpp
m_dispatcher.on(MsgType::LOGIN, [this](const std::string& json) {
    std::string username = JsonGetString(json, "username");
    if (username.empty()) {
        SendToClient(m_currentSocket, MsgType::LOGIN_RESP,
            "{" + JsonSetString("success", "false") + "," +
            JsonSetString("reason", "用户名为空") + "}");
        return;
    }

    // 1. 查数据库（已有则返回已有 ID，没有则新建）
    int userId = m_dbWrapper.iAddUser(username);

    // 2. 记录在线状态
    m_userIdToSocket[userId] = m_currentSocket;
    m_socketToUserId[m_currentSocket] = userId;
    m_userIdToName[userId] = username;
    OnUserLogin(userId, username, "unknown");  // 更新 UI

    // 3. 查询好友列表
    auto friends = m_dbWrapper.vecGetFriends(userId);
    std::string friendListJson = "\"friends\":[";
    for (size_t i = 0; i < friends.size(); i++) {
        if (i > 0) friendListJson += ",";
        int fid = friends[i].first;
        std::string fname = friends[i].second;
        bool online = m_userIdToSocket.count(fid) > 0;
        friendListJson += "{" +
            JsonSetInt("user_id", fid) + "," +
            JsonSetString("username", fname) + "," +
            JsonSetString("online", online ? "true" : "false") + "}";
    }
    friendListJson += "]";

    // 4. 发送 LOGIN_RESP
    std::string resp = "{" +
        JsonSetString("success", "true") + "," +
        JsonSetInt("user_id", userId) + "," +
        friendListJson + "}";
    SendToClient(m_currentSocket, MsgType::LOGIN_RESP, resp);

    // 5. 广播 STATUS_ONLINE 给其他在线用户
    std::string onlineMsg = "{" +
        JsonSetInt("user_id", userId) + "," +
        JsonSetString("username", username) + "}";
    for (auto& [uid, sock] : m_userIdToSocket) {
        if (uid != userId) {
            SendToClient(sock, MsgType::STATUS_ONLINE, onlineMsg);
        }
    }

    UpdateLog(_T("[登录] ") + CString(username.c_str()) +
              _T(" (ID=") + CString(std::to_string(userId).c_str()) + _T(")"));
});
```

### 3.5 服务端 LOGOUT 处理

```cpp
m_dispatcher.on(MsgType::LOGOUT, [this](const std::string& json) {
    if (!m_currentSocket) return;
    auto it = m_socketToUserId.find(m_currentSocket);
    if (it == m_socketToUserId.end()) return;

    int userId = it->second;
    std::string username = m_userIdToName[userId];

    // 广播下线通知
    std::string offlineMsg = "{" +
        JsonSetInt("user_id", userId) + "," +
        JsonSetString("username", username) + "}";
    for (auto& [uid, sock] : m_userIdToSocket) {
        if (uid != userId) {
            SendToClient(sock, MsgType::STATUS_OFFLINE, offlineMsg);
        }
    }

    // 清理状态
    m_userIdToSocket.erase(userId);
    m_socketToUserId.erase(m_currentSocket);
    m_userIdToName.erase(userId);
    OnUserLogout(userId);
});
```

同时修改 `OnClientDisconnected`，在 socket 断开时自动清理：
```cpp
void CNetworkServerDlg::OnClientDisconnected(CConnectSocket* pSocket) {
    // 清理在线状态
    auto it = m_socketToUserId.find(pSocket);
    if (it != m_socketToUserId.end()) {
        int userId = it->second;
        std::string username = m_userIdToName[userId];

        // 广播下线
        std::string offlineMsg = "{" +
            JsonSetInt("user_id", userId) + "," +
            JsonSetString("username", username) + "}";
        for (auto& [uid, sock] : m_userIdToSocket) {
            if (uid != userId) {
                SendToClient(sock, MsgType::STATUS_OFFLINE, offlineMsg);
            }
        }

        m_userIdToSocket.erase(userId);
        m_socketToUserId.erase(pSocket);
        m_userIdToName.erase(userId);
        OnUserLogout(userId);
    }

    auto sit = std::find(m_clientSockets.begin(),
                         m_clientSockets.end(), pSocket);
    if (sit != m_clientSockets.end()) m_clientSockets.erase(sit);

    pSocket->Close();
    delete pSocket;
}
```

### 3.6 客户端 LOGIN_RESP 处理

```cpp
m_dispatcher.on(MsgType::LOGIN_RESP, [this](const std::string& json) {
    std::string success = JsonGetString(json, "success");
    if (success == "true") {
        int userId = JsonGetInt(json, "user_id");
        // 解析好友列表（略，完整代码需要解析 JSON 数组）
        OnLoginSuccess(userId, m_username, {});
        UpdateStatus(_T("已登录"));
    } else {
        AfxMessageBox(_T("登录失败"));
        UpdateStatus(_T("登录失败"));
    }
});

m_dispatcher.on(MsgType::STATUS_ONLINE, [this](const std::string& json) {
    int userId = JsonGetInt(json, "user_id");
    std::string name = JsonGetString(json, "username");
    OnFriendStatusChanged(userId, name, true);
});

m_dispatcher.on(MsgType::STATUS_OFFLINE, [this](const std::string& json) {
    int userId = JsonGetInt(json, "user_id");
    std::string name = JsonGetString(json, "username");
    OnFriendStatusChanged(userId, name, false);
});
```

### 3.7 客户端发送 LOGIN

在 `OnBnClickedButtonConnect` 里，连接成功后发送 LOGIN：
```cpp
void CNetworkClientDlg::OnBnClickedButtonConnect() {
    // ... 连接逻辑 ...

    // TCP 连接成功后（在 OnConnect 回调里发送 LOGIN）
}
```

`OnConnect`：
```cpp
void CNetworkClientDlg::OnConnect() {
    UpdateStatus(_T("已连接"));
    GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(TRUE);
    UpdateLog(_T("[连接] TCP 连接成功"));

    // 发送 LOGIN
    std::string json = "{" + JsonSetString("username", m_username) + "}";
    auto data = ProtocolEncode(MsgType::LOGIN, json);
    m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
}
```

### 3.8 验证方法

1. 启动服务端 → 启动客户端 A → 输入 "Alice"，连接
2. 服务端显示 "[登录] Alice (ID=1)"
3. 客户端 A 显示 "已登录 — Alice"
4. 启动客户端 B → 输入 "Bob"，连接
5. 客户端 A 的好友列表应该出现 "● Bob (在线)"
6. 关闭客户端 B → 客户端 A 的 Bob 变为 "○ Bob (离线)"

---

## 里程碑 4：好友管理

### 4.1 添加好友

**客户端**（OnBnClickedButtonAddFriend）：
```cpp
void CNetworkClientDlg::OnBnClickedButtonAddFriend() {
    // 从消息框读取好友名（格式：/addfriend <用户名>）
    CString strMsg;
    GetDlgItemText(IDC_EDIT_MSG, strMsg);
    strMsg.Trim();
    if (strMsg.Find(_T("/addfriend ")) == 0)
        strMsg = strMsg.Mid(11);
    strMsg.Trim();
    if (strMsg.IsEmpty()) {
        AfxMessageBox(_T("请先在消息框输入 /addfriend <用户名>"));
        return;
    }

    std::string friendName = CT2A(strMsg);
    std::string json = "{" +
        JsonSetInt("user_id", m_userId) + "," +
        JsonSetString("friend_username", friendName) + "}";
    auto data = ProtocolEncode(MsgType::FRIEND_ADD, json);
    m_connectSocket.Send(data.data(), static_cast<int>(data.size()));

    UpdateLog(_T("[好友] 发送添加好友请求: ") + strMsg);
    SetDlgItemText(IDC_EDIT_MSG, _T(""));
}
```

**服务端**：
```cpp
m_dispatcher.on(MsgType::FRIEND_ADD, [this](const std::string& json) {
    int userId = JsonGetInt(json, "user_id");
    std::string friendName = JsonGetString(json, "friend_username");

    int friendId = m_dbWrapper.iGetUserId(friendName);
    if (friendId < 0) {
        // 用户不存在
        SendToClient(m_currentSocket, MsgType::ACK, "{...}");
        return;
    }

    m_dbWrapper.bAddFriend(userId, friendId);

    // 通知双方更新好友列表
    // 向请求方回复 ACK
    // 如果 friend 在线，也通知 friend
});
```

### 4.2 删除好友

类似添加好友的流程，调用 `m_dbWrapper.bRemoveFriend(userId, friendId)`。

### 4.3 好友列表

登录时已经在 LOGIN_RESP 里返回了好友列表。也可以单独请求：

```cpp
m_dispatcher.on(MsgType::FRIEND_LIST, [this](const std::string& json) {
    int userId = JsonGetInt(json, "user_id");
    auto friends = m_dbWrapper.vecGetFriends(userId);
    // 构造 JSON 返回
});
```

### 4.4 验证

- Alice 和 Bob 都登录 → Alice 添加 Bob 为好友 → Alice 的好友列表出现 Bob
- Bob 的好友列表也应出现 Alice（双向好友）

---

## 里程碑 5：单聊消息

### 5.1 客户端发送

修改 `OnBnClickedButtonSend`：
```cpp
void CNetworkClientDlg::OnBnClickedButtonSend() {
    CString strMsg;
    GetDlgItemText(IDC_EDIT_MSG, strMsg);
    if (strMsg.IsEmpty()) return;
    if (m_userId < 0) { AfxMessageBox(_T("请先登录！")); return; }
    if (m_selectedFriendId <= 0) { AfxMessageBox(_T("请选择一个好友！")); return; }

    std::string content = CT2A(strMsg);

    // 构造 TEXT 消息
    std::string json = "{" +
        JsonSetInt("sender_id", m_userId) + "," +
        JsonSetInt("receiver_id", m_selectedFriendId) + "," +
        JsonSetString("content", content) + "}";
    auto data = ProtocolEncode(MsgType::TEXT, json);
    m_connectSocket.Send(data.data(), static_cast<int>(data.size()));

    // 本地显示
    AppendChatMessage(_T("[我] ") + strMsg);
    SetDlgItemText(IDC_EDIT_MSG, _T(""));
}
```

### 5.2 服务端转发

```cpp
m_dispatcher.on(MsgType::TEXT, [this](const std::string& json) {
    int senderId = JsonGetInt(json, "sender_id");
    int receiverId = JsonGetInt(json, "receiver_id");
    std::string content = JsonGetString(json, "content");

    // 1. 存数据库
    m_dbWrapper.bSaveMessage(senderId, receiverId, content);

    // 2. 转发给接收方
    auto it = m_userIdToSocket.find(receiverId);
    if (it != m_userIdToSocket.end()) {
        // 在线 → 直接转发
        SendToClient(it->second, MsgType::TEXT, json);
    } else {
        // 离线 → 存离线消息
        m_dbWrapper.bSaveOfflineMessage(senderId, receiverId, content);
    }
});
```

### 5.3 客户端接收

```cpp
m_dispatcher.on(MsgType::TEXT, [this](const std::string& json) {
    int senderId = JsonGetInt(json, "sender_id");
    std::string content = JsonGetString(json, "content");

    // 查找发送者用户名
    std::string senderName;
    auto it = m_friendMap.find(senderId);
    if (it != m_friendMap.end()) senderName = it->second.username;

    OnMessageReceived(senderName, content, "");
});
```

### 5.4 验证

- Alice 和 Bob 都登录 → Alice 选择 Bob → 发消息 → Bob 实时收到

---

## 里程碑 6：群聊消息

### 6.1 客户端发送

选中"★ 全体（群聊）"时，receiver_id = 0：

```cpp
void CNetworkClientDlg::OnBnClickedButtonSend() {
    // ... 前面相同 ...

    MsgType type = (m_selectedFriendId == 0) ? MsgType::GROUP_TEXT : MsgType::TEXT;

    std::string json = "{" +
        JsonSetInt("sender_id", m_userId) + "," +
        JsonSetInt("receiver_id", m_selectedFriendId) + "," +
        JsonSetString("content", content) + "}";
    auto data = ProtocolEncode(type, json);
    m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
}
```

### 6.2 服务端广播

```cpp
m_dispatcher.on(MsgType::GROUP_TEXT, [this](const std::string& json) {
    int senderId = JsonGetInt(json, "sender_id");

    auto data = ProtocolEncode(MsgType::GROUP_TEXT, json);

    for (auto& [userId, pSocket] : m_userIdToSocket) {
        if (userId != senderId) {  // 不发给自己
            pSocket->Send(data.data(), static_cast<int>(data.size()));
        }
    }
});
```

### 6.3 验证

- 3 个客户端登录 → Alice 选"全体"发消息 → Bob 和 Charlie 都收到

---

## 里程碑 7：离线消息 + 聊天记录

### 7.1 离线消息推送

在 LOGIN handler 中（里程碑 3 的代码基础上）添加：

```cpp
// 查询离线消息
std::vector<std::string> offlineMsgs;
m_dbWrapper.bGetOfflineMessages(userId, offlineMsgs);

// 构造 OFFLINE_MSGS 消息
std::string offlineJson = "{\"messages\":[";
for (size_t i = 0; i < offlineMsgs.size(); i++) {
    if (i > 0) offlineJson += ",";
    offlineJson += "\"" + offlineMsgs[i] + "\"";  // 简单字符串拼接
}
offlineJson += "]}";
SendToClient(m_currentSocket, MsgType::OFFLINE_MSGS, offlineJson);
```

### 7.2 聊天记录查询

客户端请求：
```cpp
// 客户端某处发起请求
std::string json = "{" +
    JsonSetInt("user_id", m_userId) + "," +
    JsonSetInt("other_user_id", m_selectedFriendId) + "," +
    JsonSetInt("limit", "50") + "}";
auto data = ProtocolEncode(MsgType::HISTORY, json);
m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
```

服务端处理：
```cpp
m_dispatcher.on(MsgType::HISTORY, [this](const std::string& json) {
    int userId = JsonGetInt(json, "user_id");
    int otherId = JsonGetInt(json, "other_user_id");
    int limit = JsonGetInt(json, "limit");

    std::vector<std::string> messages;
    m_dbWrapper.bGetMessages(userId, otherId, limit, messages);

    // 构造 JSON 返回
    std::string resp = "{\"messages\":[";
    for (size_t i = 0; i < messages.size(); i++) {
        if (i > 0) resp += ",";
        resp += "\"" + messages[i] + "\"";
    }
    resp += "]}";
    SendToClient(m_currentSocket, MsgType::HISTORY, resp);
});
```

### 7.3 验证

- Alice 登录，Bob 离线 → Alice 给 Bob 发消息 → Bob 登录 → 收到离线消息
- 选中好友 → 聊天区显示历史记录

---

## 里程碑 8：文件传输（FTP 集成）

### 8.1 前提

需要运行一个 FTP 服务器（如 FileZilla Server），与聊天服务端可在同一机器。

### 8.2 客户端发送文件

```cpp
void CNetworkClientDlg::OnBnClickedButtonSendFile() {
    CFileDialog dlg(TRUE, nullptr, nullptr,
                    OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    _T("所有文件 (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK) return;

    CString filePath = dlg.GetPathName();
    CString fileName = dlg.GetFileName();
    long fileSize = ...; // 获取文件大小

    // 1. 上传到 FTP
    FtpHelper ftp;
    ftp.Connect(L"127.0.0.1", 21, L"anonymous", L"");
    std::wstring remotePath = L"/files/" + std::wstring(fileName);
    if (!ftp.UploadFile(std::wstring(filePath), remotePath)) {
        AfxMessageBox(_T("FTP 上传失败"));
        return;
    }

    // 2. 发送 FILE_REQUEST 通知对方
    std::string json = "{" +
        JsonSetInt("sender_id", m_userId) + "," +
        JsonSetInt("receiver_id", m_selectedFriendId) + "," +
        JsonSetString("filename", CT2A(fileName)) + "," +
        JsonSetInt("filesize", fileSize) + "," +
        JsonSetString("ftp_path", CT2A(remotePath.c_str())) + "}";
    auto data = ProtocolEncode(MsgType::FILE_REQUEST, json);
    m_connectSocket.Send(data.data(), static_cast<int>(data.size()));

    AppendChatMessage(_T("[文件] 已发送: ") + fileName);
}
```

### 8.3 客户端接收文件

```cpp
m_dispatcher.on(MsgType::FILE_REQUEST, [this](const std::string& json) {
    std::string fileName = JsonGetString(json, "filename");
    std::string ftpPath = JsonGetString(json, "ftp_path");

    CString msg;
    msg.Format(_T("收到文件: %s，是否接收？"), CString(fileName.c_str()));
    if (AfxMessageBox(msg, MB_YESNO) == IDYES) {
        // 下载
        FtpHelper ftp;
        ftp.Connect(L"127.0.0.1", 21, L"anonymous", L"");
        CString localPath = _T("C:\\Downloads\\") + CString(fileName.c_str());
        ftp.DownloadFile(CString(ftpPath.c_str()), localPath);

        // 回复接受
        std::string resp = "{" +
            JsonSetInt("sender_id", m_userId) + "," +
            JsonSetInt("receiver_id", JsonGetInt(json, "sender_id")) + "," +
            JsonSetString("accepted", "true") + "}";
        auto data = ProtocolEncode(MsgType::FILE_RESP, resp);
        m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
    }
});
```

### 8.4 验证

- Alice 选择文件发送给 Bob → Bob 收到文件请求 → 接受 → 文件下载到本地

---

## 附录 A：JSON 工具函数（完整版）

放在 `Common/JsonUtils.h` 或各自的 .cpp 顶部：

```cpp
#pragma once
#include <string>
#include <sstream>

inline std::string JsonGetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

inline int JsonGetInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.length();
    std::string num;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '-'))
        num += json[pos++];
    return num.empty() ? 0 : std::stoi(num);
}

inline std::string JsonSetString(const std::string& key, const std::string& value) {
    return "\"" + key + "\":\"" + value + "\"";
}

inline std::string JsonSetInt(const std::string& key, int value) {
    return "\"" + key + "\":" + std::to_string(value);
}

// 构造简单 JSON 对象
inline std::string JsonMakeObject(std::initializer_list<std::string> pairs) {
    std::string result = "{";
    bool first = true;
    for (auto& p : pairs) {
        if (!first) result += ",";
        result += p;
        first = false;
    }
    result += "}";
    return result;
}
```

用法示例：
```cpp
std::string json = JsonMakeObject({
    JsonSetInt("sender_id", 1),
    JsonSetInt("receiver_id", 2),
    JsonSetString("content", "Hello")
});
// 结果: {"sender_id":1,"receiver_id":2,"content":"Hello"}
```

---

## 附录 B：nlohmann/json 快速上手

如果要引入完整的 JSON 库：

1. 从 https://github.com/nlohmann/json/releases 下载 `json.hpp`
2. 放到 `Common/` 目录
3. 在代码中使用：

```cpp
#include "../Common/json.hpp"
using json = nlohmann::json;

// 解析
auto j = json::parse(jsonString);
std::string username = j["username"];
int userId = j["user_id"];

// 构造
json resp;
resp["success"] = true;
resp["user_id"] = userId;
resp["friends"] = json::array();
for (auto& f : friendList) {
    json fj;
    fj["user_id"] = f.first;
    fj["username"] = f.second;
    resp["friends"].push_back(fj);
}
std::string output = resp.dump();
```
