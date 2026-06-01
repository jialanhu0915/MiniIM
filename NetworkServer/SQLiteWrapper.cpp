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
    StmtGuard() = default;

    explicit StmtGuard(sqlite3_stmt* stmt) noexcept : m_stmt(stmt) {}

    ~StmtGuard() { reset(); }

    // 禁止拷贝
    StmtGuard(const StmtGuard&) = delete;
    StmtGuard& operator=(const StmtGuard&) = delete;

    sqlite3_stmt* get() const noexcept { return m_stmt; }
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
    explicit TransactionGuard(sqlite3* db) noexcept : m_db(db) {
        sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);
    }

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

SQLiteWrapper::SQLiteWrapper() : m_db(nullptr) {}

SQLiteWrapper::~SQLiteWrapper() {
    bCloseDb();
}

// ============================================================================
// 数据库管理
// ============================================================================

bool SQLiteWrapper::bOpenDb(const char* dbPath) {
    CSingleLock lock(&m_csDb, TRUE);
    return bOpenDbImpl(dbPath);
}

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

bool SQLiteWrapper::bCloseDb() {
    CSingleLock lock(&m_csDb, TRUE);
    return bCloseDbImpl();
}

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

int SQLiteWrapper::iAddUser(const std::string& username) {
    CSingleLock lock(&m_csDb, TRUE);
    return iAddUserImpl(username);
}

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

int SQLiteWrapper::iGetUserId(const std::string& username) {
    CSingleLock lock(&m_csDb, TRUE);
    return iGetUserIdImpl(username);
}

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

std::string SQLiteWrapper::strGetUsername(int userId) {
    CSingleLock lock(&m_csDb, TRUE);
    return strGetUsernameImpl(userId);
}

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

bool SQLiteWrapper::bUserExistsByName(const std::string& username) {
    CSingleLock lock(&m_csDb, TRUE);
    return bUserExistsByNameImpl(username);
}

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

bool SQLiteWrapper::bUserExistsByID(int userId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bUserExistsByIDImpl(userId);
}

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

bool SQLiteWrapper::bSaveMessage(int senderId, int receiverId,
                                  const std::string& content) {
    CSingleLock lock(&m_csDb, TRUE);
    return bSaveMessageImpl(senderId, receiverId, content);
}

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

bool SQLiteWrapper::bGetMessages(int userId, int otherUserId, int limit,
                                  std::vector<std::string>& results) {
    CSingleLock lock(&m_csDb, TRUE);
    return bGetMessagesImpl(userId, otherUserId, limit, results);
}

bool SQLiteWrapper::bGetMessagesImpl(int userId, int otherUserId, int limit,
                                     std::vector<std::string>& results) {
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
        int         sender    = sqlite3_column_int(stmt.get(), 0);
        int         receiver  = sqlite3_column_int(stmt.get(), 1);
        const char* content   = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        const char* timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));

        char buf[1024];
        snprintf(buf, sizeof(buf), "[%s] %d -> %d: %s",
                 timestamp ? timestamp : "",
                 sender, receiver,
                 content ? content : "");
        results.push_back(buf);
    }

    return true;
}

bool SQLiteWrapper::bMarkMessagesAsRead(int senderId, int receiverId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bMarkMessagesAsReadImpl(senderId, receiverId);
}

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

int SQLiteWrapper::iGetUnreadCount(int userId, int otherUserId) {
    CSingleLock lock(&m_csDb, TRUE);
    return iGetUnreadCountImpl(userId, otherUserId);
}

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

bool SQLiteWrapper::bAddFriend(int userId, int friendId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bAddFriendImpl(userId, friendId);
}

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

bool SQLiteWrapper::bRemoveFriend(int userId, int friendId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bRemoveFriendImpl(userId, friendId);
}

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

bool SQLiteWrapper::bIsFriend(int userId, int friendId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bIsFriendImpl(userId, friendId);
}

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

std::vector<std::pair<int, std::string>>
SQLiteWrapper::vecGetFriends(int userId) {
    CSingleLock lock(&m_csDb, TRUE);
    return vecGetFriendsImpl(userId);
}

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

int SQLiteWrapper::iSaveFileRecord(int senderId, int receiverId,
                                    const std::string& filename,
                                    long filesize,
                                    const std::string& filepath) {
    CSingleLock lock(&m_csDb, TRUE);
    return iSaveFileRecordImpl(senderId, receiverId, filename, filesize, filepath);
}

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

bool SQLiteWrapper::bUpdateFileDownloaded(int fileId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bUpdateFileDownloadedImpl(fileId);
}

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

bool SQLiteWrapper::bSaveOfflineMessage(int senderId, int receiverId,
                                         const std::string& content) {
    CSingleLock lock(&m_csDb, TRUE);
    return bSaveOfflineMessageImpl(senderId, receiverId, content);
}

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

bool SQLiteWrapper::bGetOfflineMessages(int userId,
                                         std::vector<std::string>& results) {
    CSingleLock lock(&m_csDb, TRUE);
    return bGetOfflineMessagesImpl(userId, results);
}

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

int SQLiteWrapper::iGetOfflineMessageCount(int userId) {
    CSingleLock lock(&m_csDb, TRUE);
    return iGetOfflineMessageCountImpl(userId);
}

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

bool SQLiteWrapper::bDeleteOfflineMessage(int msgId) {
    CSingleLock lock(&m_csDb, TRUE);
    return bDeleteOfflineMessageImpl(msgId);
}

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