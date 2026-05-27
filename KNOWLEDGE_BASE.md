# 计算机网络核心概念参考

## TCP 消息分帧

TCP 是**字节流**协议，不是**消息流**协议。它只保证字节按顺序到达，不保证每次 `recv` 的边界和 `send` 的边界一致：

```
发送方调用：  send("ABC")  send("DEF")  send("GHI")
              |            |            |
网络传输：    ....ABCDEFGHI............
              |        |          |
接收方可能：  recv→"ABCD" recv→"EFG" recv→"HI"
```

**粘包**：多条消息粘在一次 recv 里
**半包**：一条消息分多次 recv 才收完

解决方案：在每条消息前面加上**长度字段**。本项目 Protocol.h 里 `[4B type][4B length][N B body]` 就是这个设计。

---

## 网络字节序

不同 CPU 架构存储多字节整数的顺序不同：
- **小端**（x86/x64）：低位字节在前。`0x12345678` 存为 `78 56 34 12`
- **大端**（网络字节序）：高位字节在前。`0x12345678` 存为 `12 34 56 78`

网络协议统一用大端。Protocol.h 里的 `HostToNet32()` / `NetToHost32()` 就是做这个转换。

---

## CAsyncSocket 的异步模型

`CAsyncSocket` 是**非阻塞**的：
- `OnReceive` 回调触发时，数据可能只到了部分
- `Receive` 返回的字节数不确定（可能 1、可能 4096）
- 必须把每次收到的数据**累积**到缓冲区，然后检查是否凑够一条完整消息

---

## 深入理解 1：字节序 —— 为什么 `1` 发过去变成 `16777216`

### 什么是字节序

内存里每个字节有一个地址。4 字节的 `int`，排列顺序有两种：

```
数值: 0x01020304 (十进制 16909060)

大端序 (Big-Endian, 网络字节序):
  地址+0: 0x01  ← 最高位在低地址
  地址+1: 0x02
  地址+2: 0x03
  地址+3: 0x04  ← 最低位在高地址

小端序 (Little-Endian, Intel x86/x64):
  地址+0: 0x04  ← 最低位在低地址
  地址+1: 0x03
  地址+2: 0x02
  地址+3: 0x01  ← 最高位在高地址
```

你的电脑（Intel CPU）是小端序。网络传输用大端序。

### 不转换会怎样

```
你机器上: 256 = 0x00000100
内存里存: 00 01 00 00  (小端序)

send() 原样发出 → 对方收到 "00 01 00 00"
对方按大端序解析: 0x00010000 = 65536 → 消息长度"变成"了 65536！
```

### HostToNet32 是怎么做的

```cpp
inline uint32_t HostToNet32(uint32_t host32) {
    return ((host32 & 0x000000FF) << 24) |   // 把第0字节移到第3字节位置
           ((host32 & 0x0000FF00) << 8)  |   // 把第1字节移到第2字节位置
           ((host32 & 0x00FF0000) >> 8)  |   // 把第2字节移到第1字节位置
           ((host32 & 0xFF000000) >> 24);     // 把第3字节移到第0字节位置
}
```

以 `0x01020304` 为例：

```
输入: 0x01020304 (小端序在内存中是 04 03 02 01)

第1步: host32 & 0x000000FF = 0x00000004
       0x00000004 << 24     = 0x04000000  ← 把第0字节移到第3字节位置

第2步: host32 & 0x0000FF00 = 0x00000300
       0x00000300 << 8      = 0x00030000  ← 把第1字节移到第2字节位置

第3步: host32 & 0x00FF0000 = 0x00020000
       0x00020000 >> 8      = 0x00000200  ← 把第2字节移到第1字节位置

第4步: host32 & 0xFF000000 = 0x01000000
       0x01000000 >> 24     = 0x00000001  ← 把第3字节移到第0字节位置

结果: 0x04030201
经过网络传输，对方按大端序读: 0x04030201 ✓ 和原始值一致！
```

一句话：**小端序机器的 `1` 经由 `HostToNet32` 变成大端序的 `1`，对方就能正确读出来。**

`Winsock` 的 `htonl()` 内部做的是完全一样的事。但自己写一遍位运算，才知道"网络字节序"不是魔法，就是 4 次移位+掩码。

---

## 深入理解 2：粘包 / 半包 —— 为什么 TCP 不能直接当消息用

### 问题演示

