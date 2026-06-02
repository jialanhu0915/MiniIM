/**
 * @file   SQLiteWrapper.cpp
 * @brief  SQLite数据库封装类的实现
 * @author Yan Runxin
 * @date   2026-05-25
 */

#include "pch.h"
#include "SQLiteWrapper.h"

#include <cstdio>

// ============================================================================
// 匿名命名空间 — RAII 辅助类，仅本文件可见
// ============================================================================
namespace {

/**
 * @brief sqlite3_stmt 的 RAII 守卫
 * @details 离开作用域时自动调用 sqlite3_finalize，
 *          避免手动 finalize 以及在提前 return / 异常路径上泄漏。
 */
class StmtGuard {
public:
    /** @brief 默认构造 */
    StmtGuard() = default;

    /** @param[in] stmt 初始化的 sqlite3_stmt 句柄 */
    explicit StmtGuard(sqlite3_stmt* stmt) noexcept : m_stmt(stmt) {}

    /** @brief 析构，释放语句资源 */
    ~StmtGuard() { reset(); }

    // 禁止拷贝
    StmtGuard(const StmtGuard&) = delete;
    StmtGuard& operator=(const StmtGuard&) = delete;

    /** @return sqlite3_stmt 句柄 */
    sqlite3_stmt* get() const noexcept { return m_stmt; }

    /** @return 句柄是否有效 */
    bool valid() const noexcept { return m_stmt != nullptr; }

    /**
     * @brief 返回 &m_stmt，用于 sqlite3_prepare_v2 的输出参数
     * @note  调用前会先 finalize 已有语句
     */
    sqlite3_stmt** out() noexcept {
        reset();
        return &m_stmt;
    }

    void reset() noexcept {
        if (m_stmt) {
            sqlite3_finalize(m_stmt);
            m_stmt = nullptr;
        }
    }

private:
    sqlite3_stmt* m_stmt = nullptr;
};

/**
 * @brief 数据库事务的 RAII 守卫
 * @details 构造时执行 BEGIN IMMEDIATE；
 *          析构时若未提交则自动 ROLLBACK。
 */
class TransactionGuard {
public:
    /** @param[in] db sqlite3 数据库句柄，构造时开启事务 */
    explicit TransactionGuard(sqlite3* db) noexcept : m_db(db) {
        sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);
    }

    /** @brief 析构，未提交则自动回滚 */
    ~TransactionGuard() {
        if (!m_committed) {
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
    }

    // 禁止拷贝
    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;

    /**
     * @brief  提交事务
     * @return 成功返回 true；失败时内部已记录，析构仍会尝试 ROLLBACK
     */
    bool commit() noexcept {
        if (m_committed) return false;
        char* err = nullptr;
        if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK) {
            if (err) sqlite3_free(err);
            return false;
        }
        m_committed = true;
        return true;
    }

private:
    sqlite3* m_db;
    bool     m_committed = false;
};

}  // namespace

// ============================================================================
// 构造 / 析构
// ============================================================================

/** @brief 默认构造，句柄置空 */
SQLiteWrapper::SQLiteWrapper() : m_db(nullptr) {}

/** @brief 析构，关闭数据库连接 */
SQLiteWrapper::~SQLiteWrapper() {
    bCloseDb();
}

// ============================================================================
// 数据库管理
// ============================================================================

/**
 * @brief 打开（或创建）SQLite 数据库文件
 * @param[in] dbPath 数据库文件路径
 * @return 成功返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bOpenDb(const char* dbPath) {
    CSingleLock lock(&m_csDb, TRUE);
    return bOpenDbImpl(dbPath);
}

/**
 * @brief 打开数据库实现（不加锁）
 * @param[in] dbPath 数据库文件路径
 * @return 成功返回 true
 */
