# MiniIM 项目进度总结

> 截至 2026-06-03，**项目已完成 8 个里程碑 + 开源准备**，已推送 GitHub。
>
> 本文件用于跨对话保留上下文，新会话开始时读这一份 + `CLAUDE.md` 即可。

---

## 项目概况

- **仓库**：https://github.com/jialanhu0915/MiniIM
- **本地路径**：`/mnt/b/VS_Project/ComputerNetworkProgramming`（WSL 视角）
- **类型**：C/S 即时通讯教学项目，MFC + WinSock
- **License**：MIT (Copyright Yan Runxin 2026)
- **关联项目**：[MiniFtpServer](https://github.com/jialanhu0915/MiniFtpServer) —— 配套的独立 FTP 服务端

## 技术栈

- MFC 对话框（Visual Studio 2019+，x64，Unicode 字符集）
- Windows Sockets 2 (BSD socket + `AfxBeginThread` 多线程)
- SQLite（`SQLiteWrapper` 封装，两把锁 m_csData / m_csDb）
- WinInet（客户端 FTP 客户端 `FtpHelper`）
- 自研手写 JSON 工具（`Common/JsonUtils.h`，无外部依赖）
- 协议：二进制头部（4 字节类型 + 4 字节长度）+ UTF-8 JSON

## 8 个里程碑全部完成

```
里程碑 1 (理解现状 + 协议)   ✅
里程碑 2 (多客户端)          ✅
里程碑 3 (登录/登出)         ✅
里程碑 4 (好友管理)          ✅
里程碑 5 (单聊消息)          ✅
里程碑 6 (群聊消息)          ✅
里程碑 7 (离线消息/聊天记录)  ✅
里程碑 8 (文件传输)          ✅
```

## 已实现功能

| 功能 | 协议 | 状态 |
|------|------|------|
| 登录/登出 | LOGIN / LOGIN_RESP / LOGOUT | ✅ |
| 好友管理 | FRIEND_ADD/ACCEPT/REJECT/DEL/LIST/ADD_RESP | ✅ |
| 单聊 | TEXT（带时间戳、离线存储） | ✅ |
| 群聊 | GROUP_TEXT（receiver_id=0 全体广播） | ✅ |
| 离线消息 | OFFLINE_MSGS（登录推送） | ✅ |
| 历史记录 | HISTORY（SQLite 持久化） | ✅ |
| 状态通知 | STATUS_ONLINE/OFFLINE（广播给好友） | ✅ |
| 文件传输 | FILE_REQUEST / FILE_RESP（FTP 中转） | ✅ |
| 沙箱 FTP | FtpRoot\ 限根目录 | ✅ |
| 双端口监听 | IM 21 + FTP 2121 | ✅ |

## 双进程模型

```
NetworkServer 进程
├── 21 端口（IM 协议）— CListenSocket + CConnectSocket
│   每客户端 1 个 CWinThread + 1 个 CConnectSocket
│   delete this 自销毁
└── 2121 端口（FTP 中转）— CFtpListenSocket + CFtpSession
    每客户端 1 个 AfxBeginThread + 1 个 CFtpSession
    delete this 自销毁

NetworkClient 进程
└── 21 端口到服务端（IM 协议）— CConnectSocket（CAsyncSocket 异步）
    + WinInet FtpHelper 单独连 2121（FTP 客户端）
```

## 关键文件清单

### Common/
- `Protocol.h` — MsgType 枚举、RecvBuffer、ProtocolDispatcher、ProtocolEncode/Decode
- `JsonUtils.h` — JsonGetString/Int/Array、JsonSetString/Int/LongLong、JsonMakeObject

### NetworkServer/
- `NetworkServerDlg.h/.cpp` — 主对话框，注册所有协议 handler
- `CListenSocket.h/.cpp` — IM 监听 socket
- `CConnectSocket.h/.cpp` — per-client IM socket（自销毁）
- `CFtpListenSocket.h/.cpp` — FTP 监听 socket
- `CFtpSession.h/.cpp` — per-client FTP 会话（自销毁）
- `SQLiteWrapper.h/.cpp` — SQLite 封装（Xxx/XxxImpl 模式，m_csDb 锁）

### NetworkClient/
- `NetworkClientDlg.h/.cpp` — 主对话框
- `CConnectSocket.h/.cpp` — CAsyncSocket 异步
- `CAddFriendDlg.h/.cpp` — 添加好友对话框
- `FtpHelper.h/.cpp` — WinInet 客户端

## 关键设计决策

1. **两把锁不嵌套**：handler 持 m_csData 期间不调 DB，调 DB 时不加 m_csData
2. **跨线程 UI 安全**：日志通过 `PostUIUpdate(WM_USER_UPDATE_UI)` 投递，**不直接调 UpdateLog**
3. **FTP 是中转模式**：客户端先上传到服务端 FtpRoot\，再通知接收方下载。简单可靠，穿透 NAT
4. **沙箱 FTP**：`ResolveRealPath()` 验证所有路径在 m_root 内
5. **Doxygen 注释**：所有 .h/.cpp 已有 Doxygen 风格注释（中文/英文按文件原风格）
6. **FtpHelper 在客户端独有**：最初在 Common/，后迁到 NetworkClient/（"标准做法"原则：仅客户端用就不放 Common）

## Git 历史（最近 10 commit）

```
6c241e0 开源准备：仓库改名为 MiniIM + 加 LICENSE + 加 .gitignore
044712a 清理收尾：删死代码 + 补 README + 更新学习路线
2717500 客户端 FtpHelper 迁移 + FILE_REQUEST/RESP 收发
5ac774f 服务端内嵌 FTP 监听（端口 2121）
fb0f646 消息时间戳透传 + 历史消息短格式 + 统一名字解析
d5aee00 NetworkServerDlg.cpp: 补全 Doxygen 注释
aa3e49d CLAUDE.md：加「编辑后必检」规则提醒
69a5e05 .gitattributes 自身强制 LF
7434fc8 锁定 .h/.cpp 强制 CRLF（MSVC 兼容性）
665cf34 修复编码：UTF-8 with BOM + CRLF（22 个文件）
```

## 当前状态

- ✅ 所有功能编译通过（`msbuild` 0 错误）
- ✅ 端到端联调通过（FTP 文件收发跑通）
- ✅ 4 个 commit 已 push 到 GitHub
- ✅ README + LICENSE + .gitignore 完整
- ✅ 仓库名 MiniIM（与 MiniFtpServer 风格一致）

## GitHub 仓库待办

- [ ] 在仓库 About 加 Topics：`mfc`, `instant-messaging`, `network-programming`, `c-plus-plus`, `windows-sockets`, `ftp`, `educational`
- [ ] 在仓库 About 加 Description（push 时 GitHub 可能自动填）
- [ ] （可选）Branch 改名 master → main
- [ ] （可选）v1.0.0 release tag

## 关键文档

- `CLAUDE.md` —— 项目规则（必读！含编码陷阱、注释规范、构建命令等）
- `LEARNING_GUIDE.md` —— 8 个里程碑学习路线（已标记全部完成 + 末尾"扩展方向"章节）
- `KNOWLEDGE_BASE.md` —— TCP 粘包/半包、字节序、CAsyncSocket 异步模型详解
- `README.md` —— 英文为主的项目说明（已加 badges）
- `MULTITHREAD_PLAN.md` —— 多线程设计（历史规划文件）
- `PROGRESS.md` —— **本文件，跨对话上下文**

## 下一步可选方向（LEARNING_GUIDE.md 末尾有完整列表）

按兴趣挑 1-2 个深入：
- **TLS/SSL 加密**（传输层安全）
- **断点续传**（FTP REST 命令）
- **P2P 直连**（STUN/TURN/NAT 穿透，复杂）
- **群组成员管理**（当前是"全体"，缺真正群组）
- **心跳保活**（替代 TCP keepalive）
- **消息已读回执**（TEXT + READ_ACK 协议）
- **进度条/分块**（大文件传输 UX）

## 项目学习意义

- 完整的多线程 socket 编程（accept + 客户端线程 + 自销毁）
- 二进制协议设计 + JSON 序列化
- 两把锁的并发设计（m_csData vs m_csDb，Xxx/XxxImpl 模式）
- MFC 跨线程 UI（PostMessage 模式）
- 沙箱安全（ResolveRealPath 越界检测）
- 自包含项目（无外部 JSON 库、无 Protobuf，纯手写）

## 复现项目

```powershell
# 1. 克隆
git clone https://github.com/jialanhu0915/MiniIM.git
cd MiniIM

# 2. 编译
msbuild MiniIM.slnx /p:Configuration=Debug /p:Platform=x64

# 3. 启动
x64\Debug\NetworkServer.exe  # 先启动，点"启动"
x64\Debug\NetworkClient.exe  # 再启动一个或多个
```

## 跟 MiniFtpServer 的关系

- **MiniFtpServer** 是独立 FTP 服务端项目（含 README_CN/EN，MIT，4 个命令 PASV/LIST/RETR/STOR 等）
- **MiniIM** 把 MiniFtpServer 的核心（CFtpSession + CListenSocket）嵌入到 IM 服务端，作为文件传输的 FTP 中转层
- 两个项目代码相似但用途不同：MiniFtpServer 是 FTP 教学，MiniIM 是 IM 教学

## 关键 Bug 修复（已修复）

| Bug | 修复 |
|-----|------|
| `CFtpListenSocket::AcceptLoop` 子线程直接调 `UpdateLog` | 改用 `PostUIUpdate` 跨线程安全投递 |
| `FtpHelper::GetLastErrorMessage` const 函数内取 `&m_dwLastError` | 用局部变量 `DWORD dwError = m_dwLastError;` 绕过 const |

## 8 处 TODO 注释清理

`NetworkClient/NetworkClientDlg.cpp` 和 `NetworkServer/CListenSocket.cpp` 中的 TODO 注释（代码早已实现但 TODO 注释残留）已全部清理。

---

**如果你要在新对话中继续**，请把这个 PROGRESS.md 读一遍，背景就齐了。
