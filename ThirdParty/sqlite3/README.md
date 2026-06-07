# ThirdParty / sqlite3

本地内置的 SQLite 库，避免工程依赖本机 `B:\Miniconda\Library\...` 绝对路径。

## 当前版本

- SQLite **3.51.0**
- 来源：`B:\Miniconda\Library\`（conda 包 `libsqlite-3.52.0` 提供，3.51.0 是其内嵌的 SQLite 库版本）
- 仅 x64。Win32 平台未提供（项目实际只编 x64）

## 目录结构

```
ThirdParty/sqlite3/
├── include/
│   └── sqlite3.h        # C 头文件
└── lib/
    └── x64/
        └── sqlite3.lib  # x64 静态链接库
```

## 升级方法

1. 拿到新的 `sqlite3.h` 和 `sqlite3.lib`（可用 `vcpkg install sqlite3:x64-windows-static` 或 conda）
2. 覆盖 `include/sqlite3.h` 与 `lib/x64/sqlite3.lib`
3. 在 sqlite 官网 https://www.sqlite.org/changes.html 核对 RELEASE 列表，更新本文件顶部的版本号
