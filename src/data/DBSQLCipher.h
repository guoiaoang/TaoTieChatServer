#pragma once

#include "headers.h"
#include "utils.h"
#include <filesystem>
#include <format>

namespace SQL{
    namespace COMMANDS {
        // SQL 语句模板
        // constexpr std::string_view SQL_SET_PRAGMA_KEY      = "PRAGMA key = '{}';";  // 字符串替换会导致数据库注入，但加密语句不支持数据库绑定

        // 创建表格
        namespace CREATE{
            // 元数据
            constexpr std::string_view SQL_CREATE_TABLE_META =
                R"(CREATE TABLE IF NOT EXISTS meta ()"
                R"(    id INTEGER PRIMARY KEY,)"
                R"(    name   TEXT,)"
                R"(    value  TEXT,)"
                R"(    common TEXT)"
                R"();)";
            // 用户账号信息
            constexpr std::string_view SQL_CREATE_TABLE_USERS  =
                R"(CREATE TABLE IF NOT EXISTS users ()"
                R"(    id INTEGER PRIMARY KEY,)"  // id 是自增的唯一主键，外部不使用，内部也不使用，主要是为了满足 SQLCipher 的要求，必须有一个 INTEGER PRIMARY KEY 列才能启用自动增量 ID 功能，从而保证每条记录都有一个唯一的标识符。
                R"(    account_id TEXT,)"  // account_id 账号 ID，唯一，注册时由系统生成，不可被用户修改，销号时不回收
                R"(    user_id    TEXT,)"  // user_id 用户 ID，唯一，注册时由系统生成，但可被用户修改，可用于账号查找，销号时回收
                R"(    avatar     TEXT,)"  // avatar 头像 URL
                R"(    nickname   TEXT,)"  // nickname 昵称
                R"(    sign       TEXT,)"  // sign 个性签名
                R"(    gender     TEXT,)"  // gender 性别
                R"(    region     TEXT,)"  // region 多级地区
                R"(    birthday   TEXT,)"  // birthday 多级生日
                R"(    phone      TEXT,)"  // phone 多个电话
                R"(    email      TEXT,)"  // email 多个邮箱
                R"(    password   TEXT,)"  // password 简单加密存储的密码
                // R"( token      TEXT,)"  // token 多个令牌  // 需要单独的一个表来存储 token，包含 token 的值、类型、过期时间等信息
                // R"( create_time       TEXT,)"  // create_time 创建时间，update_time 更新时间，register_time 注册时间，这三个时间字段的区别在于：create_time 记录账号首次创建的时间，register_time 记录用户完成注册流程的时间（可能在 create_time 之后），update_time 记录用户信息最后一次修改的时间。
                R"(    register_time     TEXT,)"  // register_time 注册时间
                R"(    update_time       TEXT,)"  // update_time 更新时间
                R"(    unregister_time   TEXT,)"  // unregister_time 注销时间
                R"(    security_question TEXT,)"  // security_question 安全问题
                R"(    security_answer   TEXT,)"  // security_answer 安全答案
                // R"( register_ip       TEXT,)"  // register_ip 注册 IP
                R"(    common            TEXT,)"  // common 通用字段
                R"(    UNIQUE(account_id),)"  // 标示唯一
                R"(    UNIQUE(user_id))"      // 标示唯一
                R"();)";
            // 好友关系 // 好友的个人信息不进行缓存
            constexpr std::string_view SQL_CREATE_TABLE_FRIENDS =
                R"(CREATE TABLE IF NOT EXISTS friends ()"
                R"(    id INTEGER PRIMARY KEY,)" 
                R"(    account_id        TEXT,)"  // 当前用户的账号 ID
                R"(    friend_account_id TEXT,)"  // friend_account_id 好友的账号 ID
                R"(    remark            TEXT,)"  // remark 对该好友的备注
                R"(    tags              TEXT,)"  // tags 好友关系的标签
                R"(    form              TEXT,)"  // form 好友关系的来源
                R"(    add_time          TEXT,)"  // add_time 添加为好友的时间
                R"(    remove_time       TEXT,)"  // remove_time 删除好友的时间
                R"(    starred           BOOLEAN,)"  // starred 好友关系的星标
                // R"(    pin            BOOLEAN,)"  // pin 好友关系的置顶  // 应当存储在聊天列表中
                R"(    silent            BOOLEAN,)"  // silent 好友关系的静音
                R"(    common            TEXT)"  // common 通用字段
                R"();)";
            // 群组信息  // 暂定，后续根据实际需求调整
            constexpr std::string_view SQL_CREATE_TABLE_GROUPS =
                R"(CREATE TABLE IF NOT EXISTS groups ()"
                R"(    id INTEGER PRIMARY KEY,)" 
                R"(    group_id    TEXT,)"  // group_id 群 ID，唯一，注册时由系统生成，不可被用户修改，销号时不回收
                R"(    name        TEXT,)"  // name 群名称
                R"(    avatar      TEXT,)"  // avatar 群头像 URL
                R"(    owner_id    TEXT,)"  // owner_id 群主的账号 ID
                R"(    member_ids  TEXT,)"  // member_ids 群成员的账号 ID 列表，逗号分隔
                R"(    create_time TEXT,)"  // create_time 创建时间
                R"(    update_time TEXT,)"  // update_time 更新时间
                R"(    common      TEXT)"   // common 通用字段
                R"();)";
            // 聊天记录 // 一对一的聊天记录和群聊记录都存储在同一个表中，通过 receiver_id 来区分是单聊还是群聊，单聊时 receiver_id 是好友的账号 ID，群聊时 receiver_id 是群 ID。
            constexpr std::string_view SQL_CREATE_TABLE_MESSAGES =
                R"(CREATE TABLE IF NOT EXISTS messages ()"
                R"(    id INTEGER PRIMARY KEY,)" 
                R"(    message_id  TEXT,)"  // message_id 消息 ID，对于消息唯一
                R"(    sender_id   TEXT,)"  // sender_id 发送者的账号 ID
                R"(    receiver_id TEXT,)"  // receiver_id 接收者的账号 ID，单聊时为好友的账号 ID，群聊时为群 ID
                R"(    content     TEXT,)"  // content 消息内容
                R"(    type        TEXT,)"  // type 消息类型，如文本、图片、文件等
                R"(    timestamp   TEXT,)"  // timestamp 消息时间戳
                R"(    common      TEXT)"   // common 通用字段
                R"();)";
            // // 群聊消息记录 // 群聊消息记录表可以存储一些特定于群聊的信息，如 @提及 的用户列表等
            // // 也可以不单独存储，而是在 messages 表中通过 content 字段
            // // 来存储一个 JSON 对象，包含消息内容和一些额外的元信息，这样可以避免表结构的复杂化，同时也更灵活地适应未来可能出现的需求变化。
            // constexpr std::string_view SQL_CREATE_TABLE_GROUP_MESSAGES =
            //     R"(CREATE TABLE IF NOT EXISTS group_messages ()"
            //     R"(    id INTEGER PRIMARY KEY,)" 
            //     R"(    message_id TEXT,)"  // message_id 消息 ID，对于消息唯一
            //     R"(    sender_id  TEXT,)"  // sender_id 发送者的账号 ID
            //     R"(    group_id   TEXT,)"  // group_id 群 ID
            //     R"(    content    TEXT,)"  // content 消息内容，存储一个 JSON 对象，包含消息内容和一些额外的元信息，如 @提及 的用户列表等
            //     R"(    type       TEXT,)"  // type 消息类型，如文本、图片、文件等
            //     R"(    timestamp  TEXT,)"  // timestamp 消息时间戳
            //     R"(    common     TEXT)"   // common 通用字段
            //     R"();)";
            // 有效 token 表 // 存储用户的登录状态等信息，包含 token 的值、类型、过期时间等信息
            constexpr std::string_view SQL_CREATE_TABLE_TOKENS = 
                R"(CREATE TABLE IF NOT EXISTS tokens ()"
                R"(    id INTEGER PRIMARY KEY,)" 
                R"(    token       TEXT,)"  // token 令牌的值，唯一
                R"(    account_id  TEXT,)"  // account_id 账号 ID，外键关联 users 表的 account_id
                R"(    type        TEXT,)"  // type 令牌类型，如登录令牌、设备令牌等
                R"(    expire_time TEXT,)"  // expire_time 过期时间
                R"(    create_time TEXT,)"  // create_time 创建时间
                R"(    common      TEXT)"   // common 通用字段
                R"();)";
        }