```
发送方:
  send("HELLO")          // 5 字节
  send("WORLD")          // 5 字节

网络传输的字节流:
  ....H E L L O W O R L D....

接收方可能遇到的情况:

  情况A (粘包): 一次 recv 收到 "HELLOWORLD" (10字节)
  情况B (半包): 第一次 recv 收到 "HEL" (3字节)
                第二次 recv 收到 "LOWORLD" (7字节)
  情况C (正常): 第一次 recv 收到 "HELLO" (5字节)
                第二次 recv 收到 "WORLD" (5字节)
  情况D (混合): 第一次 recv 收到 "HELLOWO" (8字节)
                第二次 recv 收到 "RLD" (3字节)
```

TCP 只保证**字节顺序正确 + 不丢不重**，不保证**消息边界**。

### 为什么会出现这种情况

TCP 底层有发送缓冲区和接收缓冲区。`send` 只是把数据丢进发送缓冲区，TCP 协议栈决定什么时候、以多大块发出去。中间经过路由器分片、网络延迟、对端接收窗口大小……最终对端的 `recv` 根本不知道原来 `send` 了几次。

### 解决方案：长度前缀分帧

```
消息格式:
┌──────────┬──────────┬──────────────────────┐
│ 4B 类型   │ 4B 长度   │ N 字节 消息体          │
│ (大端序)  │ (大端序)  │                      │
└──────────┴──────────┴──────────────────────┘

接收方逻辑:
  1. 收到数据 → 丢进缓冲区
  2. 缓冲区 ≥ 8 字节? → 读出头部的类型和长度
  3. 缓冲区 ≥ 8 + length 字节? → 取出 length 字节作为一条完整消息
  4. 回到步骤 2，继续检查是否还有完整消息
```

### 没有分帧会怎样

```
客户端 send("Hello") → recv 收到 "HelloWor" → 显示 "HelloWor" ❌
客户端 send("World") → recv 收到 "ld"        → 显示 "ld"      ❌
```

---

## 深入理解 3：消息协议设计

### MsgType 消息类型一览

| 类型 | 值 | 方向 | 说明 |
|------|-----|------|------|
| LOGIN | 0x0001 | C→S | 客户端登录 |
| LOGIN_RESP | 0x0002 | S→C | 服务端返回登录结果 |
| LOGOUT | 0x0003 | C→S | 客户端登出 |
| TEXT | 0x0010 | C→S→C | 单聊消息 |
| GROUP_TEXT | 0x0011 | C→S→多C | 群聊消息 |
| FILE_REQUEST | 0x0020 | C→S→C | 文件传输请求 |
| FILE_RESP | 0x0021 | C→S→C | 文件传输响应 |
| FRIEND_ADD | 0x0030 | C→S | 添加好友 |
| FRIEND_DEL | 0x0031 | C→S | 删除好友 |
| FRIEND_LIST | 0x0032 | C→S→C | 请求好友列表 |
| STATUS_ONLINE | 0x0040 | S→C | 用户上线通知 |
| STATUS_OFFLINE | 0x0041 | S→C | 用户下线通知 |
| HISTORY | 0x0050 | C→S→C | 聊天记录查询 |
| OFFLINE_MSGS | 0x0051 | S→C | 离线消息推送 |
| OFFLINE_READ | 0x0052 | C→S | 标记离线消息已读 |
| ACK | 0xFFFF | 双向 | 通用确认 |

### RecvBuffer 核心逻辑（Protocol.h）

```cpp
bool RecvBuffer::next(uint32_t& outType, uint32_t& outLength, std::string& outJson) {
    if (m_buf.size() < HEADER_SIZE) return false;  // 半包：头部还没收完

    // 解析头部 8 字节（大端序 → 本地序）
    uint32_t typeNet, lengthNet;
    memcpy(&typeNet, m_buf.data(), 4);
    memcpy(&lengthNet, m_buf.data() + 4, 4);
    outType = NetToHost32(typeNet);
    outLength = NetToHost32(lengthNet);

    if (outLength > MAX_MSG_SIZE) { m_buf.clear(); return false; }  // 防恶意数据
    if (m_buf.size() < HEADER_SIZE + outLength) return false;       // 半包：body 还没收完

    // 取出完整消息
    outJson.assign(m_buf.begin() + HEADER_SIZE, m_buf.begin() + HEADER_SIZE + outLength);
    m_buf.erase(m_buf.begin(), m_buf.begin() + HEADER_SIZE + outLength);  // 消费掉
    return true;
}
```

### 使用方式

```cpp
// 不管 TCP 一次送来多少字节，RecvBuffer 帮你切出完整消息
char buf[4096];
int n = socket.Receive(buf, sizeof(buf));
m_recvBuf.append(buf, n);                    // 喂进去

uint32_t type, length;
std::string json;
while (m_recvBuf.next(type, length, json)) { // 吐出来完整消息
    m_dispatcher.dispatch(static_cast<MsgType>(type), json);
}
```
