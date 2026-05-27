# 服务端多线程迁移计划

## 目标

将服务端从 `CAsyncSocket`（单线程异步）改为 BSD Socket + `AfxBeginThread` 多线程。客户端不变。

---

## 修改范围

| 文件 | 程度 |
|------|------|
| `NetworkServer/pch.h` | 删 `<afxsock.h>`，加 `<afxmt.h>` |
| `NetworkServer/CConnectSocket.h/.cpp` | 完全重写 |
| `NetworkServer/CListenSocket.h/.cpp` | 完全重写 |
| `NetworkServer/NetworkServerDlg.h` | 加锁/UI更新成员，删 m_currentSocket |
| `NetworkServer/NetworkServerDlg.cpp` | 改所有 handler、启动/停止、断开处理 |
| `Common/Protocol.h` | 不动 |
| `NetworkClient/*` | 不动 |

---

## 架构对比

```
旧（CAsyncSocket）：              新（BSD + 多线程）：
主线程（消息循环）                 主线程（UI）
  ├─ OnAccept                       ├─ ListenThread: accept() 阻塞循环
  ├─ OnReceive (多个socket)         │   └─ 每accept → new CConnectSocket → ClientThread
  └─ OnClose                        ├─ ClientThread1: recv() 阻塞 → RecvBuffer → dispatch
                                    ├─ ClientThread2: recv() 阻塞 → RecvBuffer → dispatch
                                    └─ ClientThread3: ...

共享数据无锁（串行）               共享数据用 CCriticalSection 保护
直接操作 UI                         PostMessage(WM_USER+1) 更新 UI
```

---

## 实现顺序

| # | 文件 | 内容 |
|---|------|------|
| 1 | pch.h | `afxsock.h` → `afxmt.h` |
| 2 | CConnectSocket.h/.cpp | SOCKET + thread + RecvBuffer + Send |
| 3 | CListenSocket.h/.cpp | SOCKET + accept 线程 |
| 4 | NetworkServerDlg.h | 加 CCriticalSection、WM_USER+1、新方法 |
| 5 | NetworkServerDlg.cpp | thread_local、handler 改锁、启动/停止重写 |

---

## 新类设计

### CConnectSocket

```
成员:
  m_socket (SOCKET)
  m_recvBuf (RecvBuffer)           ← 保留，每线程独立
  m_pThread (CWinThread*)
  m_bRunning (volatile LONG)

关键方法:
  Start()      → AfxBeginThread(ThreadProc, this) → recv 循环
  Send()       → 阻塞 send，循环直到写完
  Stop()       → shutdown → closesocket → WaitForSingleObject
  ThreadProc() → 静态，调用 RecvLoop()
  RecvLoop()   → while(m_bRunning) { recv → append → next → dispatch }
  ~dtor()      → 虚的？不需要。线程负责 delete this
```

### CListenSocket

```
成员:
  m_socket (SOCKET)
  m_pThread (CWinThread*)
  m_bRunning (volatile LONG)

关键方法:
  Start(port) → socket+bind+listen → AfxBeginThread
  Stop()      → closesocket → WaitForSingleObject
  AcceptLoop() → while(m_bRunning) { accept → new CConnectSocket → Start() }
```

---

## 关键设计决策

### 1. Protocol.h 不动

`Handler = void(const std::string&)` 签名不变，用 `thread_local g_pCurrentSocket` 传递 socket 上下文。

### 2. 锁策略（CCriticalSection m_csData）

- `m_userIdToSocket`、`m_socketToUserId`、`m_userIdToName` → 读写加锁
- `m_connectSockets` → 增删加锁
- `m_dbWrapper` 调用 → 加锁
- **绝不持锁调用 send()**

广播模式：加锁复制 socket 列表 → 解锁 → 逐个 send。

### 3. UI 更新：PostMessage

```cpp
struct UIUpdateData { type, text, userId, username, ip };
PostMessage(WM_USER_UPDATE_UI, type, (LPARAM)new UIUpdateData{...});
// UI 线程: OnUIUpdate → unique_ptr 接收 → 分发到具体方法
```

### 4. 线程生命周期

- ListenThread: Start() 创建 → Stop() closesocket → accept 返回错误 → 线程退出
- ClientThread: Start() 创建 → recv 返回 0/错误 → RecvLoop 退出 → delete this
- 关闭顺序: ListenThread 先停 → ClientThread 逐个停 → 清空 maps

---

## 验证

1. 启动服务端
2. 多客户端登录，确认上线列表正确
3. 好友添加/接受，确认双方列表更新
4. 客户端断开，确认服务端清理 + 广播离线
5. 服务端停止，确认无崩溃