        // 索引
        namespace INDEX {
            constexpr std::string_view SQL_INDEX_ACCOUNT_ID = "CREATE INDEX IF NOT EXISTS idx_account_id ON users (account_id);";
            constexpr std::string_view SQL_INDEX_USER_ID    = "CREATE INDEX IF NOT EXISTS idx_user_id ON users (user_id);";
        }

        // 查询
        namespace QUERY {
            // 查询数据版本
            constexpr std::string_view SQL_QUERY_META_VERSION  = "SELECT name, value FROM meta where name = 'version' LIMIT 1;";
            // // 查询账号 uuid 的 SQL 语句模板
            // constexpr std::string_view SQL_QUERY_ACCOUNT_ID = "SELECT account_id FROM users WHERE account_id = :account_id;";
            // // 查询用户 uuid 的 SQL 语句模板
            // constexpr std::string_view SQL_QUERY_USER_ID = "SELECT user_id FROM users WHERE user_id = :user_id;";
            // 查询账号是否存在的 SQL 语句模板
            constexpr std::string_view SQL_QUERY_ACCOUNT_EXISTS = "SELECT 1 FROM users WHERE account_id = :account_id LIMIT 1;";
            // 查询用户是否存在的 SQL 语句模板
            constexpr std::string_view SQL_QUERY_USER_EXISTS = "SELECT 1 FROM users WHERE user_id = :user_id LIMIT 1;";
            // // 查询账号和用户是否匹配的 SQL 语句模板
            // constexpr std::string_view SQL_QUERY_ACCOUNT_USER_MATCH = "SELECT 1 FROM users WHERE account_id = :account_id AND user_id = :user_id LIMIT 1;";