bool SQLiteWrapper::bOpenDbImpl(const char* dbPath) {
    if (sqlite3_open(dbPath, &m_db) != SQLITE_OK) {
        return false;
    }

    // DB params
    const char* pragmas =
        "PRAGMA foreign_keys = ON;"
        "PRAGMA journal_mode = WAL;"
        "PRAGMA busy_timeout = 5000;"
        "PRAGMA synchronous = NORMAL;"
        ;

    char* errMsg = nullptr;
    if (sqlite3_exec(m_db, pragmas, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        sqlite3_free(errMsg);
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // create tables
    const char* ddl =
        "CREATE TABLE IF NOT EXISTS users ("
        "   user_id    INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   username   TEXT    UNIQUE NOT NULL,"
        "   created_at TEXT    DEFAULT (datetime('now','localtime'))"
        ");"
        "CREATE TABLE IF NOT EXISTS messages ("
        "   msg_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   sender_id   INTEGER NOT NULL,"
        "   receiver_id INTEGER NOT NULL,"
        "   content     TEXT    NOT NULL,"
        "   timestamp   TEXT    DEFAULT (datetime('now','localtime')),"
        "   is_read     INTEGER DEFAULT 0,"
        "   FOREIGN KEY (sender_id)   REFERENCES users(user_id),"
        "   FOREIGN KEY (receiver_id) REFERENCES users(user_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS friends ("
        "   id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   user_id    INTEGER NOT NULL,"
        "   friend_id  INTEGER NOT NULL,"
        "   created_at TEXT    DEFAULT (datetime('now','localtime')),"
        "   FOREIGN KEY (user_id)   REFERENCES users(user_id),"
        "   FOREIGN KEY (friend_id) REFERENCES users(user_id),"
        "   UNIQUE(user_id, friend_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS files ("
        "   file_id       INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   sender_id     INTEGER NOT NULL,"
        "   receiver_id   INTEGER NOT NULL,"
        "   filename      TEXT    NOT NULL,"
        "   filesize      INTEGER NOT NULL,"
        "   filepath      TEXT    NOT NULL,"
        "   timestamp     TEXT    DEFAULT (datetime('now','localtime')),"
        "   is_downloaded INTEGER DEFAULT 0,"
        "   FOREIGN KEY (sender_id)   REFERENCES users(user_id),"
        "   FOREIGN KEY (receiver_id) REFERENCES users(user_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS offline_messages ("
        "   msg_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "   sender_id   INTEGER NOT NULL,"
        "   receiver_id INTEGER NOT NULL,"
        "   content     TEXT    NOT NULL,"
        "   timestamp   TEXT    DEFAULT (datetime('now','localtime')),"
        "   is_read     INTEGER DEFAULT 0,"
        "   FOREIGN KEY (sender_id)   REFERENCES users(user_id),"
        "   FOREIGN KEY (receiver_id) REFERENCES users(user_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_messages_sender   ON messages(sender_id);"
        "CREATE INDEX IF NOT EXISTS idx_messages_receiver ON messages(receiver_id);"
        "CREATE INDEX IF NOT EXISTS idx_friends_user      ON friends(user_id);"
        "CREATE INDEX IF NOT EXISTS idx_offline_receiver  ON offline_messages(receiver_id);";

    if (sqlite3_exec(m_db, ddl, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        sqlite3_free(errMsg);
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    return true;
}

/**
 * @brief 关闭数据库连接
 * @return 成功返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bCloseDb() {
    CSingleLock lock(&m_csDb, TRUE);
    return bCloseDbImpl();
}

/**
 * @brief 关闭数据库实现（不加锁）
 * @return 成功返回 true
 */
bool SQLiteWrapper::bCloseDbImpl() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    return true;
}

// ============================================================================
// 用户管理
// ============================================================================

/**
 * @brief 添加用户
 * @param[in] username 用户名
 * @return 成功返回用户 ID，失败返回 -1
 * @note 线程安全，内部加 m_csDb 锁；若用户已存在则返回已有 ID
 */
int SQLiteWrapper::iAddUser(const std::string& username) {
    CSingleLock lock(&m_csDb, TRUE);
    return iAddUserImpl(username);
}

/**
 * @brief 添加用户实现（不加锁）
 * @param[in] username 用户名
 * @return 成功返回用户 ID，失败返回 -1
 */
int SQLiteWrapper::iAddUserImpl(const std::string& username) {
    // 已存在则直接返回已有 ID
    if (bUserExistsByNameImpl(username)) {
        return iGetUserIdImpl(username);
    }

    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "INSERT INTO users (username) VALUES (?);",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_DONE) {
        return static_cast<int>(sqlite3_last_insert_rowid(m_db));
    }
    return -1;
}

/**
 * @brief 根据用户名查询用户 ID
 * @param[in] username 用户名
 * @return 用户 ID，不存在返回 -1
 * @note 线程安全，内部加 m_csDb 锁
 */
int SQLiteWrapper::iGetUserId(const std::string& username) {
    CSingleLock lock(&m_csDb, TRUE);
    return iGetUserIdImpl(username);
}

/**
 * @brief 根据用户名查询用户 ID 实现（不加锁）
 * @param[in] username 用户名
 * @return 用户 ID，不存在返回 -1
 */
int SQLiteWrapper::iGetUserIdImpl(const std::string& username) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT user_id FROM users WHERE username = ?;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);

    int userId = -1;
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        userId = sqlite3_column_int(stmt.get(), 0);
    }
    return userId;
}

