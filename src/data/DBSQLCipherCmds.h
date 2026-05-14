#pragma once
#include "headers.h"

// 类型声明等

// sqlcipher taotie_chat_server.db
// > PRAGMA key='your_secure_encryption_key'; # 密码
// > SELECT * FROM meta; # `1|version|1.0|`
// > .exit

namespace SQL{
    namespace COMMANDS {
        // SQL 语句模板
        // constexpr std::string_view SQL_SET_PRAGMA_KEY      = "PRAGMA key = '{}';";  // 字符串替换会导致数据库注入，但加密语句不支持数据库绑定

        // SQLite 中的 DATETIME 类型实际上是一个文本类型，存储格式为 "YYYY-MM-DD HH:MM:SS"(UTC)。

        // SQLCipher 数据库相关语句
        // 不包含参数绑定的语句
        namespace PRAGMA{
            // 数据库设置语句，用于配置数据库的性能和安全性
            constexpr std::string_view SQL_PRAGMA_SETTINGS = R"(
                -- 设置 WAL 模式，提升并发性能
                PRAGMA journal_mode = WAL;

                -- 设置锁定模式为独占，减少锁竞争，提高性能
                -- PRAGMA locking_mode = EXCLUSIVE;

                -- 启用自动清理，保持数据库文件大小合理
                -- PRAGMA auto_vacuum = FULL;

                -- 设置页面大小，提升性能和安全性
                -- PRAGMA cipher_page_size = 4096;

                -- 设置兼容模式，确保与 SQLCipher 4.x 版本兼容
                -- PRAGMA cipher_default_compatibility = 4;

                -- 设置同步模式，提升性能的同时保持合理的数据安全性
                -- PRAGMA synchronous = NORMAL;

                -- 将临时存储设置为内存，提升性能
                -- PRAGMA temp_store = MEMORY;

                -- 启用安全删除，确保删除的数据无法恢复
                -- PRAGMA secure_delete = ON;

                -- 设置 HMAC 算法，提升安全性
                -- PRAGMA cipher_hmac_algorithm = HMAC_SHA512;

                -- 设置 KDF 算法，提升安全性
                -- PRAGMA cipher_kdf_algorithm = PBKDF2_HMAC_SHA512;
            )";

            // 数据库查询语句，用于获取数据库的元数据和配置信息
            constexpr std::string_view SQL_PRAGMA_QUERY = R"(
                -- 获取 SQLCipher 版本信息"
                PRAGMA cipher_version;

                -- 获取 SQLCipher 源代码 ID"
                PRAGMA cipher_source_id;

                -- 获取 SQLCipher 版本号"
                PRAGMA cipher_libversion_number;

                -- 获取 SQLCipher 编译选项"
                PRAGMA compile_options;

                -- 检查 SQLCipher 是否线程安全"
                PRAGMA threadsafe;

                -- 获取 SQLCipher 当前使用的内存量"
                PRAGMA memory_used;

                -- 获取 SQLCipher 关键字数量"
                PRAGMA keyword_count;

                -- 初始化 SQLCipher"
                -- PRAGMA initialize;

                -- 关闭 SQLCipher"
                -- PRAGMA shutdown;

                -- 初始化 SQLCipher 的操作系统接口"
                -- PRAGMA os_init;