            // 校验用户注册是否成功/一致
            constexpr std::string_view SQL_QUERY_USER_REGISTER_CHECK = 
                R"(SELECT 1)"
                R"(    FROM users )"
                R"(    WHERE account_id        = :account_id       )"
                R"(      AND user_id           = :user_id          )"
                R"(      AND nickname          = :nickname         )"
                R"(      AND password          = :password         )"
                R"(      AND register_time     = :register_time    )"
                R"(      AND security_question = :security_question)"
                R"(      AND security_answer   = :security_answer  )"
                R"( LIMIT 1;)";
        }

        // 设置
        namespace INSERT {
            // 设置数据版本
            // constexpr std::string_view SQL_INSERT_META_VERSION = "INSERT INTO meta (name, value) VALUES ('version', :version);";
            constexpr std::string_view SQL_INSERT_META_VERSION = "INSERT INTO meta (name, value) VALUES ('version', '1.0');";
            // 用户注册的 SQL 语句模板，注意：SQLCipher 不支持参数绑定，所以只能使用字符串替换，这会导致数据库注入风险，但由于这是一个注册语句，且参数都是由系统生成的，不直接暴露给用户，所以风险相对较小，可以接受。
            constexpr std::string_view SQL_INSERT_USER_REGISTER = 
                R"(INSERT INTO users (account_id, user_id, nickname, password, register_time, security_question, security_answer) )"
                R"(VALUES (:account_id, :user_id, :nickname, :password, :register_time, :security_question, :security_answer);)";
                // R"(INSERT INTO users (account_id, user_id, avatar, nickname, sign, gender, region, birthday, phone, email, password, register_time, security_question, security_answer) )"
                // R"(VALUES (:account_id, :user_id, :avatar, :nickname, :sign, :gender, :region, :birthday, :phone, :email, :password, :register_time, :security_question, :security_answer);)";
        }
    }
}

// 定义一个简单的多类型值容器（C++17）
using SqlValue = std::variant<int, double, std::string, std::nullptr_t>; // 0,1,2,3 分别表示 int、double、string、null
// using SqlNamedParam = std::map<std::string, SqlValue>;

struct WrapperQueryResult {
    bool success;
    std::vector<std::map<std::string, SqlValue>> results; // 列名 - 列值
    std::string error_message;
    WrapperQueryResult(bool success = false, const std::vector<std::map<std::string, SqlValue>>& results = {}, const std::string& error_message = "")
        : success(success), results(results), error_message(error_message) {}
};

struct WrapperNamedParam {
    std::map<std::string, SqlValue> params;
    WrapperNamedParam(const std::map<std::string, SqlValue>& params = {}) : params(params) {}
    SqlValue& operator[](const std::string& name) {
        return params[name];
    }
    const SqlValue& operator[](const std::string& name) const {
        auto it = params.find(name);
        if (it != params.end()) {
            return it->second;
        }
        // throw std::out_of_range("Parameter not found: " + name);
        return nullptr; // 返回一个空值，表示未找到参数
    }
};