/**
 * @brief 根据用户 ID 查询用户名
 * @param[in] userId 用户 ID
 * @return 用户名，不存在返回空字符串
 * @note 线程安全，内部加 m_csDb 锁
 */
std::string SQLiteWrapper::strGetUsername(int userId) {
    CSingleLock lock(&m_csDb, TRUE);
    return strGetUsernameImpl(userId);
}

/**
 * @brief 根据用户 ID 查询用户名实现（不加锁）
 * @param[in] userId 用户 ID
 * @return 用户名，不存在返回空字符串
 */
std::string SQLiteWrapper::strGetUsernameImpl(int userId) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT username FROM users WHERE user_id = ?;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return "";
    }

    sqlite3_bind_int(stmt.get(), 1, userId);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const char* name =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        if (name) {
            return name;
        }
    }
    return "";
}

/**
 * @brief 检查用户名是否存在
 * @param[in] username 用户名
 * @return 存在返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bUserExistsByName(const std::string& username) {
    CSingleLock lock(&m_csDb, TRUE);
    return bUserExistsByNameImpl(username);
}

/**
 * @brief 检查用户名是否存在实现（不加锁）
 * @param[in] username 用户名
 * @return 存在返回 true
 */
bool SQLiteWrapper::bUserExistsByNameImpl(const std::string& username) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT 1 FROM users WHERE username = ? LIMIT 1;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

/**
 * @brief 检查用户 ID 是否存在
 * @param[in] userId 用户 ID
 * @return 存在返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bUserExistsByID(int userId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bUserExistsByIDImpl(userId);
}

/**
 * @brief 检查用户 ID 是否存在实现（不加锁）
 * @param[in] userId 用户 ID
 * @return 存在返回 true
 */
bool SQLiteWrapper::bUserExistsByIDImpl(int userId) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT 1 FROM users WHERE user_id = ? LIMIT 1;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, userId);
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

// ============================================================================
// 消息管理
// ============================================================================

/**
 * @brief 保存一条聊天消息
 * @param[in] senderId   发送者 ID
 * @param[in] receiverId 接收者 ID
 * @param[in] content   消息内容
 * @return 成功返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bSaveMessage(int senderId, int receiverId,
                                  const std::string& content) {
    CSingleLock lock(&m_csDb, TRUE);
    return bSaveMessageImpl(senderId, receiverId, content);
}

/**
 * @brief 保存消息实现（不加锁）
 * @param[in] senderId   发送者 ID
 * @param[in] receiverId 接收者 ID
 * @param[in] content   消息内容
 * @return 成功返回 true
 */