                -- 关闭 SQLCipher 的操作系统接口"
                -- PRAGMA os_end;
            )";
        }

        // 创建
        namespace CREATE{
            // 元数据
            constexpr std::string_view SQL_CREATE_TABLE_META = R"(
                CREATE TABLE IF NOT EXISTS meta (
                    id                INTEGER PRIMARY KEY AUTOINCREMENT,
                    name              TEXT,
                    value             TEXT,
                    common            TEXT
                );
            )";

            // 用户账号信息
            constexpr std::string_view SQL_CREATE_TABLE_USERS = R"(
                CREATE TABLE IF NOT EXISTS users (
                    -- 主键与唯一标识
                    id                INTEGER PRIMARY KEY AUTOINCREMENT,
                    account_id        TEXT NOT NULL UNIQUE,  -- 系统生成，永久唯一
                    user_id           TEXT NOT NULL,         -- 用户可改，在有效用户中逻辑唯一

                    -- 状态字段：0:正常, 1:锁定, 2:已注销
                    status            INTEGER DEFAULT 0,

                    -- 认证信息
                    password_hash     TEXT NOT NULL,         -- 密码哈希

                    -- 用户属性
                    nickname          TEXT,                  -- 昵称
                    avatar_id         TEXT,                  -- 头像 ID
                    sign              TEXT,                  -- 个性签名
                    desc              TEXT,                  -- 个人简介
                    gender            INTEGER DEFAULT 0,     -- 0:未设置, 1:男, 2:女, 3:保密
                    region            TEXT,                  -- 建议存区域代码 (Code)
                    birthday          TEXT,                  -- 格式: YYYY-MM-DD

                    -- 联系方式
                    phones            TEXT,                  -- 建议存 JSON 数组或独立表  -- 暂定为以 `;` 划分的字符串数组
                    emails            TEXT,                  -- 建议存 JSON 数组或独立表  -- 暂定为以 `;` 划分的字符串数组
                    social_links      TEXT,                  -- 建议存 JSON 数组或独立表  -- 暂定为以 `;` 划分的字符串数组

                    -- 时间戳
                    register_time     DATETIME DEFAULT (datetime('now')),  -- 注册时间
                    update_time       DATETIME DEFAULT (datetime('now')),  -- 更新时间
                    deactivate_time   DATETIME,              -- 账号停用/冻结/注销时间

                    -- 安全与扩展
                    security_question    TEXT,                  -- 安全问题
                    security_answer_hash TEXT,                  -- 安全答案
                    common               TEXT                   -- 用于存储扩展的 JSON 配置
                );
            )";

            // 创建用户账户索引
            constexpr std::string_view SQL_CREATE_INDEX_ACCOUNT_ID = R"(
                CREATE INDEX IF NOT EXISTS idx_account_id ON users (account_id);
            )";

            // 创建用户 ID 索引
            constexpr std::string_view SQL_CREATE_INDEX_USER_ID    = R"(
                CREATE INDEX IF NOT EXISTS idx_user_id    ON users (user_id   );
            )";

            // 补充触发器：确保每次 update 记录时，update_time 都会自动变为当前时间
            constexpr std::string_view SQL_CREATE_TRIGGER_UPDATE = R"(
                CREATE TRIGGER IF NOT EXISTS trg_users_update_time
                AFTER UPDATE ON users
                BEGIN
                    UPDATE users SET update_time = datetime('now') WHERE id = NEW.id;
                END;
            )";

            // 用户登录令牌（刷新令牌与访问令牌）
            constexpr std::string_view SQL_CREATE_TABLE_TOKENS = R"(
                CREATE TABLE IF NOT EXISTS user_tokens (
                    id                INTEGER PRIMARY KEY AUTOINCREMENT,

                    -- 关联 users 表的 account_id (系统生成且不可变的唯一 ID)
                    account_id        TEXT NOT NULL,

                    -- 存储 Token 的哈希值（SHA-256），避免明文泄露
                    token_hash        TEXT NOT NULL,

                    -- 令牌类型：如 "access" (访问令牌), "refresh" (刷新令牌)
                    token_type        TEXT DEFAULT 'access',

                    -- 令牌状态：0:有效, 1:已撤销(手动退出), 2:因注销失效
                    token_status      INTEGER DEFAULT 0,

                    -- 设备标识：如 "iPhone 15", "Chrome-Windows", "Android-App"
                    device_info       TEXT,

                    -- 最后使用的 IP 地址
                    last_ip           TEXT,

                    -- 时间戳
                    create_time       DATETIME DEFAULT (datetime('now')),              -- 创建时间
                    expire_time       DATETIME DEFAULT (datetime('now', '+30 days')),  -- 过期时间

                    -- 用于存储扩展的 JSON 配置
                    common            TEXT,

                    -- 级联删除：当用户从 users 表注销(DELETE)时，该用户的所有 token 自动清理
                    -- FOREIGN KEY (account_id) REFERENCES users(account_id) ON DELETE CASCADE

                    -- 仅做关联，不设置级联删除，以保留注销后的历史轨迹
                    FOREIGN KEY (account_id) REFERENCES users(account_id)
                );
            )";

            // 为 token_hash 创建唯一索引，确保验证速度达到 O(1)
            constexpr std::string_view SQL_CREATE_INDEX_TOKEN = R"(
                CREATE UNIQUE INDEX IF NOT EXISTS idx_token_lookup ON user_tokens(token_hash);
            )";

            // 为 account_id 创建普通索引，方便快速查询某个用户名下的所有登录设备
            constexpr std::string_view SQL_CREATE_INDEX_USER_SESSIONS = R"(
                CREATE INDEX IF NOT EXISTS idx_user_sessions ON user_tokens(account_id);
            )";

            // // 用户注销：标记用户状态为已注销，并使其所有 Token 失效
            // constexpr std::string_view SQL_USER_UNREGISTER = R"(
            //     -- 1. 标记用户注销
            //     UPDATE users SET status = 2, unregister_time = datetime('now') WHERE account_id = :account_id;
            //
            //     -- 2. 使该用户所有 Token 立即失效（逻辑清理）
            //     UPDATE user_tokens SET token_status = 2 WHERE account_id = :account_id;
            // )";

            // 好友关系 // 好友的个人信息不进行缓存
            constexpr std::string_view SQL_CREATE_TABLE_FRIENDS = R"(
                CREATE TABLE IF NOT EXISTS friends (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    account_id        TEXT,  -- 当前用户的账号 ID
                    friend_account_id TEXT,  -- friend_account_id 好友的账号 ID

                    -- 时间戳为辅助验证，状态码为主要依据
                    -- 状态字段：0:非好友关系, 1:已经是好友关系, 2:被定向拉黑
                    --           10:好友请求待处理, 11:好友请求被拒绝
                    status            INTEGER DEFAULT 0,

                    -- 关系属性
                    starred           BOOLEAN,   -- starred 好友关系的星标
                    silent            BOOLEAN,   -- silent 好友关系的静音
                    notes             TEXT,      -- notes 对该好友的备注
                    tags              TEXT,      -- tags 好友关系的标签
                    added_source      TEXT,      -- added_source 好友关系的来源

                    -- 时间戳  -- SQLite 中的 DATETIME 类型实际上是一个文本类型，存储格式为 "YYYY-MM-DD HH:MM:SS"(UTC)
                    added_time            DATETIME DEFAULT (datetime('now')),  -- added_time 添加为好友的时间
                    -- added_time_str     TEXT,      -- added_time 添加为好友的时间
                    removed_time          DATETIME,  -- removed_time 删除好友的时间
                    -- removed_time_str   TEXT,      -- removed_time 删除好友的时间

                    -- pin            BOOLEAN,   -- pin 好友关系的置顶     -- 应当存储在聊天列表中

                    common            TEXT       -- common 通用字段
                );
            )";

            // // 群组信息  // 暂定，后续根据实际需求调整
            // constexpr std::string_view SQL_CREATE_TABLE_GROUPS = R"(
            //     CREATE TABLE IF NOT EXISTS groups (
            //         id INTEGER PRIMARY KEY AUTOINCREMENT,
            //         group_id    TEXT,  -- group_id 群 ID，唯一，注册时由系统生成，不可被用户修改，销号时不回收
            //         name        TEXT,  -- name 群名称
            //         avatar_id   TEXT,  -- avatar_id 群头像 ID
            //         owner_id    TEXT,  -- owner_id 群主的账号 ID
            //         member_ids  TEXT,  -- member_ids 群成员的账号 ID 列表，逗号分隔
            //         create_time DATETIME DEFAULT (datetime('now')),  -- create_time 创建时间
            //         update_time DATETIME DEFAULT (datetime('now')),  -- update_time 更新时间
            //         common      TEXT   -- common 通用字段
            //     );
            // )";

            // 聊天记录
            // 一对一的聊天记录和群聊记录都存储在同一个表中，
            // 通过 receiver_id 来区分是单聊还是群聊，
            // 单聊时 receiver_id 是好友的账号 ID，群聊时 receiver_id 是群 ID。
            constexpr std::string_view SQL_CREATE_TABLE_MESSAGES = R"(
                CREATE TABLE IF NOT EXISTS messages (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,

                    -- server_message_id 服务端生成的全局唯一的消息 ID，可用于消息同步
                    server_message_id   TEXT NOT NULL,

                    -- client_message_id 客户端发送消息时生成的消息 ID，可用于消息去重（防止重复发送）
                    client_message_id   TEXT NOT NULL,

                    -- sender_account_id 发送者的账号 ID
                    sender_account_id   TEXT NOT NULL,

                    -- receiver_account_id 接收者的账号 ID，单聊时为好友的账号 ID，群聊时为群 ID
                    receiver_account_id TEXT NOT NULL,

                    -- short_text 短文本消息内容直接存储在该字段，适用于 [0, 8192) 个字节以内的文本消息内容
                    -- 短文本内容不允许为空，为空时认为发送的是大文件或长文本消息
                    short_text          TEXT,

                    -- big_context_id 长文本消息及文件内容的 ID，
                    -- 指向存储长文本消息内容的表中的记录 ID，
                    -- 适用于 [8192, ...) 个字节以上的文本消息内容或文件
                    -- 建议存储 sha 值以便去重
                    big_context_id    TEXT,

                    timestamp         TEXT NOT NULL,  -- timestamp 消息时间戳  -- 需要返回该值，以便客户端进行消息排序和去重
                    common            TEXT   -- common 通用字段
                );
            )";

            // // 群聊消息记录 // 群聊消息记录表可以存储一些特定于群聊的信息，如 @提及 的用户列表等
            // // 也可以不单独存储，而是在 messages 表中通过 content 字段
            // // 来存储一个 JSON 对象，包含消息内容和一些额外的元信息，这样可以避免表结构的复杂化，同时也更灵活地适应未来可能出现的需求变化。
            // constexpr std::string_view SQL_CREATE_TABLE_GROUP_MESSAGES =
            //     R"(CREATE TABLE IF NOT EXISTS group_messages ()"
            //     R"(    id INTEGER PRIMARY KEY AUTOINCREMENT,)"
            //     R"(    message_id TEXT,)"  // message_id 消息 ID，对于消息唯一
            //     R"(    sender_id  TEXT,)"  // sender_id 发送者的账号 ID
            //     R"(    group_id   TEXT,)"  // group_id 群 ID
            //     R"(    content    TEXT,)"  // content 消息内容，存储一个 JSON 对象，包含消息内容和一些额外的元信息，如 @提及 的用户列表等
            //     R"(    type       TEXT,)"  // type 消息类型，如文本、图片、文件等
            //     R"(    timestamp  TEXT,)"  // timestamp 消息时间戳
            //     R"(    common     TEXT)"   // common 通用字段
            //     R"();)";

            // 待处理的添加好友申请
            constexpr std::string_view SQL_CREATE_TABLE_FRIEND_REQUESTS = R"(
                CREATE TABLE IF NOT EXISTS friend_requests (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    -- request_account_id 发起好友请求的账号 ID
                    request_account_id TEXT NOT NULL,

                    -- target_account_id 接收好友请求的账号 ID
                    target_account_id  TEXT NOT NULL,

                    -- status 好友请求状态：0:待处理, 1:已接受, 2:已拒绝, 3:已忽略(不再提醒)
                    status             INTEGER NOT NULL,

                    -- timestamp 好友请求时间戳
                    timestamp          DATETIME DEFAULT (datetime('now')),

                    -- message 好友请求附带的消息
                    message            TEXT,

                    -- reply 好友请求附带的回复消息
                    reply              TEXT,

                    -- common 通用字段
                    common             TEXT
                );
            )";

            // 文件 metadata 表，包含唯一的文件 ID、权限 ID、文件名、文件大小、文件内容的 MD5
            constexpr std::string_view SQL_CREATE_TABLE_FILE_METADATAS = R"(
                CREATE TABLE IF NOT EXISTS file_metadatas (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    -- file_id 文件 ID，唯一，注册时由系统生成，不可被用户修改，销号时不回收
                    file_id    TEXT NOT NULL UNIQUE,

                    -- file_name 文件名，包含扩展名
                    file_name   TEXT NOT NULL,

                    -- file_size 文件大小，单位字节
                    file_size   INTEGER NOT NULL,

                    -- file_md5 文件内容的 MD5 哈希值，用于校验文件完整性和去重
                    file_md5   TEXT NOT NULL,

                    -- file_status 文件状态：0:未设置, 1:已上传, 2:上传中, 3: 不存在
                    file_status INTEGER DEFAULT 0,

                    -- upload_time 文件上传时间戳
                    upload_time DATETIME DEFAULT (datetime('now')),

                    -- common 通用字段
                    common      TEXT
                );
            )";
            // 为 file_md5 创建索引，提升查询性能
            constexpr std::string_view SQL_CREATE_INDEX_FILE_METADATAS_FILE_MD5 = R"(
                CREATE INDEX IF NOT EXISTS idx_file_metadatas_file_md5 ON file_metadatas(file_md5);
            )";
            // 文件读取权限关联信息表，包含文件 ID、账号 ID、访问权限类型、访问权限值等信息
            constexpr std::string_view SQL_CREATE_TABLE_FILE_PERMISSIONS = R"(
                CREATE TABLE IF NOT EXISTS file_permissions (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    -- file_id 文件 ID，关联 file_metadatas 表的 file_id
                    file_id    TEXT NOT NULL,

                    -- account_id 账号 ID，文件所属的账号 ID
                    account_id TEXT NOT NULL,

                    -- permission_code 访问权限
                    permission_code INTEGER DEFAULT 0,  -- 0:无权限

                    -- update_time 权限更新时间戳
                    update_time DATETIME DEFAULT (datetime('now')),

                    -- common 通用字段
                    common      TEXT,

                    -- 必须添加这一行：定义联合唯一约束，
                    -- 即 (file_id, account_id) 的组合必须唯一，
                    -- 确保同一个账号 ID 对同一个文件 ID 只能有一条权限记录，避免权限冲突和冗余
                    UNIQUE(file_id, account_id),

                    -- 外键约束，确保 file_id 的有效性
                    FOREIGN KEY (file_id) REFERENCES file_metadatas(file_id)
                );
            )";
            // 为 file_id 创建索引，提升查询性能
            constexpr std::string_view SQL_CREATE_INDEX_FILE_PERMISSIONS_FILE_ID = R"(
                CREATE INDEX IF NOT EXISTS idx_file_permissions_file_id ON file_permissions(file_id);
            )";
            // // 创建文件 md5 与实际路径的映射表，包含文件 MD5、文件存储路径等信息
            // constexpr std::string_view SQL_CREATE_TABLE_FILE_STORAGE = R"(
            //     CREATE TABLE IF NOT EXISTS file_storage (
            //         id INTEGER PRIMARY KEY AUTOINCREMENT,
            //         -- file_md5 文件内容的 MD5 哈希值，唯一，注册时由系统生成，不可被用户修改，销号时不回收
            //         file_md5   TEXT NOT NULL UNIQUE,

            //         -- file_path 文件存储路径，指向实际存储文件的路径或 URL
            //         file_path   TEXT NOT NULL,

            //         -- update_time 更新时间戳
            //         update_time DATETIME DEFAULT (datetime('now')),

            //         -- common 通用字段
            //         common      TEXT
            //     );
            // )";

            // 事件发生时进行记录及最新计数的更新

            // 创建 pts 记录表，用于记录每个用户的最新消息同步点
            // 事件发生时记录类型与计数，并填充事件对应的 ID
            constexpr std::string_view SQL_CREATE_TABLE_PTS_RECORDS = R"(
                CREATE TABLE IF NOT EXISTS pts (
                    id           INTEGER PRIMARY KEY AUTOINCREMENT,   -- 记录 ID
                    account_id   TEXT NOT NULL UNIQUE,                -- 账号 ID
                    pts_type     TEXT NOT NULL,                       -- 事件主类型 -- 用于客户端更新对比
                    pts_sub_type TEXT NOT NULL,                       -- 事件子类型 -- 用于指明查找位置
                    pts_value    INTEGER DEFAULT 0,                   -- 事件序号   -- 自增 ID
                    pts_event_id TEXT NOT NULL,                       -- 事件 ID    -- 事件对应的全局唯一 ID，如消息 ID 等
                    update_time  DATETIME DEFAULT (datetime('now')),  -- 更新时间
                    common       TEXT                                 -- common 通用字段
                );
            )";

            // 创建 pts 记录统计表，用于统计每个用户的不同类型事件的最新序号
            // 事件发生时直接更新对应类型的序号，无需填充事件 ID
            constexpr std::string_view SQL_CREATE_TABLE_PTS_STATS = R"(
                CREATE TABLE IF NOT EXISTS pts_stats (
                    id          INTEGER PRIMARY KEY AUTOINCREMENT,   -- 记录 ID
                    account_id  TEXT NOT NULL UNIQUE,                -- 账号 ID
                    pts_type    TEXT NOT NULL,                       -- 事件类型
                    pts_value   INTEGER DEFAULT 0,                   -- 事件序号  -- 对用户的自增 ID
                    update_time DATETIME DEFAULT (datetime('now')),  -- 更新时间
                    common      TEXT                                 -- common 通用字段
                );
            )";
        }

        // 在有索引的情况下，SQL 多条件查找会先过滤前面的条件，
        // 在过滤结果中再进行后面条件的判断，每个条件过滤的时间为 O(Log N)
        // 优先推荐使用复杂的 SQL 语句，而不是手动比对

        // 查询
        namespace QUERY {
            // 内部需要转换 user_id 为 account_id，使用 account_id 进行查询

            // 查询数据版本
            constexpr std::string_view SQL_QUERY_META_VERSION      = R"(SELECT name, value FROM meta where name = 'version' LIMIT 1;)";

            // 用户注册时搜索 account_id 是否可用 // account_id 全局唯一，要求包含停用或已经注销的账号 ID  // 用户注册时使用该筛选条件
            constexpr std::string_view SQL_QUERY_EXIST_ACCOUNTID         = R"(SELECT 1          FROM users WHERE account_id = :account_id                 LIMIT 10;)";
            // 搜索在用的 account_id
            constexpr std::string_view SQL_QUERY_EXIST_ACTIVE_ACCOUNTID  = R"(SELECT 1          FROM users WHERE account_id = :account_id  AND status = 0 LIMIT 10;)";
            // 通过账号 ID 查询用户 ID  // 包含停用的账号
            constexpr std::string_view SQL_QUERY_GET_USERID              = R"(SELECT user_id    FROM users WHERE account_id = :account_id                  LIMIT 10;)";
            // 通过账号 ID 查询用户 ID  // 查询状态正常的用户 ID
            constexpr std::string_view SQL_QUERY_GET_ACTIVE_USERID       = R"(SELECT user_id    FROM users WHERE account_id = :account_id   AND status = 0 LIMIT 10;)";
            // 通过账号 ID 查询用户信息
            constexpr std::string_view SQL_QUERY_GET_ACCOUNT_INFO        = R"(SELECT *          FROM users WHERE account_id = :account_id                  LIMIT 10;)";
            // 通过账号 ID 查询用户信息
            constexpr std::string_view SQL_QUERY_GET_ACTIVE_ACCOUNT_INFO = R"(SELECT *          FROM users WHERE account_id = :account_id   AND status = 0 LIMIT 10;)";

            // 用户注册时搜索 user_id 是否可用  // 仅包含正在使用的用户 ID
            constexpr std::string_view SQL_QUERY_EXIST_ACTIVE_USER_ID    = R"(SELECT 1          FROM users WHERE user_id    = :user_id    AND status = 0 LIMIT 10;)";
            // 通过用户 ID 查询账号 ID  // 包含停用的用户 ID，数量可以无限多
            constexpr std::string_view SQL_QUERY_GET_ACCOUNT_ID          = R"(SELECT account_id FROM users WHERE user_id    = :user_id                   LIMIT 10;)";
            // 通过用户 ID 查询账号 ID  // 统一将 user_id 转换为 account_id  // 在用账号的用户 ID 唯一，一般只返回一条数据
            constexpr std::string_view SQL_QUERY_GET_ACTIVE_ACCOUNT_ID   = R"(SELECT account_id FROM users WHERE user_id    = :user_id    AND status = 0 LIMIT 10;)";

            // 检查服务端生成的消息 ID(server_message_id) 是否存在
            constexpr std::string_view SQL_QUERY_EXIST_SERVER_MESSAGE_ID = R"(SELECT 1 FROM   messages WHERE  server_message_id = :message_id LIMIT 1; )";
            // 检查客户端生成的消息 ID(client_message_id) 是否存在
            constexpr std::string_view SQL_QUERY_EXIST_CLIENT_MESSAGE_ID = R"(SELECT 1 FROM   messages WHERE  client_message_id = :message_id AND sender_account_id = :sender_account_id LIMIT 1; )";

            // 检查文件 md5 是否已经存在
            constexpr std::string_view SQL_QUERY_EXIST_FILE_MD5          = R"(SELECT 1 FROM   file_metadatas WHERE  file_md5 = :file_md5 LIMIT 1; )";
            // 检查文件 ID 是否已经存在
            constexpr std::string_view SQL_QUERY_EXIST_FILE_ID           = R"(SELECT 1 FROM   file_metadatas WHERE  file_id  = :file_id  LIMIT 1; )";
            // 通过文件 ID 获取文件信息
            constexpr std::string_view SQL_QUERY_GET_FILE_INFO           = R"(SELECT * FROM   file_metadatas WHERE  file_id  = :file_id  LIMIT 1; )";

            // 校验用户注册是否成功/一致
            constexpr std::string_view SQL_QUERY_CHECK_REGISTER = R"(
                SELECT  1
                FROM    users
                WHERE
                        account_id           = :account_id            -- 账号 ID 匹配
                    AND user_id              = :user_id               -- 用户 ID 匹配
                    AND password_hash        = :password_hash         -- 密码哈希匹配
                    AND register_time        = :register_time         -- 注册时间匹配
                    AND security_question    = :security_question     -- 安全问题匹配
                    AND security_answer_hash = :security_answer_hash  -- 安全答案匹配
                    AND status               = 0                      -- 账号状态正常
                LIMIT 1;
            )";

            // 密码登录校验
            constexpr std::string_view SQL_QUERY_CHECK_PASSWORD = R"(
                SELECT  1
                FROM    users
                WHERE
                        account_id    = :account_id
                    AND password_hash = :password_hash
                    AND status        = 0              -- 要求用户账户状态正常，排除冻结或注销的账户
                LIMIT 1;
            )";

            // token 登录校验
            constexpr std::string_view SQL_QUERY_CHECK_TOKEN = R"(
                SELECT 1
                FROM   user_tokens
                WHERE
                        account_id   = :account_id     -- 传入的账号 ID
                    AND token_hash   = :token_hash     -- token_hash 匹配
                    AND token_status = 0               -- Token 未手动撤销
                    AND expire_time  > datetime('now') -- Token 未过期
                LIMIT 1;
            )";

            // 检查是否为好友关系
            constexpr std::string_view SQL_QUERY_GET_FRIEND_STATUS = R"(
                SELECT status
                FROM   friends
                WHERE
                        account_id        = :account_id        -- 当前用户的账号 ID
                    AND friend_account_id = :friend_account_id -- 好友的账号 ID
                    -- AND status         = 1                  -- 好友关系
                LIMIT 1;
            )";

            // 获取好友的关联信息
            constexpr std::string_view SQL_QUERY_GET_FRIEND_INFO = R"(
                SELECT *
                FROM   friends
                WHERE
                        account_id        = :account_id        -- 当前用户的账号 ID
                    AND friend_account_id = :friend_account_id -- 好友的账号 ID
                    -- AND status         = 1                  -- 好友关系
                LIMIT 1;
            )";

            // 获取指定账号的好友添加请求状态
            constexpr std::string_view SQL_QUERY_GET_FRIEND_REQUEST_STATUS = R"(
                SELECT status
                FROM   friend_requests
                WHERE
                        request_account_id = :request_account_id
                    AND target_account_id  = :target_account_id
                LIMIT 1;
            )";

            // 通过账号 ID 查询安全问题  // 账号停用时返回空，表示失败
            constexpr std::string_view SQL_QUERY_GET_ACTIVE_ACCOUNT_SECURITY_QUESTION = R"(
                SELECT  security_question
                FROM    users
                WHERE
                        account_id = :account_id  -- 账号 ID 匹配
                    AND status     = 0            -- 账号状态正常
                LIMIT 1;
            )";

            // 通过账号 ID 校验安全问题和答案  // 账号停用时返回空，表示失败
            constexpr std::string_view SQL_QUERY_EXIST_ACTIVE_ACCOUNT_SECURITY = R"(
                SELECT  1
                FROM    users
                WHERE
                        account_id           = :account_id           -- 账号 ID 匹配
                    AND security_question    = :security_question    -- 安全问题匹配
                    AND security_answer_hash = :security_answer_hash -- 安全答案匹配
                    AND status               = 0                     -- 账号状态正常
                LIMIT 1;
            )";

            // 查询文件 metadata 和 MD5 匹配的记录
            constexpr std::string_view SQL_QUERY_GET_FILE_ID = R"(
                SELECT file_id
                FROM   file_metadatas
                WHERE
                        file_name  = :file_name   -- 文件名匹配
                    AND file_size  = :file_size   -- 文件大小匹配
                    AND file_md5   = :file_md5    -- 文件 MD5 匹配
                LIMIT 1;
            )";

            // 查询账号文件权限
            constexpr std::string_view SQL_QUERY_GET_FILE_PERMISSION = R"(
                SELECT permission_code
                FROM   file_permissions
                WHERE
                        file_id    = :file_id    -- 文件 ID 匹配
                    AND account_id = :account_id -- 账号 ID 匹配
                LIMIT 1;
            )";

            // // 查询文件存储路径
            // constexpr std::string_view SQL_QUERY_GET_FILE_PATH = R"(
            //     SELECT file_path
            //     FROM   file_storage
            //     WHERE  file_md5 = :file_md5   -- 文件 MD5 匹配
            //     LIMIT 1;
            // )";
        }

        // 插入
        namespace INSERT {
            // constexpr std::string_view SQL_INSERT_META_VERSION = "INSERT INTO meta (name, value) VALUES ('version', :version);";

            // 设置数据版本
            constexpr std::string_view SQL_INSERT_META_VERSION = R"(
                INSERT INTO meta (name, value) VALUES ('version', '1.0');
            )";
            // 用户注册的 SQL 语句模板，注意：SQLCipher 不支持参数绑定，所以只能使用字符串替换，这会导致数据库注入风险，但由于这是一个注册语句，且参数都是由系统生成的，不直接暴露给用户，所以风险相对较小，可以接受。
            constexpr std::string_view SQL_INSERT_USER_REGISTER = R"(
                INSERT INTO users ( account_id,  user_id,  password_hash,  security_question,  security_answer_hash,  register_time, status)
                VALUES            (:account_id, :user_id, :password_hash, :security_question, :security_answer_hash, :register_time, 0     );
            )";
            // 用户登录记录的 SQL 语句模板
            constexpr std::string_view SQL_INSERT_USER_TOKEN = R"(
                INSERT INTO user_tokens ( account_id,  token_hash,  create_time,  expire_time)
                VALUES                  (:account_id, :token_hash, :create_time, :expire_time);
            )";

            // 添加好友请求记录
            constexpr std::string_view SQL_INSERT_FRIEND_REQUEST = R"(
                INSERT INTO friend_requests ( request_account_id,  target_account_id,  status,  message, timestamp)
                VALUES                      (:request_account_id, :target_account_id,       0, :message, datetime('now'));
            )";

            // 添加好友
            // 事务操作：需要同时更新双方的好友关系表，确保数据一致性
            // 1. 在自己的好友列表中添加对方，状态为已关联
            // 2. 在对方的好友列表中添加自己，状态为已关联
            // 3. 更新好友申请列表中的状态为已接受
            constexpr std::string_view SQL_INSERT_FRIEND = R"(
                INSERT INTO friends ( account_id,  friend_account_id, status, added_time     )
                VALUES              (:account_id, :friend_account_id,      1, datetime('now'));
             )";

            // 幂等插入好友关系：不存在才插入，避免重复记录
            constexpr std::string_view SQL_INSERT_FRIEND_IF_ABSENT = R"(
                INSERT INTO friends ( account_id,  friend_account_id, status, added_time    )
                SELECT               :account_id, :friend_account_id,      1, datetime('now')
                WHERE NOT EXISTS (
                        SELECT 1
                        FROM   friends
                        WHERE  account_id        = :account_id
                          AND  friend_account_id = :friend_account_id
                );
            )";

            // 写入短消息记录
            constexpr std::string_view SQL_INSERT_SHORT_MESSAGE = R"(
            INSERT INTO messages ( server_message_id,  client_message_id,  sender_account_id,  receiver_account_id,  short_text,  timestamp)
            VALUES               (:server_message_id, :client_message_id, :sender_account_id, :receiver_account_id, :short_text, :timestamp);
            )";

            // 写入文件 metadata 记录
            constexpr std::string_view SQL_INSERT_FILE_METADATA = R"(
                INSERT INTO file_metadatas ( file_id,  file_name,  file_size,  file_md5,  file_status, upload_time    )
                VALUES                     (:file_id, :file_name, :file_size, :file_md5, :file_status, datetime('now'));
            )";

            // // 保存文件 md5 与路径的映射关系
            // constexpr std::string_view SQL_INSERT_FILE_PATH = R"(
            //     INSERT INTO file_storage ( file_md5,  file_path, update_time    )
            //     VALUES                   (:file_md5, :file_path, datetime('now'))
            //  )";

        }

        // 更新
        // 没有数据时执行 UPDATE 语句不会报错，但受影响的行数为 0
        namespace UPDATE {
            // 已存在的好友关系更新为已关联
            constexpr std::string_view SQL_UPDATE_FRIEND_ASSOCIATED = R"(
                UPDATE friends
                SET    status            = 1,
                       added_time        = datetime('now')
                WHERE  account_id        = :account_id
                  AND  friend_account_id = :friend_account_id;
            )";

            // 将待处理的好友申请更新为已接受。
            // request_account_id: 发起申请的一方
            // target_account_id : 接收申请并执行同意的一方
            constexpr std::string_view SQL_UPDATE_FRIEND_REQUEST_ACCEPTED = R"(
                UPDATE friend_requests
                SET status = 1
                WHERE request_account_id = :request_account_id
                  AND target_account_id  = :target_account_id
                  AND status = 0;
            )";

            // 设置好友请求的状态
            // request_account_id: 发起申请的一方
            // target_account_id : 接收申请并执行拒绝的一方
            constexpr std::string_view SQL_UPDATE_FRIEND_REQUEST_STATUS = R"(
                UPDATE friend_requests
                SET    status = :status
                WHERE  request_account_id = :request_account_id
                  AND  target_account_id  = :target_account_id
            )";

            // 更新用户信息
            constexpr std::string_view SQL_UPDATE_USER_PROFILE = R"(
                UPDATE users
                SET    user_id      = :user_id     ,
                       nickname     = :nickname    ,
                       avatar_id    = :avatar_id   ,
                       sign         = :sign        ,
                       gender       = :gender      ,
                       region       = :region      ,
                       birthday     = :birthday    ,
                       desc         = :desc        ,
                       phones       = :phones      ,
                       emails       = :emails      ,
                       social_links = :social_links
                WHERE  account_id   = :account_id  ;
            )";

            // 更新好友信息
            constexpr std::string_view SQL_UPDATE_FRIEND_PROFILE = R"(
                UPDATE  friends
                SET     status  = :status ,
                        starred = :starred,
                        silent  = :silent ,
                        notes   = :notes  ,
                        tags    = :tags
                WHERE   account_id        = :account_id
                  AND   friend_account_id = :friend_account_id;
            )";

            // 使用子语句校验账户 ID 的密码并更新密码哈希
            constexpr std::string_view SQL_UPDATE_USER_PASSWORD_BY_PASSWORD = R"(
                UPDATE users
                SET    password_hash = :new_password_hash
                WHERE  account_id    = :account_id
                  AND  password_hash = :old_password_hash
                  AND  status        = 0; -- 确保账号状态正常，排除冻结或注销的账户
            )";

            // 使用子语句校验账户 ID 的安全问题并更新密码哈希
            constexpr std::string_view SQL_UPDATE_USER_PASSWORD_BY_SECURITY = R"(
                UPDATE users
                SET    password_hash = :new_password_hash
                WHERE  account_id    = :account_id
                  AND  security_question    = :security_question
                  AND  security_answer_hash = :security_answer_hash
                  AND  status               = 0; -- 确保账号状态正常，排除冻结或注销的账户
            )";

            // 使用子语句校验账户 ID 的安全问题并更新用户安全问题和答案
            constexpr std::string_view SQL_UPDATE_USER_SECURITY_BY_SECURITY = R"(
                UPDATE users
                SET    security_question    = :new_security_question,
                       security_answer_hash = :new_security_answer_hash
                WHERE  account_id           = :account_id
                  AND  security_question    = :old_security_question
                  AND  security_answer_hash = :old_security_answer_hash
                  AND  status               = 0; -- 确保账号状态正常，排除冻结或注销的账户
            )";

            // 如果文件权限表中已经有当前文件 ID 和账号 ID 的记录，则更新权限值；如果没有，则插入新记录
            constexpr std::string_view SQL_UPDATE_FILE_PERMISSION = R"(
                INSERT INTO file_permissions ( file_id,  account_id,  permission_code, update_time    )
                VALUES                       (:file_id, :account_id, :permission_code, datetime('now'))
                ON CONFLICT(file_id, account_id)
                DO UPDATE SET
                    permission_code  = excluded.permission_code,
                    update_time      = excluded.update_time         -- datetime('now');
             )";

            // 更新文件状态
            constexpr std::string_view SQL_UPDATE_FILE_STATUS = R"(
                UPDATE file_metadatas
                SET    file_status = :file_status
                WHERE  file_id     = :file_id;
            )";
        }
    }
}