class SQLiteWrapper {
public:
    // 执行查询并将结果存入 vector<map>
    std::shared_ptr<WrapperQueryResult> query_named(const std::string& sql, const std::shared_ptr<WrapperNamedParam> params = nullptr) {
        sqlite3_stmt* stmt;
        auto results = std::make_shared<WrapperQueryResult>();

        if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            results->success = false;
            results->error_message = sqlite3_errmsg(m_db);
            return results;
        }

        // --- 核心：命名参数绑定 ---
        if (params) {
            for (auto const& [name, value] : params->params) {
                int index = sqlite3_bind_parameter_index(stmt, name.c_str());
                if (index > 0) { // 只有在 SQL 中找到了该名称才绑定
                    bind_variant(stmt, index, value);
                }
                else {
                    LOG_WARNING("Parameter name '{}' not found in SQL statement, skipping binding.", name);
                }
            }
        }

        // --- 获取结果并转换 ---
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, SqlValue> row;
            int col_count = sqlite3_column_count(stmt);

            for (int i = 0; i < col_count; i++) {
                std::string col_name = sqlite3_column_name(stmt, i);
                row[col_name] = extract_column(stmt, i);
            }
            results->results.emplace_back(row);
        }
        results->success = true;

        sqlite3_finalize(stmt);
        return results;
    }

    void set_db(sqlite3* database) {
        m_db = database;
    }

    sqlite3* get_db() const {
        return m_db;
    }

private:
    sqlite3* m_db = nullptr;

    // 辅助：根据 variant 类型执行绑定
    void bind_variant(sqlite3_stmt* stmt, int idx, const SqlValue& v) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            ;    if constexpr (std::is_same_v<T, int           >) sqlite3_bind_int   (stmt, idx, arg);
            else if constexpr (std::is_same_v<T, double        >) sqlite3_bind_double(stmt, idx, arg);
            else if constexpr (std::is_same_v<T, std::string   >) sqlite3_bind_text  (stmt, idx, arg.c_str(), -1, SQLITE_TRANSIENT);
            else if constexpr (std::is_same_v<T, std::nullptr_t>) sqlite3_bind_null  (stmt, idx);
        }, v);
    }

    // 辅助：从列中提取数据存入 variant
    SqlValue extract_column(sqlite3_stmt* stmt, int idx) {
        int type = sqlite3_column_type(stmt, idx);
        if (type == SQLITE_INTEGER) return sqlite3_column_int(stmt, idx);
        if (type == SQLITE_FLOAT)   return sqlite3_column_double(stmt, idx);
        if (type == SQLITE_TEXT)    return std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx)));
        return nullptr;
    }
};

class DBSQLCipher {
private:
    DBSQLCipher() = default;
    DBSQLCipher(const DBSQLCipher&) = delete;
    DBSQLCipher& operator=(const DBSQLCipher&) = delete;
public:
    static DBSQLCipher& instance() {
        static DBSQLCipher instance;
        return instance;
    }
    ~DBSQLCipher() {
        // 关闭数据库连接
        if (m_db_sqlcipher) {
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            LOG_INFO("Closed database connection");
        }
    }

public:
    void set_init_params(const std::filesystem::path& db_path, const std::string& encryption_key) {
        m_db_path = db_path;
        m_encryption_key = encryption_key;
    }

