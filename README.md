# ComputerNetworkProgramming

A C/S instant messaging (IM) system for learning network programming. MFC + Windows Sockets.

```
NetworkServer/   Server (MFC dialog)
NetworkClient/   Client (MFC dialog)
Common/          Shared: Protocol.h, JsonUtils.h
```

## Features

- **Friend management** — add / accept / reject / remove + online/offline status broadcast
- **One-to-one chat** — TEXT message protocol with offline storage
- **Group chat** — `receiver_id=0` broadcasts to all online users
- **Offline messages** — server stores undelivered TEXT, pushes on next login
- **Chat history** — `HISTORY` query, persisted in SQLite
- **File transfer** — sender uploads via FTP (port 2121), receiver downloads; FTPRoot on server side hosts the relay
- **Friend requests** — `FRIEND_ADD` / `FRIEND_ACCEPT` / `FRIEND_REJECT` with a "waiting for approval" state
- **Threading** — BSD socket + `AfxBeginThread` per client, with `delete this` self-destruction
- **Sandboxed FTP** — file ops confined to a configurable root directory

## Build

Requires Windows + Visual Studio 2019 or later (x64, Unicode, MFC dynamic).

Open `ComputerNetworkProgramming.slnx` and build, or via Developer Command Prompt:

```powershell
msbuild ComputerNetworkProgramming.slnx /p:Configuration=Debug /p:Platform=x64
```

Output: `x64/Debug/NetworkServer.exe` and `x64/Debug/NetworkClient.exe`.

## Run

1. Launch `NetworkServer.exe`, click **Start**. Server listens on:
   - **21** (default) — IM protocol (configurable in the dialog)
   - **2121** — FTP relay for file transfer (root: `FtpRoot\` next to the exe; SQLite `chat_history.db` sits in the same directory)
2. Launch one or more `NetworkClient.exe` instances, enter the server IP, port, and a username, then click **Connect**.
3. Add friends, chat, send files.

## Project Layout

```
NetworkServer/
├── NetworkServerDlg       — main dialog (UI, dispatcher, DB)
├── CConnectSocket         — per-client IM socket
├── CListenSocket          — IM listener
├── CFtpListenSocket       — FTP listener
├── CFtpSession            — per-client FTP session
└── SQLiteWrapper          — database access (with m_csDb lock)

NetworkClient/
├── NetworkClientDlg       — main dialog
├── CConnectSocket         — async socket
├── CAddFriendDlg          — add-friend dialog
└── FtpHelper              — WinInet client for FTP upload/download

Common/
├── Protocol.h             — MsgType enum, RecvBuffer, ProtocolDispatcher
└── JsonUtils.h            — hand-rolled JSON parser/builder
```

## Wire Protocol

```
[0..3]   uint32_t  message type (big-endian)
[4..7]   uint32_t  body length  (big-endian, excludes the 8-byte header)
[8..n]   uint8_t[] UTF-8 JSON body
```

Message types (see `Common/Protocol.h`): `LOGIN`, `LOGIN_RESP`, `TEXT`, `GROUP_TEXT`, `HISTORY`, `OFFLINE_MSGS`, `FRIEND_ADD/ACCEPT/REJECT/DEL/LIST`, `FRIEND_ADD_RESP`, `STATUS_ONLINE/OFFLINE`, `FILE_REQUEST`, `FILE_RESP`, `LOGOUT`.

For protocol-level details (header encoding, JSON schema per message), see Doxygen comments in `Common/Protocol.h`.

## Threading Model

- **Server main thread** — UI, starts two listeners
- **IM listener thread** — `accept()`s, creates `CConnectSocket` per client, each in its own `CWinThread`
- **FTP listener thread** — `accept()`s, creates `CFtpSession` per client, each in its own thread
- **Locks** — `m_csData` (memory maps) and `m_csDb` (SQLite handle); nested locking avoided by the `Xxx()` / `XxxImpl()` pattern in `SQLiteWrapper`
- **Cross-thread UI** — log/event posts go through `PostUIUpdate(WM_USER_UPDATE_UI)` rather than direct UI calls

For deeper details, see `KNOWLEDGE_BASE.md` (TCP framing, sticky packets, byte order, CAsyncSocket).

## License

MIT License — free to use, modify, and distribute.

## Learning Path

See `LEARNING_GUIDE.md` for milestone-by-milestone walkthroughs of how this project was built. The companion `KNOWLEDGE_BASE.md` covers the network-programming concepts behind each piece.

## Contributing

This is a teaching project — issues and PRs are welcome, but please open an issue first to discuss the change.
