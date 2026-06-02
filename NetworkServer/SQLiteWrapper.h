/**
 * @file   SQLiteWrapper.h
 * @brief  SQLite数据库封装类，提供用户、消息、好友、文件和离线消息的CRUD操作
 * @author Yan Runxin
 * @date   2026-05-25
 */

#pragma once

#include <sqlite3.h>
#include <string>
#include <utility>
#include <vector>

/**
 * @class  SQLiteWrapper
 * @brief  SQLite数据库操作封装类
 * @details 提供面向即时通讯系统的数据库操作，包括：
 *          - 用户管理（注册、查询）
 *          - 消息存储与历史记录查询
 *          - 好友关系管理
 *          - 文件传输记录
 *          - 离线消息存储
 *
 *          连接初始化时会自动开启：
 *          - 外键约束  (PRAGMA foreign_keys = ON)
 *          - WAL 日志模式，提升并发读写性能
 *          - busy_timeout，避免锁冲突立即失败
 */
class SQLiteWrapper
{
public:
    SQLiteWrapper();
    ~SQLiteWrapper();

    // 禁止拷贝（sqlite3* 不能由两个对象共同持有）
    SQLiteWrapper(const SQLiteWrapper&) = delete;
    SQLiteWrapper& operator=(const SQLiteWrapper&) = delete;

    // ==================== 数据库管理 ====================

    /**
     * @brief       打开数据库连接
     * @param[in]   dbPath  数据库文件路径（UTF-8编码）
     * @return      成功返回true，失败返回false
     * @note        若数据库不存在则自动创建，并初始化所有表结构及连接参数
     */
    bool bOpenDb(const char* dbPath);

    /**
     * @brief  关闭数据库连接
     * @return 成功返回true
     */
    bool bCloseDb();

    // ==================== 用户管理 ====================

    /**
     * @brief       添加用户
     * @param[in]   username  用户名（唯一）
     * @return      成功返回用户ID，失败返回-1
     * @note        若用户名已存在，直接返回该用户的ID
     */
    int iAddUser(const std::string& username);

    /**
     * @brief       根据用户名查询用户ID
     * @param[in]   username  用户名
     * @return      存在返回用户ID，不存在返回-1
     */
    int iGetUserId(const std::string& username);

    /**
     * @brief       根据用户ID查询用户名
     * @param[in]   userId  用户ID
     * @return      成功返回用户名，失败返回空字符串
     */
    std::string strGetUsername(int userId);

    /**
     * @brief       检查用户是否存在（按用户名）
     * @param[in]   username  用户名
     * @return      存在返回true，不存在返回false
     */
    bool bUserExistsByName(const std::string& username);

    /**
     * @brief       检查用户是否存在（按用户ID）
     * @param[in]   userId  用户ID
     * @return      存在返回true，不存在返回false
     */
    bool bUserExistsByID(int userId);

    // ==================== 消息管理 ====================

    /**
     * @brief       保存聊天消息到数据库
     * @param[in]   senderId    发送者用户ID
     * @param[in]   receiverId  接收者用户ID
     * @param[in]   content     消息内容
     * @return      成功返回true，失败返回false
     */
    bool bSaveMessage(int senderId, int receiverId, const std::string& content);

    /**
     * @brief       获取两个用户之间的聊天记录
     * @param[in]   userId       当前用户ID
     * @param[in]   otherUserId  对方用户ID
     * @param[in]   limit        返回消息数量上限
     * @param[out]  results      存储查询结果（格式：[时间] sender->receiver: content）
     * @return      成功返回true，失败返回false
     */
    bool bGetMessages(int userId, int otherUserId, int limit,
                      std::vector<std::string>& results);

    /**
     * @brief       将指定用户之间的未读消息标记为已读
     * @param[in]   senderId    发送者用户ID
     * @param[in]   receiverId  接收者用户ID
     * @return      成功返回true，失败返回false
     */
    bool bMarkMessagesAsRead(int senderId, int receiverId);

    /**
     * @brief       获取两个用户之间的未读消息数量
     * @param[in]   userId       当前用户ID
     * @param[in]   otherUserId  对方用户ID
     * @return      未读消息数量，失败返回-1
     */
    int iGetUnreadCount(int userId, int otherUserId);

    // ==================== 好友管理 ====================

    /**
     * @brief       添加好友关系（事务内双向插入）
     * @param[in]   userId    用户ID
     * @param[in]   friendId  好友用户ID
     * @return      成功返回true，失败返回false（已存在也算成功）
     */
    bool bAddFriend(int userId, int friendId);

    /**
     * @brief       删除好友关系（双向删除）
     * @param[in]   userId    用户ID
     * @param[in]   friendId  好友用户ID
     * @return      成功返回true，失败返回false
     */
    bool bRemoveFriend(int userId, int friendId);

    /**
     * @brief       检查两个用户是否为好友
     * @param[in]   userId    用户ID
     * @param[in]   friendId  好友用户ID
     * @return      是好友返回true，否则返回false
     */
    bool bIsFriend(int userId, int friendId);