    bool database_open() {
        // 1. 检查句柄，如果不为空则表示已经正确打开并已经被初始化
        // 2. 句柄为空则使用密码打开，并读取数据版本信息
        // 3. 读取版本失败时进行创建表和写入版本信息的初始化操作
        // 4. 再次尝试读取版本号，失败则报错

        // 密码不正确也可以打开句柄，但无法读出及写入数据，因此我们通过读写数据来验证密码是否正确

        using namespace SQL::COMMANDS::CREATE;
        using namespace SQL::COMMANDS::INDEX;
        using namespace SQL::COMMANDS::QUERY;
        using namespace SQL::COMMANDS::INSERT;

        auto absolute_db_path   = std::filesystem::absolute(m_db_path);
        auto parent_path        = absolute_db_path.parent_path();
        std::string str_db_path = absolute_db_path.string(); // 必须，否则 c_str() 返回的指针会指向一个临时对象，导致后续使用时访问无效内存
        const char* sz_db_path  = str_db_path.c_str();
        LOG_TRACE("Attempting to open database at path: {}", absolute_db_path.string());
        LOG_TRACE("Attempting to open database at path: {}", sz_db_path);

        // 句柄打开则认为已经初始化，不再校验
        if(m_db_sqlcipher) {
            LOG_TRACE("Database is already open: {}", sz_db_path);
            return true;
        }

        LOG_INFO("Database is not open, attempting to open: {}", sz_db_path);

        // SQLCipher 不会自动创建父文件夹，需要手动创建
        if(!std::filesystem::exists(parent_path)) {
            LOG_INFO("Creating database parent directory: {}", parent_path.string());
            std::filesystem::create_directories(parent_path); // 确保目录存在
        }
        LOG_TRACE("Database parent directory exists: {}", parent_path.string());

        // 尝试打开数据库，如果数据库文件不存在，sqlite3_open 会创建一个新的空数据库文件
        int rc = sqlite3_open(sz_db_path, &m_db_sqlcipher);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Can't open database: {}", sqlite3_errmsg(m_db_sqlcipher));
            m_db_sqlcipher = nullptr;
            return false;
        }

        LOG_DEBUG("Opened database successfully");

        // PRAGMA key 不支持参数绑定，但使用字符串依然会导致数据库注入
        rc = sqlite3_key(m_db_sqlcipher, m_encryption_key.c_str(), (int)m_encryption_key.size());
        if(rc != SQLITE_OK) {
            LOG_ERROR("Failed to set encryption key: {}", sqlite3_errmsg(m_db_sqlcipher));
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }
        // 仅表示执行成功，并不代表密钥正确，后续通过读写验证密钥是否正确
        LOG_DEBUG("Encryption key set successfully");

        // 如果可以正确获取到版本号则说明数据库已经被初始化且秘钥正确。
        // 空文件也会导致失败
        // 空文件：SQLITE_ERROR  (1) no such table: meta
        // 密码错误：SQLITE_NOTADB (26) file is not a database
        auto execute_query_meta_version = execute(SQL_QUERY_META_VERSION.data());
        if (execute_query_meta_version->success) {
            auto&v = execute_query_meta_version->results;
            if(!v.empty()) {
                auto&m = v[0];
                auto ite_name  = m.find("name" );
                auto ite_value = m.find("value");
                if(ite_name != m.end() && ite_value != m.end()) {
                    auto name  = std::get<std::string>(ite_name->second);
                    auto value = std::get<std::string>(ite_value->second);
                    LOG_INFO("Meta version query result: {} = {}", name, value);
                }
                else {
                    LOG_WARNING("Meta version query result has unexpected format, expected 2 columns 'name' and 'value', but got {} columns", m.size());
                }
            }
            return true;
        }

        int error_code = sqlite3_errcode(m_db_sqlcipher);
        if(error_code == SQLITE_NOTADB) {
            // 密码可能错误
            LOG_ERROR("Failed to query version test, file is not a database, may be caused by incorrect encryption key");
        }
        else if(error_code == SQLITE_ERROR) {
            // 可能为空文件
            LOG_ERROR("Failed to query version test, SQL error, may be caused by missing table, error code: {}", error_code);
        }
        // else {
        //     LOG_ERROR("Failed to query version test, unexpected error code: {}", error_code);
        // }

        // 创建元数据表
        // SQLCipher 要求必须有写入操作才会初始化数据库文件，否则为 0 字节的空文件
        // 尝试执行一个写入操作来初始化数据库文件
        // 密码错误时也可以执行到该步骤，只是会返回失败
        // std::string str_sql_table_init_version = std::format("{}{}", SQL_CREATE_TABLE_META, std::format(SQL_INSERT_META_VERSION, "1.0"));
        // auto execute_set_meta_version = execute(str_sql_table_init_version);
        auto execute_create_table_meta = execute(SQL_CREATE_TABLE_META.data());
        if (!execute_create_table_meta->success) {
            LOG_ERROR("Failed to initialize database file with create table meta: {}", execute_create_table_meta->error_message);
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }
        LOG_DEBUG("Database file initialized successfully with create table meta");