bool SQLiteWrapper::bSaveMessageImpl(int senderId, int receiverId,
                                     const std::string& content) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "INSERT INTO messages (sender_id, receiver_id, content) "
            "VALUES (?, ?, ?);",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, senderId);
    sqlite3_bind_int(stmt.get(), 2, receiverId);
    sqlite3_bind_text(stmt.get(), 3, content.c_str(), -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

/**
 * @brief 查询两个用户之间的聊天记录
 * @param[in]     userId      查询者 ID
 * @param[in]     otherUserId 对方用户 ID
 * @param[in]     limit       最大返回条数
 * @param[out]    results     结果容器，每条格式为 "[timestamp] sender -> receiver: content"
 * @return 成功返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bGetMessages(int userId, int otherUserId, int limit,
                                  std::vector<MessageRecord>& results) {
    CSingleLock lock(&m_csDb, TRUE);
    return bGetMessagesImpl(userId, otherUserId, limit, results);
}

/**
 * @brief 查询聊天记录实现（不加锁）
 * @param[in]     userId      查询者 ID
 * @param[in]     otherUserId 对方用户 ID
 * @param[in]     limit       最大返回条数
 * @param[out]    results     结果容器
 * @return 成功返回 true
 */
bool SQLiteWrapper::bGetMessagesImpl(int userId, int otherUserId, int limit,
                                     std::vector<MessageRecord>& results) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT sender_id, receiver_id, content, timestamp "
            "FROM messages "
            "WHERE (sender_id = ? AND receiver_id = ?) "
            "   OR (sender_id = ? AND receiver_id = ?) "
            "ORDER BY timestamp ASC "
            "LIMIT ?;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, userId);
    sqlite3_bind_int(stmt.get(), 2, otherUserId);
    sqlite3_bind_int(stmt.get(), 3, otherUserId);
    sqlite3_bind_int(stmt.get(), 4, userId);
    sqlite3_bind_int(stmt.get(), 5, limit);

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        MessageRecord rec;
        rec.senderId   = sqlite3_column_int(stmt.get(), 0);
        rec.receiverId = sqlite3_column_int(stmt.get(), 1);
        const char* pContent   = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        const char* pTimestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        rec.content   = pContent   ? pContent   : "";
        rec.timestamp = pTimestamp ? pTimestamp : "";
        results.push_back(std::move(rec));
    }

    return true;
}

/**
 * @brief 将指定用户之间的未读消息标记为已读
 * @param[in] senderId   发送者 ID
 * @param[in] receiverId 接收者 ID
 * @return 成功返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bMarkMessagesAsRead(int senderId, int receiverId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bMarkMessagesAsReadImpl(senderId, receiverId);
}

/**
 * @brief 标记已读实现（不加锁）
 * @param[in] senderId   发送者 ID
 * @param[in] receiverId 接收者 ID
 * @return 成功返回 true
 */
bool SQLiteWrapper::bMarkMessagesAsReadImpl(int senderId, int receiverId) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "UPDATE messages SET is_read = 1 "
            "WHERE sender_id = ? AND receiver_id = ? AND is_read = 0;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, senderId);
    sqlite3_bind_int(stmt.get(), 2, receiverId);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

/**
 * @brief 获取指定用户收到的未读消息数量
 * @param[in] userId      接收者 ID
 * @param[in] otherUserId 发送者 ID
 * @return 未读消息数量，失败返回 -1
 * @note 线程安全，内部加 m_csDb 锁
 */
int SQLiteWrapper::iGetUnreadCount(int userId, int otherUserId) {
    CSingleLock lock(&m_csDb, TRUE);
    return iGetUnreadCountImpl(userId, otherUserId);
}

