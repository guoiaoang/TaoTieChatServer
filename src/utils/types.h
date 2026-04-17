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
                    avatar            TEXT,                  -- 头像 URL
                    sign              TEXT,                  -- 个性签名
                    gender            INTEGER DEFAULT 0,     -- 0:未设置, 1:男, 2:女, 3:保密
                    region            TEXT,                  -- 建议存区域代码 (Code)
                    birthday          TEXT,                  -- 格式: YYYY-MM-DD

                    -- 联系方式
                    phones            TEXT,                  -- 建议存 JSON 数组或独立表  -- 暂定为以 `;` 划分的字符串数组
                    emails            TEXT,                  -- 建议存 JSON 数组或独立表  -- 暂定为以 `;` 划分的字符串数组

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
                    status            INTEGER DEFAULT 0,

                    -- 关系属性
                    starred           BOOLEAN,   -- starred 好友关系的星标
                    silent            BOOLEAN,   -- silent 好友关系的静音
                    remark            TEXT,      -- remark 对该好友的备注
                    tags              TEXT,      -- tags 好友关系的标签
                    form              TEXT,      -- form 好友关系的来源

                    -- 时间戳  -- SQLite 中的 DATETIME 类型实际上是一个文本类型，存储格式为 "YYYY-MM-DD HH:MM:SS"(UTC)
                    add_time             DATETIME DEFAULT (datetime('now')),  -- add_time 添加为好友的时间
                    -- add_time_str      TEXT,      -- add_time 添加为好友的时间
                    remove_time          DATETIME,  -- remove_time 删除好友的时间
                    -- remove_time_str   TEXT,      -- remove_time 删除好友的时间

                    -- pin            BOOLEAN,   -- pin 好友关系的置顶     -- 应当存储在聊天列表中

                    common            TEXT       -- common 通用字段
                );
            )";

            // 群组信息  // 暂定，后续根据实际需求调整
            constexpr std::string_view SQL_CREATE_TABLE_GROUPS = R"(
                CREATE TABLE IF NOT EXISTS groups (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    group_id    TEXT,  -- group_id 群 ID，唯一，注册时由系统生成，不可被用户修改，销号时不回收
                    name        TEXT,  -- name 群名称
                    avatar      TEXT,  -- avatar 群头像 URL
                    owner_id    TEXT,  -- owner_id 群主的账号 ID
                    member_ids  TEXT,  -- member_ids 群成员的账号 ID 列表，逗号分隔
                    create_time TEXT,  -- create_time 创建时间
                    update_time TEXT,  -- update_time 更新时间
                    common      TEXT   -- common 通用字段
                );
            )";

            // 聊天记录
            // 一对一的聊天记录和群聊记录都存储在同一个表中，
            // 通过 receiver_id 来区分是单聊还是群聊，
            // 单聊时 receiver_id 是好友的账号 ID，群聊时 receiver_id 是群 ID。
            constexpr std::string_view SQL_CREATE_TABLE_MESSAGES = R"(
                CREATE TABLE IF NOT EXISTS messages (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    message_id  TEXT,  -- message_id 消息 ID，对于消息唯一
                    sender_id   TEXT,  -- sender_id 发送者的账号 ID
                    receiver_id TEXT,  -- receiver_id 接收者的账号 ID，单聊时为好友的账号 ID，群聊时为群 ID
                    content     TEXT,  -- content 消息内容
                    type        TEXT,  -- type 消息类型，如文本、图片、文件等
                    timestamp   TEXT,  -- timestamp 消息时间戳
                    common      TEXT   -- common 通用字段
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

            // 校验用户注册是否成功/一致
            constexpr std::string_view SQL_QUERY_CHECK_REGISTER = R"(
                SELECT 1
                FROM   users
                WHERE
                        account_id           = :account_id         -- 账号 ID 匹配
                    AND user_id              = :user_id            -- 用户 ID 匹配
                    AND password_hash        = :password_hash      -- 密码哈希匹配
                    AND register_time        = :register_time      -- 注册时间匹配
                    AND security_question    = :security_question  -- 安全问题匹配
                    AND security_answer_hash = :security_answer_hash    -- 安全答案匹配
                    AND status               = 0                   -- 账号状态正常
                LIMIT 1;
            )";

            // 密码登录校验
            constexpr std::string_view SQL_QUERY_CHECK_PASSWORD = R"(
                SELECT 1
                FROM   users
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

            // 添加好友
            constexpr std::string_view SQL_INSERT_FRIEND = R"(
                INSERT INTO friends ( account_id,  friend_account_id, status, add_time       )
                VALUES              (:account_id, :friend_account_id,      1, datetime('now'));
             )";
        }
    }
}