        // 设置元数据版本
        auto execute_set_meta_version = execute(SQL_INSERT_META_VERSION.data());
        if (!execute_set_meta_version->success) {
            LOG_ERROR("Failed to initialize database file with write version: {}", execute_set_meta_version->error_message);
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }
        LOG_DEBUG("Database file initialized successfully with write version");

        // 再次读取验证是否正确写入了数据，如果仍然失败则说明密钥不正确
        auto query_version_verify = execute(SQL_QUERY_META_VERSION.data());
        if (!query_version_verify->success) {
            LOG_ERROR("Failed to verify database initialization, encryption key may be incorrect");
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }
        LOG_INFO("Database initialization verified successfully");
        return true;
    }

    bool database_initialize() {
        using namespace SQL::COMMANDS::CREATE;
        using namespace SQL::COMMANDS::INDEX;
        using namespace SQL::COMMANDS::QUERY;
        using namespace SQL::COMMANDS::INSERT;
        // 创建表
        using namespace SQL::COMMANDS;
        std::vector<std::string_view> table_creation_commands = {
            SQL_CREATE_TABLE_META,
            SQL_CREATE_TABLE_USERS,
            SQL_CREATE_TABLE_FRIENDS,
            SQL_CREATE_TABLE_GROUPS,
            SQL_CREATE_TABLE_MESSAGES,
            // SQL_CREATE_TABLE_GROUP_MESSAGES,
            SQL_CREATE_TABLE_TOKENS,
            SQL_INDEX_ACCOUNT_ID,
            SQL_INDEX_USER_ID
        };
        for(const auto& cmd : table_creation_commands) {
            auto result = execute(cmd.data());
            if(!result->success) {
                LOG_ERROR("Failed to execute table creation command: {}, error: {}", cmd, result->error_message);
                return false;
            }
        }
        LOG_INFO("All tables created successfully");
        return true;
    }

public:
    void print_sqlcipher_info() {
        // if (!m_db_sqlcipher) {
        //     LOG_WARNING("Database not initialized");
        //     return;
        // }

        // 获取 SQLCipher 版本信息
        const char* sqlcipher_version = sqlite3_libversion();
        LOG_INFO("SQLCipher version: {}", sqlcipher_version);

        const char* sqlcipher_source_id = sqlite3_sourceid();
        LOG_INFO("SQLCipher source ID: {}", sqlcipher_source_id);

        int sqlcipher_libversion_number = sqlite3_libversion_number();
        LOG_INFO("SQLCipher version number: {}", sqlcipher_libversion_number);

        int sqlcipher_compile_options_count = sqlite3_compileoption_used("SQLCIPHER");
        LOG_INFO("SQLCipher compile options count: {}", sqlcipher_compile_options_count);

        if (sqlcipher_compile_options_count) {
            LOG_INFO("SQLCipher was compiled with SQLCIPHER option");
        } else {
            LOG_WARNING("SQLCipher was not compiled with SQLCIPHER option");
        }

        int sqlcipher_threadsafe = sqlite3_threadsafe();
        LOG_INFO("SQLCipher threadsafe: {}", (sqlcipher_threadsafe ? "Yes" : "No"));

        int sqlcipher_initialize = sqlite3_initialize();
        LOG_INFO("SQLCipher initialize: {}", sqlcipher_initialize);

        int sqlcipher_shutdown = sqlite3_shutdown();
        LOG_INFO("SQLCipher shutdown: {}", sqlcipher_shutdown);

        int sqlcipher_os_init = sqlite3_os_init();
        LOG_INFO("SQLCipher OS init: {}", sqlcipher_os_init);

        int sqlcipher_os_end = sqlite3_os_end();
        LOG_INFO("SQLCipher OS end: {}", sqlcipher_os_end);

        sqlite3_int64 sqlcipher_memory_used = sqlite3_memory_used();
        LOG_INFO("SQLCipher memory used: {}", sqlcipher_memory_used);

        // SQLITE_API SQLITE_DEPRECATED int sqlite3_global_recover(void);
        // SQLITE_API SQLITE_DEPRECATED void sqlite3_thread_cleanup(void);
        // SQLITE_API void sqlite3_reset_auto_extension(void);
        
        int sqlcipher_keyword_count = sqlite3_keyword_count();
        LOG_INFO("SQLCipher keyword count: {}", sqlcipher_keyword_count);
    }

    std::shared_ptr<WrapperQueryResult> execute(const std::string& sql, const std::shared_ptr<WrapperNamedParam> params = nullptr) {
        // SQLiteWrapper db_wrapper; // 创建 SQLiteWrapper 实例
        // // db.execute("PRAGMA cipher_default_compatibility = 4;"); // 设置兼容模式，确保与 SQLCipher 4.x 版本兼容
        // // db.execute("PRAGMA journal_mode = WAL;"); // 设置 WAL 模式，提升并发性能
        // // db.execute("PRAGMA synchronous = NORMAL;"); // 设置同步模式，提升性能的同时保持合理的数据安全性
        // // db.execute("PRAGMA temp_store = MEMORY;"); // 将临时存储设置为内存，提升性能
        // // db.execute("PRAGMA locking_mode = EXCLUSIVE;"); // 设置锁定模式为独占，减少锁竞争，提高性能
        // // db.execute("PRAGMA auto_vacuum = FULL;"); // 启用自动清理，保持数据库文件大小合理
        // // db.execute("PRAGMA secure_delete = ON;"); // 启用安全删除，确保删除的数据无法恢复
        // // db.execute("PRAGMA cipher_page_size = 4096;"); // 设置页面大小，提升性能和安全性
        // // db.execute("PRAGMA cipher_hmac_algorithm = HMAC_SHA512;"); // 设置 HMAC 算法，提升安全性
        // // db.execute("PRAGMA cipher_kdf_algorithm = PBKDF2_HMAC_SHA512;"); // 设置 KDF 算法，提升安全性
        // db_wrapper.execute(m_db_sqlcipher, SQL_SET_PRAGMA_KEY.data(), m_encryption_key);
        if(m_db_wrapper.get_db() != m_db_sqlcipher) {
            m_db_wrapper.set_db(m_db_sqlcipher);
        }
        return m_db_wrapper.query_named(sql, params);
    }

    int get_last_error_code() {
        return sqlite3_errcode(m_db_sqlcipher);
    }

    int get_extended_error_code() {
        return sqlite3_extended_errcode(m_db_sqlcipher);
    }

    const char* get_error_message() {
        return sqlite3_errmsg(m_db_sqlcipher);
    }