    /**
     * @brief       获取用户的好友列表
     * @param[in]   userId  用户ID
     * @return      好友列表，每项为 (friend_id, username) 对
     */
    std::vector<std::pair<int, std::string>> vecGetFriends(int userId);

    // ==================== 文件记录 ====================

    /**
     * @brief       保存文件传输记录
     * @param[in]   senderId    发送者用户ID
     * @param[in]   receiverId  接收者用户ID
     * @param[in]   filename    文件名
     * @param[in]   filesize    文件大小（字节）
     * @param[in]   filepath    文件存储路径
     * @return      成功返回文件记录ID，失败返回-1
     */
    int iSaveFileRecord(int senderId, int receiverId, const std::string& filename,
                        long filesize, const std::string& filepath);

    /**
     * @brief       标记文件已被下载
     * @param[in]   fileId  文件记录ID
     * @return      成功返回true，失败返回false
     */
    bool bUpdateFileDownloaded(int fileId);

    // ==================== 离线消息 ====================

    /**
     * @brief       保存离线消息
     * @param[in]   senderId    发送者用户ID
     * @param[in]   receiverId  接收者用户ID
     * @param[in]   content     消息内容
     * @return      成功返回true，失败返回false
     */
    bool bSaveOfflineMessage(int senderId, int receiverId, const std::string& content);

    /**
     * @brief       获取用户的离线消息
     * @param[in]   userId   用户ID
     * @param[out]  results  存储查询结果（格式：[时间] sender_id->receiver_id: content）
     * @return      成功返回true，失败返回false
     */
    bool bGetOfflineMessages(int userId, std::vector<std::string>& results);

    /**
     * @brief       获取用户未读的离线消息数量
     * @param[in]   userId  用户ID
     * @return      未读消息数量，失败返回-1
     */
    int iGetOfflineMessageCount(int userId);

    /**
     * @brief       删除指定的离线消息
     * @param[in]   msgId  消息ID
     * @return      成功返回true，失败返回false
     */
    bool bDeleteOfflineMessage(int msgId);

private:
    sqlite3*         m_db;    ///< SQLite数据库连接句柄
    CCriticalSection m_csDb;  ///< 保护 m_db 的互斥锁

    // ---- 不加锁的内部实现，调用方必须已持有 m_csDb ----

    /** @brief 打开数据库连接（内部实现，不加锁） */
    bool bOpenDbImpl(const char* dbPath);
    /** @brief 关闭数据库连接（内部实现，不加锁） */
    bool bCloseDbImpl();

    /** @brief 添加用户（内部实现，不加锁） */
    int         iAddUserImpl(const std::string& username);
    /** @brief 根据用户名查询用户ID（内部实现，不加锁） */
    int         iGetUserIdImpl(const std::string& username);
    /** @brief 根据用户ID查询用户名（内部实现，不加锁） */
    std::string strGetUsernameImpl(int userId);
    /** @brief 检查用户是否存在（按用户名，内部实现，不加锁） */
    bool        bUserExistsByNameImpl(const std::string& username);
    /** @brief 检查用户是否存在（按用户ID，内部实现，不加锁） */
    bool        bUserExistsByIDImpl(int userId);

    /** @brief 保存聊天消息（内部实现，不加锁） */
    bool bSaveMessageImpl(int senderId, int receiverId, const std::string& content);
    /** @brief 获取聊天记录（内部实现，不加锁） */
    bool bGetMessagesImpl(int userId, int otherUserId, int limit,
                           std::vector<std::string>& results);
    /** @brief 标记消息为已读（内部实现，不加锁） */
    bool bMarkMessagesAsReadImpl(int senderId, int receiverId);
    /** @brief 获取未读消息数量（内部实现，不加锁） */
    int  iGetUnreadCountImpl(int userId, int otherUserId);

    /** @brief 添加好友关系（内部实现，不加锁） */
    bool bAddFriendImpl(int userId, int friendId);
    /** @brief 删除好友关系（内部实现，不加锁） */
    bool bRemoveFriendImpl(int userId, int friendId);
    /** @brief 检查是否为好友关系（内部实现，不加锁） */
    bool bIsFriendImpl(int userId, int friendId);
    /** @brief 获取好友列表（内部实现，不加锁） */
    std::vector<std::pair<int, std::string>> vecGetFriendsImpl(int userId);

    /** @brief 保存文件传输记录（内部实现，不加锁） */
    int  iSaveFileRecordImpl(int senderId, int receiverId,
                              const std::string& filename, long filesize,
                              const std::string& filepath);
    /** @brief 标记文件已下载（内部实现，不加锁） */
    bool bUpdateFileDownloadedImpl(int fileId);

    /** @brief 保存离线消息（内部实现，不加锁） */
    bool bSaveOfflineMessageImpl(int senderId, int receiverId,
                                  const std::string& content);
    /** @brief 获取离线消息（内部实现，不加锁） */
    bool bGetOfflineMessagesImpl(int userId, std::vector<std::string>& results);
    /** @brief 获取离线消息数量（内部实现，不加锁） */
    int  iGetOfflineMessageCountImpl(int userId);
    /** @brief 删除离线消息（内部实现，不加锁） */
    bool bDeleteOfflineMessageImpl(int msgId);
};