/**
 * @brief 获取未读消息数量实现（不加锁）
 * @param[in] userId      接收者 ID
 * @param[in] otherUserId 发送者 ID
 * @return 未读消息数量，失败返回 -1
 */
int SQLiteWrapper::iGetUnreadCountImpl(int userId, int otherUserId) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT COUNT(*) FROM messages "
            "WHERE sender_id = ? AND receiver_id = ? AND is_read = 0;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int(stmt.get(), 1, otherUserId);
    sqlite3_bind_int(stmt.get(), 2, userId);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        return sqlite3_column_int(stmt.get(), 0);
    }
    return -1;
}

// ============================================================================
// 好友管理
// ============================================================================

/**
 * @brief 添加好友关系（双向）
 * @param[in] userId   用户 ID
 * @param[in] friendId 好友 ID
 * @return 成功返回 true；用户不存在或已是好友返回 false
 * @note 线程安全，内部加 m_csDb 锁；使用事务保证原子性
 */
bool SQLiteWrapper::bAddFriend(int userId, int friendId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bAddFriendImpl(userId, friendId);
}

/**
 * @brief 添加好友实现（不加锁）
 * @param[in] userId   用户 ID
 * @param[in] friendId 好友 ID
 * @return 成功返回 true
 */
bool SQLiteWrapper::bAddFriendImpl(int userId, int friendId) {
    if (!bUserExistsByIDImpl(userId) || !bUserExistsByIDImpl(friendId)) {
        return false;
    }
    if (userId == friendId) {
        return false;
    }

    // TransactionGuard：构造 = BEGIN，析构 = 未提交则自动 ROLLBACK
    TransactionGuard txn(m_db);

    const char* sql =
        "INSERT OR IGNORE INTO friends (user_id, friend_id) VALUES (?, ?);";

    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;   // txn 析构 → ROLLBACK
    }

    // (userId, friendId)
    sqlite3_bind_int(stmt.get(), 1, userId);
    sqlite3_bind_int(stmt.get(), 2, friendId);
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        return false;
    }

    // (friendId, userId)
    sqlite3_reset(stmt.get());
    sqlite3_clear_bindings(stmt.get());
    sqlite3_bind_int(stmt.get(), 1, friendId);
    sqlite3_bind_int(stmt.get(), 2, userId);
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        return false;
    }

    // 显式 COMMIT；失败则析构会再补一次 ROLLBACK
    return txn.commit();
}

/**
 * @brief 删除好友关系（双向）
 * @param[in] userId   用户 ID
 * @param[in] friendId 好友 ID
 * @return 成功返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bRemoveFriend(int userId, int friendId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bRemoveFriendImpl(userId, friendId);
}

/**
 * @brief 删除好友实现（不加锁）
 * @param[in] userId   用户 ID
 * @param[in] friendId 好友 ID
 * @return 成功返回 true
 */
bool SQLiteWrapper::bRemoveFriendImpl(int userId, int friendId) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "DELETE FROM friends "
            "WHERE (user_id = ? AND friend_id = ?) "
            "   OR (user_id = ? AND friend_id = ?);",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, userId);
    sqlite3_bind_int(stmt.get(), 2, friendId);
    sqlite3_bind_int(stmt.get(), 3, friendId);
    sqlite3_bind_int(stmt.get(), 4, userId);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

/**
 * @brief 检查两个用户是否为好友
 * @param[in] userId   用户 ID
 * @param[in] friendId 好友 ID
 * @return 是好友返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bIsFriend(int userId, int friendId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bIsFriendImpl(userId, friendId);
}

/**
 * @brief 检查好友关系实现（不加锁）
 * @param[in] userId   用户 ID
 * @param[in] friendId 好友 ID
 * @return 是好友返回 true
 */
bool SQLiteWrapper::bIsFriendImpl(int userId, int friendId) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT 1 FROM friends "
            "WHERE user_id = ? AND friend_id = ? LIMIT 1;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, userId);
    sqlite3_bind_int(stmt.get(), 2, friendId);

    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