public:
    struct UserRegisterParameters {
        std::string nickname;
        std::string password;
        std::string security_question;
        std::string security_answer;
    };

    struct UserRegisterResult {
        bool success = false; // 注册是否成功
        std::string account_id; // 注册成功时返回的账号 ID
        std::string user_id;    // 注册成功时返回的用户 ID
        std::string error_message; // 注册失败时的错误信息
    };

    std::shared_ptr<UserRegisterResult> user_register(std::shared_ptr<UserRegisterParameters> params) {
        using namespace SQL::COMMANDS::CREATE;
        using namespace SQL::COMMANDS::INDEX;
        using namespace SQL::COMMANDS::QUERY;
        using namespace SQL::COMMANDS::INSERT;

        // 注册用户，返回是否注册成功
        // 1. 检查账号是否已存在，如果已存在则返回失败
        // 2. 如果账号不存在则插入新记录，并返回成功
        std::shared_ptr<UserRegisterResult> result = std::make_shared<UserRegisterResult>();
        if(params == nullptr || params->nickname.empty() || params->password.empty() || params->security_question.empty() || params->security_answer.empty()) {
            LOG_ERROR("User register parameters is null");
            return result;
        }
        // 实现用户注册逻辑
        std::string account_id;
        std::string user_id;
        std::shared_ptr<WrapperQueryResult> query_account_exists;
        std::shared_ptr<WrapperQueryResult> query_user_exists;
        std::shared_ptr<WrapperQueryResult> insert_result;
        do {
            // 用户 uuid
            // do {
            //     user_id = Utils::instance()->to_string(Utils::instance()->generate_uuid());
            //     auto query_user_exists_params = std::make_shared<WrapperNamedParam>();
            //     query_user_exists_params->params[":user_id"] = user_id;
            //     query_user_exists = execute(SQL_QUERY_USER_EXISTS.data(), query_user_exists_params);
            // } while(query_user_exists->success && !query_user_exists->results.empty());
            // user_id = params->nickname; // 先使用昵称作为 user_id，后续可以提供修改接口让用户修改 user_id，但必须保证唯一性
            auto query_user_exists_params = std::make_shared<WrapperNamedParam>();
            query_user_exists_params->params[":user_id"] = params->nickname;
            query_user_exists = execute(SQL_QUERY_USER_EXISTS.data(), query_user_exists_params);
            if(query_user_exists->success && !query_user_exists->results.empty()) {
                result->success       = false;
                result->account_id    = "";
                result->user_id       = "";
                result->error_message = "User ID already exists";
                LOG_ERROR("User ID already exists: {}", params->nickname);
                break;
            }
            // 使用 nickname 作为默认用户 ID
            user_id = params->nickname;

            // 生成账号 uuid
            do {
                account_id = Utils::instance()->to_string(Utils::instance()->generate_uuid());
                auto query_account_exists_params = std::make_shared<WrapperNamedParam>();
                query_account_exists_params->params[":account_id"] = account_id;
                query_account_exists = execute(SQL_QUERY_ACCOUNT_EXISTS.data(), query_account_exists_params);
            } while(query_account_exists->success && !query_account_exists->results.empty());

            // 获取注册时间
            auto register_time = Utils::instance()->currentDateTime();

            // 构造参数
            auto user_register_params = std::make_shared<WrapperNamedParam>();
            user_register_params->params[":account_id"] = account_id;
            user_register_params->params[":user_id"]    = user_id;
            user_register_params->params[":nickname"]   = params->nickname;
            user_register_params->params[":password"]   = params->password;  // 由用户端加密后传输，服务器不进行二次加密，直接存储
            user_register_params->params[":security_question"] = params->security_question;
            user_register_params->params[":security_answer"]   = params->security_answer;
            user_register_params->params[":register_time"]     = Utils::instance()->to_string(register_time);

            // 插入新记录
            insert_result = execute(SQL_INSERT_USER_REGISTER.data(), user_register_params);
            // 注册命令执行失败
            if(!insert_result->success) {
                result->success       = false;
                result->account_id    = "";
                result->user_id       = "";
                result->error_message = insert_result->error_message;
                LOG_ERROR("Failed to insert user register record: {}", insert_result->error_message);
                break;
            }

            // 注册成功时需要校验
            auto insert_result_check = execute(SQL_QUERY_USER_REGISTER_CHECK.data(), user_register_params);
            // 校验命令执行失败
            if(    !insert_result_check->success
                ||  insert_result_check->results.empty()
                ||  insert_result_check->results[0].empty()
                ||  insert_result_check->results[0].begin()->first != "1"
                ||  std::get<int>(insert_result_check->results[0].begin()->second) != 1
            ) {
                result->success       = false;
                result->account_id    = "";
                result->user_id       = "";
                result->error_message = insert_result_check->error_message;
                LOG_ERROR("Failed to check user register record: {}", insert_result_check->error_message);
                break;
            }

            // 注册成功
            result->success       = true;
            result->account_id    = account_id;
            result->user_id       = user_id;
            result->error_message = "";
            LOG_INFO("User registered successfully, account_id: {}, user_id: {}", account_id, user_id);
            break;
        } while(false);
        return result;
    }

