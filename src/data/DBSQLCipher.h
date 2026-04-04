#pragma once

#include "headers.h"
#include "utils.h"
#include "types.h"
#include <filesystem>
#include <format>

// 定义一个简单的多类型值容器（C++17）
using SqlValue = std::variant<int, double, std::string, std::nullptr_t>;  // 0,1,2,3 分别表示 int、double、string、null
// using SqlNamedParam = std::map<std::string, SqlValue>;

struct WrapperQueryResult {
    bool done    = false;  // 仅表示是否执行完成，不代表是否成功
    std::vector<std::map<std::string, SqlValue>> results;  // 列名 - 列值
    std::string error_message;
    WrapperQueryResult(bool done = false, const std::vector<std::map<std::string, SqlValue>>& results = {}, const std::string& error_message = "")
        : done(done), results(results), error_message(error_message) {}
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
        return nullptr;  // 返回一个空值，表示未找到参数
    }
};

class SQLiteWrapper {
public:
    // 执行查询并将结果存入 vector<map>
    std::shared_ptr<WrapperQueryResult> query_named(const std::string& sql, const std::shared_ptr<WrapperNamedParam> params = nullptr) {
        sqlite3_stmt* stmt;
        auto results = std::make_shared<WrapperQueryResult>();

        if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            results->done          = true;
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
                    LOG_TRACE("Parameter name '{}' not found in SQL statement, skipping binding.", name);
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
        results->done = true;

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

    // 打开数据库并进行初始化验证
    // 初始化仅写入 version 信息
    bool database_open() {
        using namespace SQL::COMMANDS::PRAGMA;
        using namespace SQL::COMMANDS::CREATE;
        using namespace SQL::COMMANDS::QUERY;
        using namespace SQL::COMMANDS::INSERT;

        // 1. 检查句柄，如果不为空则表示已经正确打开并已经被初始化
        // 2. 句柄为空则使用密码打开，并读取数据版本信息
        // 3. 读取版本失败时进行创建表和写入版本信息的初始化操作
        // 4. 再次尝试读取版本号，失败则报错

        // 密码不正确也可以打开句柄，但无法读出及写入数据，因此我们通过读写数据来验证密码是否正确

        auto absolute_db_path   = std::filesystem::absolute(m_db_path);
        auto parent_path        = absolute_db_path.parent_path();
        std::string str_db_path = absolute_db_path.string();  // 必须，否则 c_str() 返回的指针会指向一个临时对象，导致后续使用时访问无效内存
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
            std::filesystem::create_directories(parent_path);  // 确保目录存在
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
        if (execute_query_meta_version->done) {
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
        if (!execute_create_table_meta->done) {
            LOG_ERROR("Failed to initialize database file with create table meta: {}", execute_create_table_meta->error_message);
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }
        LOG_DEBUG("Database file initialized successfully with create table meta");

        // 设置元数据版本
        auto execute_set_meta_version = execute(SQL_INSERT_META_VERSION.data());
        if (!execute_set_meta_version->done) {
            LOG_ERROR("Failed to initialize database file with write version: {}", execute_set_meta_version->error_message);
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }
        LOG_DEBUG("Database file initialized successfully with write version");

        // 再次读取验证是否正确写入了数据，如果仍然失败则说明密钥不正确
        auto query_version_verify = execute(SQL_QUERY_META_VERSION.data());
        if (!query_version_verify->done) {
            LOG_ERROR("Failed to verify database initialization, encryption key may be incorrect");
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }
        LOG_INFO("Database initialization verified successfully");
        return true;
    }

    bool database_initialize() {
        using namespace SQL::COMMANDS::PRAGMA;
        using namespace SQL::COMMANDS::CREATE;
        using namespace SQL::COMMANDS::QUERY;
        using namespace SQL::COMMANDS::INSERT;

        std::vector<std::string_view> table_creation_commands = {
            SQL_CREATE_TABLE_META,
            SQL_CREATE_TABLE_USERS,
            SQL_CREATE_INDEX_ACCOUNT_ID,
            SQL_CREATE_INDEX_USER_ID,
            SQL_CREATE_TRIGGER_UPDATE,
            SQL_CREATE_TABLE_TOKENS,
            SQL_CREATE_INDEX_TOKEN,
            SQL_CREATE_INDEX_USER_SESSIONS,
            //
            SQL_CREATE_TABLE_FRIENDS,
            SQL_CREATE_TABLE_GROUPS,
            SQL_CREATE_TABLE_MESSAGES
            // SQL_CREATE_TABLE_GROUP_MESSAGES,
        };
        for(const auto& cmd : table_creation_commands) {
            auto result = execute(cmd.data());
            if(!result || !result->error_message.empty()) {
                LOG_ERROR("Failed to execute table creation command: {}, error: {}", cmd, result ? result->error_message : "null result");
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
        }
        else {
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
    // 用户注册
    std::shared_ptr<::TaoTieChat::SignUpReply> sign_up(std::shared_ptr<::TaoTieChat::SignUpRequest> request) {
        using Request = ::TaoTieChat::SignUpRequest;
        using Reply   = ::TaoTieChat::SignUpReply;
        using sc      = ::TaoTieChat::SuccessCode;
        using ec      = ::TaoTieChat::ErrorCode;

        auto set_success = [&](std::shared_ptr<Reply>&reply, sc code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_success_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
            reply->set_account_id("");
        };
        auto set_error = [&](std::shared_ptr<Reply>&reply, ec code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_error_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
            reply->set_account_id("");
        };

        // ================================================================== //

        using namespace SQL::COMMANDS::PRAGMA;
        using namespace SQL::COMMANDS::CREATE;
        using namespace SQL::COMMANDS::QUERY;
        using namespace SQL::COMMANDS::INSERT;

        // 注册用户，返回是否注册成功
        // 1. 检查账号是否已存在，如果已存在则返回失败
        // 2. 如果账号不存在则插入新记录，并返回成功
        std::shared_ptr<Reply> reply = std::make_shared<Reply>();
        set_error(reply, ec::ERROR_INIT);
        if(!request) {
            return reply;
        }
        reply->mutable_request()->CopyFrom(*request.get());

        // 参数校验
        if(request->user_id().empty()) {
            set_error(reply, ec::SIGN_UP_USER_ID_EMPTY);
            LOG_DEBUG("User ID is empty, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        if(request->password().empty()) {
            set_error(reply, ec::SIGN_UP_PASSWORD_EMPTY);
            LOG_DEBUG("Password is empty, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        if(request->security_question().empty()) {
            set_error(reply, ec::SIGN_UP_SECURITY_QUESTION_EMPTY);
            LOG_DEBUG("Security question is empty, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        if(request->security_answer().empty()) {
            set_error(reply, ec::SIGN_UP_SECURITY_ANSWER_EMPTY);
            LOG_DEBUG("Security answer is empty, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 实现用户注册逻辑
        std::string account_id;
        std::shared_ptr<WrapperQueryResult> query_account_exists;
        std::shared_ptr<WrapperQueryResult> query_user_exists;
        std::shared_ptr<WrapperQueryResult> insert_result;

        // 检查用户 ID 是否已存在
        auto query_user_exists_params = std::make_shared<WrapperNamedParam>();
        query_user_exists_params->params[":user_id"] = request->user_id();
        query_user_exists = execute(SQL_QUERY_USER_EXISTS.data(), query_user_exists_params);

        // 操作失败
        if(!query_user_exists->done) {
            set_error(reply, ec::DB_QUERY);
            LOG_ERROR("Query user exists failed, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 已经有记录
        if(!query_user_exists->results.empty()) {
            set_error(reply, ec::SIGN_UP_USER_ID_EXISTS);
            LOG_ERROR("User ID already exists, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 生成账号 uuid // 操作失败或生成的 uuid 已经存在则重试，直到成功且不存在为止，理论上应该不会出现重复的 uuid，但为了保险起见加上这个检查
        auto query_account_exists_params = std::make_shared<WrapperNamedParam>();
        do {
            account_id = Utils::to_string(Utils::generate_uuid());
            query_account_exists_params->params[":account_id"] = account_id;
            query_account_exists = execute(SQL_QUERY_ACCOUNT_EXISTS.data(), query_account_exists_params);
        } while(!query_account_exists->done || !query_account_exists->results.empty());

        // 获取注册时间
        auto register_time        = Utils::current_time_point();
        // 不管用户端是否加密，服务端都对密码进行哈希加密存储
        auto password_hash        = Utils::hash_password(request->password());
        // 由于需要校验，因此手动生成便于比对
        auto register_time_utc    = Utils::to_utc_string(register_time);
        // 安全问题答案也进行哈希加密存储，避免明文存储安全问题答案的风险
        auto security_answer_hash = Utils::hash_security_answer(request->security_answer());

        // 构造参数
        auto user_register_params = std::make_shared<WrapperNamedParam>();
        user_register_params->params[":account_id"       ] = account_id;
        user_register_params->params[":user_id"          ] = request->user_id();
        user_register_params->params[":password_hash"    ] = password_hash;
        user_register_params->params[":security_question"] = request->security_question();
        user_register_params->params[":security_answer"  ] = request->security_answer();
        user_register_params->params[":register_time"    ] = register_time_utc;

        // 插入新记录
        insert_result = execute(SQL_INSERT_USER_REGISTER.data(), user_register_params);
        // 注册命令执行失败
        if(!insert_result->done) {
            set_error(reply, ec::DB_EXECUTE);
            LOG_ERROR("Insert user register record failed, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 注册成功时需要校验
        auto insert_result_check = execute(SQL_QUERY_USER_REGISTER_CHECK.data(), user_register_params);
        // 校验命令执行失败
        if(!insert_result_check->done) {
            set_error(reply, ec::DB_CHECK);
            LOG_ERROR("Check user register record failed, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 校验命令执行成功，但返回的结果为空
        if(insert_result_check->results.empty()) {
            set_error(reply, ec::DB_CHECK_EMPTY);
            LOG_ERROR("Check user register record failed, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 注册成功
        set_success(reply, sc::SUCCESS);
        reply->set_account_id(account_id);
        LOG_INFO("User registered successfully, account_id: {}, user_id: {}", reply->account_id(), request->user_id());
        return reply;
    }

    // 用户登录
    std::shared_ptr<::TaoTieChat::SignInReply> sign_in(std::shared_ptr<::TaoTieChat::SignInRequest> request) {
        using Request = ::TaoTieChat::SignInRequest;
        using Reply   = ::TaoTieChat::SignInReply;
        using sc      = ::TaoTieChat::SuccessCode;
        using ec      = ::TaoTieChat::ErrorCode;

        auto set_success = [&](std::shared_ptr<Reply>&reply, sc code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_success_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
            reply->set_account_id("");
            reply->set_token("");
        };
        auto set_error = [&](std::shared_ptr<Reply>&reply, ec code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_error_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
            reply->set_account_id("");
            reply->set_token("");
        };

        // ================================================================== //

        using namespace SQL::COMMANDS::PRAGMA;
        using namespace SQL::COMMANDS::CREATE;
        using namespace SQL::COMMANDS::QUERY;
        using namespace SQL::COMMANDS::INSERT;

        // 用户登录验证，返回是否登录成功
        // 1. 根据用户 ID 查询用户记录，如果没有找到则返回失败
        // 2. 如果找到了用户记录则比较密码和安全问题答案，如果匹配则返回成功，否则返回失败
        std::shared_ptr<Reply> reply = std::make_shared<Reply>();
        set_error(reply, ec::ERROR_INIT);
        if(!request) {
            return reply;
        }
        reply->mutable_request()->CopyFrom(*request.get());

        // 参数校验
        if(request->user_id().empty() ) {
            set_error(reply, ec::SIGN_UP_USER_ID_EMPTY);
            LOG_DEBUG("User ID is empty, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 参数校验
        bool has_token    = request->has_token   ();
        bool has_password = request->has_password();
        if(!has_token && !has_password) {
            set_error(reply, ec::SIGN_IN_INVALID_LOGIN_METHOD);
            LOG_DEBUG("Invalid login method is used, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        std::shared_ptr<WrapperQueryResult> query_user_login_auth;
        std::shared_ptr<WrapperQueryResult> insert_result;
        std::string req_token_hash    = !has_token    ? std::string() : Utils::hash_token   (request->token   ());
        std::string req_password_hash = !has_password ? std::string() : Utils::hash_password(request->password());
        std::string sql_query_user_login;  // token 或密码查询语句
        if(has_token) {
            sql_query_user_login = SQL_QUERY_TOKEN_VALIDITY_BY_USER_ID;
        }
        else if(has_password) {
            sql_query_user_login = SQL_QUERY_USER_LOGIN_BY_PASSWORD;
        }

        // 实现用户登录验证逻辑
        // 构造参数
        auto user_login_params = std::make_shared<WrapperNamedParam>();
        user_login_params->params[":user_id"      ] = request->user_id();
        user_login_params->params[":token_hash"   ] = req_token_hash;
        user_login_params->params[":password_hash"] = req_password_hash;
        // 查询用户记录  // 根据用户 ID、密码，查询账号 ID
        query_user_login_auth = execute(sql_query_user_login, user_login_params);

        // 查询命令执行失败
        if(!query_user_login_auth->done) {
            set_error(reply, ec::DB_QUERY);
            LOG_ERROR("Query user login record failed, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 查询命令执行成功，但返回的结果为空  // 用户名或密码错误
        if(query_user_login_auth->results.empty()) {
            LOG_DEBUG("DB query_user_login_auth result empty, message: {}", query_user_login_auth->error_message);
            set_error(reply, ec::SIGN_IN_AUTH_FAILED);
            LOG_ERROR("User ID or Password or Token error, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 获取账号 ID
        std::string account_id = std::get<std::string>(query_user_login_auth->results[0]["account_id"]);
        // 生成 token 并记录登陆请求
        // 1. 暂不校验唯一性
        // 2. 不存储明文防止泄露
        // 建议加入时间戳等信息后再 sha256 存储，或者使用 JWT 等标准格式的 token，以便后续验证和解析
        std::string token      = Utils::generate_token();
        std::string token_hash = Utils::hash_token(token);
        // 需要校验是否写入成功时，则需要保存该变量
        auto create_time       = Utils::current_time_point();
        auto create_time_str   = Utils::to_utc_string(create_time);
        // token 过期时间，暂定为 30 天
        // TODO:再次被使用时需要更新该过期时间
        auto expire_time       = create_time + std::chrono::days(30);
        auto expire_time_str   = Utils::to_utc_string(expire_time);

        // 记录登陆请求
        auto login_record_params = std::make_shared<WrapperNamedParam>();
        login_record_params->params[":account_id" ] = account_id;
        login_record_params->params[":token_hash" ] = token_hash;
        login_record_params->params[":create_time"] = create_time_str;
        login_record_params->params[":expire_time"] = expire_time_str;
        insert_result = execute(SQL_INSERT_USER_LOGIN_RECORD.data(), login_record_params);

        // 记录登陆请求命令执行失败
        if(!insert_result->done) {
            set_error(reply, ec::DB_EXECUTE);
            LOG_ERROR("Insert user login record failed, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 登录成功
        set_success(reply, sc::SUCCESS);
        reply->set_account_id(account_id);
        reply->set_token(token);
        return reply;
    }

    // 搜索用户
    std::shared_ptr<::TaoTieChat::SearchUsersReply> search_users(std::shared_ptr<::TaoTieChat::SearchUsersRequest> request) {
        using Request = ::TaoTieChat::SearchUsersReply;
        using Reply   = ::TaoTieChat::SearchUsersReply;
        using sc      = ::TaoTieChat::SuccessCode;
        using ec      = ::TaoTieChat::ErrorCode;

        auto set_success = [&](std::shared_ptr<Reply>&reply, sc code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_success_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
        };
        auto set_error = [&](std::shared_ptr<Reply>&reply, ec code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_error_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
        };

        // ================================================================== //

        using namespace SQL::COMMANDS::PRAGMA;
        using namespace SQL::COMMANDS::CREATE;
        using namespace SQL::COMMANDS::QUERY;
        using namespace SQL::COMMANDS::INSERT;

        // 复制请求
        std::shared_ptr<Reply> reply = std::make_shared<Reply>();
        set_error(reply, ec::ERROR_INIT);
        if(!request) {
            return reply;
        }
        reply->mutable_request()->CopyFrom(*request.get());

        // 参数校验
        if(request->users_keyword().empty()) {
            set_error(reply, ec::SEARCH_USERS_KEYWORD_EMPTY);
            LOG_DEBUG("Users keyword is empty, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 获取参数
        std::string users_keyword = request->users_keyword();  // 暂时同义于 user_id
        std::string users_filter  = request->users_filter ();  // 暂时无效
        std::string&user_id       = users_keyword;

        // 构造数据库查询参数
        auto search_users_params = std::make_shared<WrapperNamedParam>();
        search_users_params->params[":user_id"] = user_id;
        // 执行数据库查询命令  // 传入查询语句及相关参数
        std::shared_ptr<WrapperQueryResult> query_search_users = execute(SQL_QUERY_SEARCH_USERS.data(), search_users_params);

        // 数据库查询命令执行失败
        if(!query_search_users->done) {
            set_error(reply, ec::DB_QUERY);
            LOG_ERROR("Query users keyword failed, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 数据库查询命令执行成功，但没有有效数据  // 返回的结果为空
        if(query_search_users->results.empty()) {
            LOG_DEBUG("DB query_search_users result empty, message: {}", query_search_users->error_message);
            set_error(reply, ec::SEARCH_USERS_KEYWORD_NOT_FOUND);
            LOG_ERROR("Search Users empty, code: {:#010x}, message: {}", (int)reply->mutable_result()->error_code(), reply->mutable_result()->message());
            return reply;
        }

        // 查询命令执行成功且有有效数据  // 返回的结果不为空，至少包含一条有效数据
        set_success(reply, sc::SUCCESS);
        for(auto&db_user_info : query_search_users->results) {
            auto reply_user_info = reply->add_user_info();
            auto ite_end         = db_user_info.end();
            auto ite_user_id     = db_user_info.find("user_id" );
            auto ite_nickname    = db_user_info.find("nickname");
            auto ite_avatar      = db_user_info.find("avatar"  );
            auto ite_sign        = db_user_info.find("sign"    );
            auto ite_gender      = db_user_info.find("gender"  );
            auto ite_region      = db_user_info.find("region"  );
            auto ite_birthday    = db_user_info.find("birthday");
            auto ite_phones      = db_user_info.find("phones"  );
            auto ite_emails      = db_user_info.find("emails"  );
            if(ite_user_id  != ite_end) { auto point_value = std::get_if<std::string>(&ite_user_id ->second); if(point_value) { reply_user_info->set_user_id (*point_value); } }
            if(ite_nickname != ite_end) { auto point_value = std::get_if<std::string>(&ite_nickname->second); if(point_value) { reply_user_info->set_nickname(*point_value); } }
            if(ite_avatar   != ite_end) { auto point_value = std::get_if<std::string>(&ite_avatar  ->second); if(point_value) { reply_user_info->set_avatar  (*point_value); } }
            if(ite_sign     != ite_end) { auto point_value = std::get_if<std::string>(&ite_sign    ->second); if(point_value) { reply_user_info->set_sign    (*point_value); } }
            if(ite_gender   != ite_end) { auto point_value = std::get_if<int32_t    >(&ite_gender  ->second); if(point_value) { reply_user_info->set_gender  (*point_value); } }
            if(ite_region   != ite_end) { auto point_value = std::get_if<std::string>(&ite_region  ->second); if(point_value) { reply_user_info->set_region  (*point_value); } }
            if(ite_birthday != ite_end) { auto point_value = std::get_if<std::string>(&ite_birthday->second); if(point_value) { reply_user_info->set_birthday(*point_value); } }
            if(ite_phones   != ite_end) { auto point_value = std::get_if<std::string>(&ite_phones  ->second); if(point_value) { for(auto&phone : Utils::split(*point_value, ";")) { reply_user_info->add_phones(phone); } } }
            if(ite_emails   != ite_end) { auto point_value = std::get_if<std::string>(&ite_emails  ->second); if(point_value) { for(auto&email : Utils::split(*point_value, ";")) { reply_user_info->add_emails(email); } } }
        }
        return reply;
    }

private:
    std::filesystem::path m_db_path;
    sqlite3*              m_db_sqlcipher = nullptr;
    std::string           m_encryption_key;
    SQLiteWrapper         m_db_wrapper;  // 数据库操作包装类实例
};