/**
 * @brief 获取指定用户的好友列表
 * @param[in] userId 用户 ID
 * @return 向量，每个元素为 (friend_id, username)
 * @note 线程安全，内部加 m_csDb 锁
 */
std::vector<std::pair<int, std::string>>
SQLiteWrapper::vecGetFriends(int userId) {
    CSingleLock lock(&m_csDb, TRUE);
    return vecGetFriendsImpl(userId);
}

/**
 * @brief 获取好友列表实现（不加锁）
 * @param[in] userId 用户 ID
 * @return 向量，每个元素为 (friend_id, username)
 */
std::vector<std::pair<int, std::string>>
SQLiteWrapper::vecGetFriendsImpl(int userId) {
    std::vector<std::pair<int, std::string>> result;

    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT u.user_id, u.username "
            "FROM friends f "
            "JOIN users u ON f.friend_id = u.user_id "
            "WHERE f.user_id = ?;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_int(stmt.get(), 1, userId);

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        int         id   = sqlite3_column_int(stmt.get(), 0);
        const char* name = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt.get(), 1));
        result.emplace_back(id, name ? name : "");
    }

    return result;
}

// ============================================================================
// 文件记录
// ============================================================================

/**
 * @brief 保存文件传输记录
 * @param[in] senderId   发送者 ID
 * @param[in] receiverId 接收者 ID
 * @param[in] filename   文件名
 * @param[in] filesize   文件大小（字节）
 * @param[in] filepath   文件存储路径
 * @return 成功返回记录 ID，失败返回 -1
 * @note 线程安全，内部加 m_csDb 锁
 */
int SQLiteWrapper::iSaveFileRecord(int senderId, int receiverId,
                                    const std::string& filename,
                                    long filesize,
                                    const std::string& filepath) {
    CSingleLock lock(&m_csDb, TRUE);
    return iSaveFileRecordImpl(senderId, receiverId, filename, filesize, filepath);
}

/**
 * @brief 保存文件记录实现（不加锁）
 * @param[in] senderId   发送者 ID
 * @param[in] receiverId 接收者 ID
 * @param[in] filename   文件名
 * @param[in] filesize   文件大小
 * @param[in] filepath   文件路径
 * @return 成功返回记录 ID，失败返回 -1
 */
int SQLiteWrapper::iSaveFileRecordImpl(int senderId, int receiverId,
                                       const std::string& filename,
                                       long filesize,
                                       const std::string& filepath) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "INSERT INTO files (sender_id, receiver_id, filename, filesize, filepath) "
            "VALUES (?, ?, ?, ?, ?);",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int(stmt.get(),  1, senderId);
    sqlite3_bind_int(stmt.get(),  2, receiverId);
    sqlite3_bind_text(stmt.get(), 3, filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 4, static_cast<sqlite3_int64>(filesize));
    sqlite3_bind_text(stmt.get(), 5, filepath.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_DONE) {
        return static_cast<int>(sqlite3_last_insert_rowid(m_db));
    }
    return -1;
}

/**
 * @brief 标记文件记录为已下载
 * @param[in] fileId 文件记录 ID
 * @return 成功返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bUpdateFileDownloaded(int fileId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bUpdateFileDownloadedImpl(fileId);
}

/**
 * @brief 标记已下载实现（不加锁）
 * @param[in] fileId 文件记录 ID
 * @return 成功返回 true
 */
bool SQLiteWrapper::bUpdateFileDownloadedImpl(int fileId) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "UPDATE files SET is_downloaded = 1 WHERE file_id = ?;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, fileId);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ============================================================================
// 离线消息
// ============================================================================