// private:
//     void sqlcipher_commands() {
//         static const std::map<std::string, std::string> commands = {
//             {"PRAGMA cipher_version;", "获取 SQLCipher 版本信息"},
//             {"PRAGMA cipher_source_id;", "获取 SQLCipher 源代码 ID"},
//             {"PRAGMA cipher_libversion_number;", "获取 SQLCipher 版本号"},
//             {"PRAGMA compile_options;", "获取 SQLCipher 编译选项"},
//             {"PRAGMA threadsafe;", "检查 SQLCipher 是否线程安全"},
//             {"PRAGMA initialize;", "初始化 SQLCipher"},
//             {"PRAGMA shutdown;", "关闭 SQLCipher"},
//             {"PRAGMA os_init;", "初始化 SQLCipher 的操作系统接口"},
//             {"PRAGMA os_end;", "关闭 SQLCipher 的操作系统接口"},
//             {"PRAGMA memory_used;", "获取 SQLCipher 当前使用的内存量"},
//             {"PRAGMA keyword_count;", "获取 SQLCipher 关键字数量"}
//         };
//     }
private:
    std::filesystem::path m_db_path;
    sqlite3*              m_db_sqlcipher = nullptr;
    std::string           m_encryption_key;
    SQLiteWrapper         m_db_wrapper; // 数据库操作包装类实例
};