/**
 * @brief 保存离线消息
 * @param[in] senderId   发送者 ID
 * @param[in] receiverId 接收者 ID
 * @param[in] content    消息内容
 * @return 成功返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bSaveOfflineMessage(int senderId, int receiverId,
                                         const std::string& content) {
    CSingleLock lock(&m_csDb, TRUE);
    return bSaveOfflineMessageImpl(senderId, receiverId, content);
}

/**
 * @brief 保存离线消息实现（不加锁）
 * @param[in] senderId   发送者 ID
 * @param[in] receiverId 接收者 ID
 * @param[in] content    消息内容
 * @return 成功返回 true
 */
bool SQLiteWrapper::bSaveOfflineMessageImpl(int senderId, int receiverId,
                                             const std::string& content) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "INSERT INTO offline_messages (sender_id, receiver_id, content) "
            "VALUES (?, ?, ?);",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(),  1, senderId);
    sqlite3_bind_int(stmt.get(),  2, receiverId);
    sqlite3_bind_text(stmt.get(), 3, content.c_str(), -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

/**
 * @brief 获取指定用户的所有离线消息
 * @param[in]     userId   接收者 ID
 * @param[out]    results  结果容器，每条格式为 "[timestamp] sender -> receiver: content"
 * @return 成功返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bGetOfflineMessages(int userId,
                                         std::vector<std::string>& results) {
    CSingleLock lock(&m_csDb, TRUE);
    return bGetOfflineMessagesImpl(userId, results);
}

/**
 * @brief 获取离线消息实现（不加锁）
 * @param[in]     userId   接收者 ID
 * @param[out]    results  结果容器
 * @return 成功返回 true
 */
bool SQLiteWrapper::bGetOfflineMessagesImpl(int userId,
                                              std::vector<std::string>& results) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT msg_id, sender_id, receiver_id, content, timestamp "
            "FROM offline_messages "
            "WHERE receiver_id = ? "
            "ORDER BY timestamp ASC;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, userId);

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        int         sender    = sqlite3_column_int(stmt.get(), 1);
        int         receiver  = sqlite3_column_int(stmt.get(), 2);
        const char* content   = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        const char* timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));

        char buf[1024];
        snprintf(buf, sizeof(buf), "[%s] %d -> %d: %s",
                 timestamp ? timestamp : "",
                 sender, receiver,
                 content ? content : "");
        results.push_back(buf);
    }

    return true;
}

/**
 * @brief 获取指定用户的离线未读消息数量
 * @param[in] userId 接收者 ID
 * @return 未读离线消息数量，失败返回 -1
 * @note 线程安全，内部加 m_csDb 锁
 */
int SQLiteWrapper::iGetOfflineMessageCount(int userId) {
    CSingleLock lock(&m_csDb, TRUE);
    return iGetOfflineMessageCountImpl(userId);
}

/**
 * @brief 获取离线未读消息数量实现（不加锁）
 * @param[in] userId 接收者 ID
 * @return 未读离线消息数量，失败返回 -1
 */
int SQLiteWrapper::iGetOfflineMessageCountImpl(int userId) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT COUNT(*) FROM offline_messages "
            "WHERE receiver_id = ? AND is_read = 0;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int(stmt.get(), 1, userId);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        return sqlite3_column_int(stmt.get(), 0);
    }
    return -1;
}

/**
 * @brief 删除指定的离线消息
 * @param[in] msgId 离线消息 ID
 * @return 成功返回 true
 * @note 线程安全，内部加 m_csDb 锁
 */
bool SQLiteWrapper::bDeleteOfflineMessage(int msgId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bDeleteOfflineMessageImpl(msgId);
}

/**
 * @brief 删除离线消息实现（不加锁）
 * @param[in] msgId 离线消息 ID
 * @return 成功返回 true
 */
bool SQLiteWrapper::bDeleteOfflineMessageImpl(int msgId) {
    StmtGuard stmt;
    if (sqlite3_prepare_v2(m_db,
            "DELETE FROM offline_messages WHERE msg_id = ?;",
            -1, stmt.out(), nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt.get(), 1, msgId);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}