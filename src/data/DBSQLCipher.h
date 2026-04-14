#pragma once

#include "headers.h"
#include "utils.h"
#include "types.h"

using std::string;
using std::variant;
using std::nullptr_t;
using std::map;
using std::vector;
using std::list;
using std::pair;
using std::shared_ptr;
using std::make_shared;
using std::string_view;
using std::filesystem::path;
using std::filesystem::absolute;
using std::filesystem::exists;
using std::filesystem::create_directories;
using std::visit;
using std::decay_t;
using std::is_same_v;
using std::format;
using std::chrono::days;
using std::get_if;

using namespace SQL::COMMANDS::PRAGMA;
using namespace SQL::COMMANDS::CREATE;
using namespace SQL::COMMANDS::QUERY;
using namespace SQL::COMMANDS::INSERT;

using ::TaoTieChat::SignUpRequest;
using ::TaoTieChat::SignUpReply;
using ::TaoTieChat::SuccessCode;
using ::TaoTieChat::ErrorCode;
using ::TaoTieChat::AuthRequest;
using ::TaoTieChat::AuthReply;
using ::TaoTieChat::SignUpRequest;
using ::TaoTieChat::SignUpReply;
using ::TaoTieChat::Contact;
using ::TaoTieChat::Contacts;
using ::TaoTieChat::SearchUsersRequest;
using ::TaoTieChat::SearchUsersReply;

using Utils::set_success;
using Utils::set_error;
using Utils::to_string;
using Utils::hash_token;
using Utils::hash_password;
using Utils::hash_security_answer;
using Utils::generate_uuid;
using Utils::generate_account_id;
using Utils::generate_token;
using Utils::current_time_point;
using Utils::to_utc_string;
using Utils::split;
using Utils::success_reply;
using Utils::error_reply;

// =========================================================================== //

// 定义一个简单的多类型值容器（C++17）
using SqlValue  = variant<int, double, string, nullptr_t>;  // 0,1,2,3 分别表示 int、double、string、null
using SqlParams = map<string, SqlValue>;  // 参数其实不必保留顺序
using SqlRow    = map<string, SqlValue>;  // 保留顺序
using SqlRows   = vector<SqlRow>;  // 保留顺序

static std::string to_string(const SqlValue& v) {
    // 1. 转换
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>)
            return std::to_string(arg);
        else if constexpr (std::is_same_v<T, double>)
            return std::to_string(arg);
        else if constexpr (std::is_same_v<T, std::string>)
            return arg;
        else if constexpr (std::is_same_v<T, std::nullptr_t>)
            return "null"; // 或者返回空字符串 ""
        else
            return "";
    }, v);
    // 2. 需确保所有类型都支持 format，或者在 visit 里包装  // C++ 20
    return std::visit([](auto&& arg) {
        return std::format("{}", arg);
    }, v);
}

static string get_string(const SqlValue& v, const string&default_value = string()) {
    if (auto pVal = std::get_if<std::string>(&v)) {
        return *pVal; // 提取成功
    }
    return default_value;
}

static int get_int(const SqlValue& v, const int default_value = -1) {
    if (auto pVal = std::get_if<int>(&v)) {
        return *pVal; // 提取成功
    }
    return default_value;
}

struct SQLResult{
    bool    done = false;  // 仅表示是否执行完成，不代表是否成功
    string  msg ;  // 错误消息
    SqlRows rows;  // 列名 - 列值
    SQLResult(bool done = false, const string& msg = "", const SqlRows& rows = {})
        : done(done), msg(msg), rows(rows) {}
};

class DBSQLCipher {
private:
    DBSQLCipher() = default;
    DBSQLCipher(const DBSQLCipher&) = default;
    DBSQLCipher& operator=(const DBSQLCipher&) = default;
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
    void set_init_params(const path& db_path, const string& encryption_key) {
        m_db_path        = db_path       ;
        m_encryption_key = encryption_key;
    }

    // 打开数据库并进行初始化验证
    // 初始化仅写入 version 信息
    bool database_open() {
        // 1. 检查句柄，如果不为空则表示已经正确打开并已经被初始化
        // 2. 句柄为空则使用密码打开，并读取数据版本信息
        // 3. 读取版本失败时进行创建表和写入版本信息的初始化操作
        // 4. 再次尝试读取版本号，失败则报错

        // 密码不正确也可以打开句柄，但无法读出及写入数据，因此我们通过读写数据来验证密码是否正确

        path        absolute_db_path = absolute(m_db_path);
        path        parent_path      = absolute_db_path.parent_path();
        string      str_db_path      = absolute_db_path.string     ();  // 必须，否则 c_str() 返回的指针会指向一个临时对象，导致后续使用时访问无效内存
        const char* sz_db_path       = str_db_path     .c_str      ();

        // 句柄打开则认为已经初始化，不再校验
        if(m_db_sqlcipher) {
            LOG_TRACE("Database is already open: {}", str_db_path);
            return true;
        }
        LOG_INFO("Database is not open, attempting to open: {}", str_db_path);

        // SQLCipher 不会自动创建父文件夹，需要手动创建
        if(!exists(parent_path)) {
            create_directories(parent_path);
            if(!exists(parent_path)) {
                // 创建文件夹失败
                LOG_ERROR("Creating database parent directory failed. path: {}", parent_path.string());
                return false;
            }
            // 创建文件夹成功
            LOG_TRACE("Creating database parent directory successed. path: {}", parent_path.string());
        }

        // 尝试打开数据库，如果数据库文件不存在，sqlite3_open 会创建一个新的空数据库文件
        int rc = sqlite3_open(sz_db_path, &m_db_sqlcipher);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Can't open database. msg: {}", sqlite3_errmsg(m_db_sqlcipher));
            m_db_sqlcipher = nullptr;
            return false;
        }
        LOG_TRACE("Opened database successfully");

        // PRAGMA key 不支持参数绑定，但使用字符串依然会导致数据库注入
        rc = sqlite3_key(m_db_sqlcipher, m_encryption_key.c_str(), (int)m_encryption_key.size());
        if(rc != SQLITE_OK) {
            LOG_ERROR("Failed to set encryption key. msg: {}", sqlite3_errmsg(m_db_sqlcipher));
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }
        // 仅表示执行成功，并不代表密钥正确，后续通过读写验证密钥是否正确
        LOG_TRACE("Encryption key set successfully, not check rw.");

#ifdef DEBUG
        std::ofstream debug_key(str_db_path + ".key", std::ios::out | std::ios::trunc);
        if(debug_key.is_open()) {
            debug_key << "debug key : " << m_encryption_key << std::endl;
            debug_key.close();
        }
#endif // DEBUG

        // 验证数据库秘钥是否设置正确时必须对数据库进行读写操作，无法直接从返回值判断
        // 读写 meta version 作为数据库秘钥设置是否成功的验证

        // 如果直接获取到有效信息，则认为数据库秘钥设置正确，直接返回成功
        auto [success_get_meta_version, meta_version] = get_meta_version();
        if(!success_get_meta_version) {
            // 读取失败时忽略错误，尝试执行写入，不返回
        }
        // 执行成功且版本不为空
        if(!meta_version.empty()) {
            LOG_TRACE("Encryption key set successfully by check meta version.");
            return true;
        }
        // 执行失败或版本为空
        auto[success_write_meta_version, result_write_meta_version] = write_meta_version();
        if(!success_write_meta_version) {
            // 写入失败时则说明数据库密码设置不正确
            LOG_ERROR("Failed to set encryption key. msg: {}", sqlite3_errmsg(m_db_sqlcipher));
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }
        if(result_write_meta_version) {
            // 写入成功无需校验
            LOG_TRACE("Encryption key set successfully by verified meta version.");
            return true;
        }

        LOG_ERROR("Database initialization verified failed!");
        sqlite3_close(m_db_sqlcipher);
        m_db_sqlcipher = nullptr;
        return false;
    }

    bool database_initialize() {
        vector<string_view> table_creation_commands = {
            // SQL_CREATE_TABLE_META         ,
            SQL_CREATE_TABLE_USERS        ,
            SQL_CREATE_INDEX_ACCOUNT_ID   ,
            SQL_CREATE_INDEX_USER_ID      ,
            SQL_CREATE_TRIGGER_UPDATE     ,
            SQL_CREATE_TABLE_TOKENS       ,
            SQL_CREATE_INDEX_TOKEN        ,
            SQL_CREATE_INDEX_USER_SESSIONS,
            //
            SQL_CREATE_TABLE_FRIENDS ,
            SQL_CREATE_TABLE_GROUPS  ,
            SQL_CREATE_TABLE_MESSAGES
            // SQL_CREATE_TABLE_GROUP_MESSAGES,
        };
        do{
            for(const auto& cmd : table_creation_commands) {
                auto sql_result = execute(cmd.data());
                if(!sql_result || !sql_result->done) {
                    auto msg = sql_result ? sql_result->msg : "null result";
                    LOG_ERROR("Failed to execute table creation command: {}, message: {}", cmd, msg);
                    return false;
                }
            }
            LOG_INFO("All tables created successfully");
            return true;
        }while(false);
        return false;
    }

    bool database_close() {
        if (m_db_sqlcipher) {
            int rc = sqlite3_close(m_db_sqlcipher);
            if (rc == SQLITE_OK) {
                // 关闭成功
                m_db_sqlcipher = nullptr;
                LOG_INFO("Closed database connection");
            }
            else if (rc == SQLITE_ERROR) {
                // 关闭失败，可能是因为数据库文件损坏或其他 I/O 错误
                LOG_ERROR("Failed to close database connection. msg: {}", sqlite3_errmsg(m_db_sqlcipher));
                return false;
            }
            else if (rc == SQLITE_BUSY) {
                // 还有未完成的 statement（查询）或未关闭的 blob 句柄
                LOG_ERROR("Failed to close database connection because it is busy. msg: {}", sqlite3_errmsg(m_db_sqlcipher));
                LOG_ERROR("Please Check Code Ensure All Statements Are Finalized.");
                LOG_ERROR("Call sqlite3_finalize(stmt) for every sqlite3_prepare_v2() statement. ");
                LOG_ERROR("Unfinalized statements will keep the database connection busy and prevent it from closing.");
                return false;
            }
            else {
                // 其他未知错误
                LOG_ERROR("Failed to close database connection with unknown error code {}. msg: {}", rc, sqlite3_errmsg(m_db_sqlcipher));
                return false;
            }
        }
        return true; // 数据库未打开
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

    // 执行查询并将结果存入 vector<map>
    shared_ptr<SQLResult> execute(const string_view& sql, const shared_ptr<SqlParams> params = nullptr) {
        // 辅助：根据 variant 类型执行绑定
        auto bind_variant = [](sqlite3_stmt* stmt, int idx, const SqlValue& v)->void {
            visit([&](auto&& arg) {
                using T = decay_t<decltype(arg)>;
                ;    if constexpr (is_same_v<T, int      >) sqlite3_bind_int   (stmt, idx, arg);
                else if constexpr (is_same_v<T, double   >) sqlite3_bind_double(stmt, idx, arg);
                else if constexpr (is_same_v<T, string   >) sqlite3_bind_text  (stmt, idx, arg.c_str(), -1, SQLITE_TRANSIENT);
                else if constexpr (is_same_v<T, nullptr_t>) sqlite3_bind_null  (stmt, idx);
            }, v);
        };
        // 辅助：从列中提取数据存入 variant
        auto extract_column = [](sqlite3_stmt* stmt, int idx)->SqlValue {
            int type = sqlite3_column_type(stmt, idx);
            if (type == SQLITE_INTEGER) { return sqlite3_column_int   (stmt, idx); }
            if (type == SQLITE_FLOAT)   { return sqlite3_column_double(stmt, idx); }
            if (type == SQLITE_TEXT)    { return string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx))); }
            return nullptr;
        };

        //
        auto results = make_shared<SQLResult>();
        if(!m_db_sqlcipher) {
            results->done = false;
            results->msg  = "Database not opened.";
            return results;
        }
        if(sql.empty()) {
            results->done = false;
            results->msg  = "sql is empty.";
            return results;
        }
        // **每次调用 `sqlite3_stmt` 都必须调用 `sqlite3_finalize()` **来清理，
        // 否则 `sqlite3_close(db) == SQLITE_BUSY`，无法关闭数据库
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(m_db_sqlcipher, sql.data(), -1, &stmt, nullptr) != SQLITE_OK) {
            results->done = false;
            results->msg  = sqlite3_errmsg(m_db_sqlcipher);
            sqlite3_finalize(stmt);
            return results;
        }

        // 命名参数绑定
        if (params) {
            for (auto const& [name, value] : *params) {
                int index = sqlite3_bind_parameter_index(stmt, name.c_str());
                // 只有在 SQL 中找到了该名称才绑定
                if (index > 0) {
                    bind_variant(stmt, index, value);
                }
                else {
                    LOG_TRACE("Parameter name '{}' not found in SQL statement, skipping binding.", name);
                }
            }
        }

        int rc = -1;  // SQLITE_ROW 未读完，还有数据可读
        // 执行语句并保存结果  // 数据已经写入，无需读取验证
        // while (sqlite3_step(stmt) == SQLITE_ROW) {
        for(int loo_count = 0; loo_count <= SQL_RESULTS_MAX_LINES; ++loo_count) {
            rc = sqlite3_step(stmt);
            if(rc != SQLITE_ROW) {
                break;
            }
            SqlRow row;
            for (int i = 0; i < sqlite3_column_count(stmt); i++) {
                row[sqlite3_column_name(stmt, i)] = extract_column(stmt, i);
            }
            results->rows.emplace_back(row);
        }
        if(rc == SQLITE_DONE) {
            // 执行过程没有错误
            results->done = true;
            sqlite3_finalize(stmt);
            return results;
        }
        else {
            if(rc == SQLITE_ROW) {
                LOG_WARNING("SQL execute result is large.");
            }
            // 执行过程发生错误
            results->done = false;
            results->msg  = sqlite3_errmsg(m_db_sqlcipher);
            sqlite3_finalize(stmt);
            return results;
        }

        results->done = false;
        sqlite3_finalize(stmt);
        return results;
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
    // ================== 业务层不要包含 SQL 语句与参数构造 ================== //

    // 获取在线状态
    shared_ptr<StatusReply> get_status(shared_ptr<StatusRequest> request) {
#ifdef DEBUG
        // LOG_DEBUG("Received get_status request: {}", request ? request->DebugString() : "null");
        std::cout << "Received get_status request: " << (request ? request->DebugString() : "null") << std::endl;
#endif // DEBUG
        using Request = StatusRequest;
        using Reply   = StatusReply;
        // =========================== 初始化操作 =========================== //
        // 注册用户，返回是否注册成功
        // 1. 检查账号是否已存在，如果已存在则返回失败
        // 2. 如果账号不存在则插入新记录，并返回成功
        shared_ptr<Reply> reply = make_shared<Reply>();
        set_error(reply->mutable_result(), ErrorCode::ERROR_INIT);
        if(!request) {
            return reply;
        }
        reply->mutable_request()->CopyFrom(*request.get());
        set_error(reply->mutable_result(), ErrorCode::UNKNOWN);
        // =========================== 参数校验 =========================== //
        if(request->target_user_id().empty()) {
            return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_EMPTY, std::format("Target user ID is required."));
        }
        if(!request->has_request_user_auth()) {
            return error_reply<Reply>(reply, ErrorCode::AUTH_INFO_NONE, std::format("Auth info is required."));
        }
        shared_ptr<AuthRequest> auth_request = make_shared<AuthRequest>(request->request_user_auth());
#ifdef DEBUG
        if(!auth_request->has_user_id() || auth_request->user_id().empty()) {
            return error_reply<Reply>(reply, ErrorCode::USER_ID_EMPTY, std::format("User id is required."));
        }
        if(!auth_request->has_token() || auth_request->token().empty()) {
            return error_reply<Reply>(reply, ErrorCode::TOKEN_EMPTY, std::format("Token id is required."));
        }
#else
        auto auth_reply  = auth_check(auth_request);
        auto&auth_result = auth_reply->result();
        if(!auth_result.has_success_code()) {
            return error_reply<Reply>(reply, auth_result.error_code(), auth_result.message());
        }
#endif // DEBUG
        // =========================== 执行操作 =========================== //
#ifdef DEBUG
        // test
        {
            // id 和 token 都不为空时返回用户的状态信息
            // 取出参数
            auto target_user_id     = request->target_user_id();
            auto request_user_id    = request->request_user_auth().user_id();
            auto request_user_token = request->request_user_auth().token();
            auto cache_request_user_tokens = std::set<string>();
            // 用户 token 缓存
            static map<string, std::set<string> > cache_request_users_tokens = {
                {"user1", {"token1", "token11"}},
                {"user2", {"token2"}},
                {"user3", {"token3"}},
            };
            // 用户在线状态缓存
            static map<string, string> cache_target_users_status = {
                {"user1", "online"},
                {"user2", "offline"},
                {"user3", "away"},
            };
            do{
                // 查找用户 ID 是否存在
                auto cache_request_user_tokens_it = cache_request_users_tokens.find(request_user_id);
                if (cache_request_user_tokens_it == cache_request_users_tokens.end()) {
                    return error_reply<Reply>(reply, ErrorCode::USER_ID_NOT_FOUND, std::format("Request User ID not found."));
                }
                // 获取用户 tokens 并校验传入 token 的合法性  // 同一用户可以有多个 token
                cache_request_user_tokens = cache_request_user_tokens_it->second;
                if (cache_request_user_tokens.find(request_user_token) == cache_request_user_tokens.end()) {
                    return error_reply<Reply>(reply, ErrorCode::TOKEN_NOT_FOUND, std::format("User token is invalid."));
                }
                // 查找目标用户是否有状态缓存
                auto cache_target_users_status_it = cache_target_users_status.find(target_user_id);
                if (cache_target_users_status_it == cache_target_users_status.end()) {
                    return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_NOT_FOUND, std::format("Target User ID not found."));
                }
                // 成功  // 返回目标用户的状态缓存
                else {
                    reply->set_status(cache_target_users_status_it->second);
                    return success_reply<Reply>(
                        reply,
                        SuccessCode::STATUS_SUCCESS,
                        std::format(
                            "Get status successed. "
                            "Target ID: {}, status: {}, request:{}",
                            target_user_id,
                            cache_target_users_status_it->second,
                            request->DebugString()));
                }
            } while(false);
            // 默认失败
            return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
        }
#endif // DEBUG
        LOG_ERROR("TODO:get_status");
        // 默认失败
        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 用户注册
    shared_ptr<SignUpReply> sign_up(shared_ptr<SignUpRequest> request) {
        using Request = SignUpRequest;
        using Reply   = SignUpReply;
        // =========================== 初始化操作 =========================== //
        // 注册用户，返回是否注册成功
        // 1. 检查账号是否已存在，如果已存在则返回失败
        // 2. 如果账号不存在则插入新记录，并返回成功
        shared_ptr<Reply> reply = make_shared<Reply>();
        set_error(reply->mutable_result(), ErrorCode::ERROR_INIT);
        if(!request) {
            return reply;
        }
        reply->mutable_request()->CopyFrom(*request.get());
        set_error(reply->mutable_result(), ErrorCode::UNKNOWN);
        // =========================== 参数校验 =========================== //
        // 用户 ID 不能为空
        if(request->user_id().empty()) {
            return error_reply<Reply>(reply, ErrorCode::USER_ID_EMPTY, std::format("User ID is empty."));
        }
        // 密码不能为空
        if(request->password().empty()) {
            return error_reply<Reply>(reply, ErrorCode::PASSWORD_EMPTY, std::format("Password is empty."));
        }
        // 密码与确认密码不同
        if(request->password() != request->confirm()) {
            return error_reply<Reply>(reply, ErrorCode::DIFF_PASSWORD_WITH_CONFIRM, std::format("Password diff with confirm."));
        }
        // 密保问题不能为空
        if(request->security_question().empty()) {
            return error_reply<Reply>(reply, ErrorCode::SECURITY_QUESTION_EMPTY, std::format("Security question is empty."));
        }
        // 密保答案不能为空
        if(request->security_answer().empty()) {
            return error_reply<Reply>(reply, ErrorCode::SECURITY_ANSWER_EMPTY, std::format("Security answer is empty."));
        }
        // =========================== 执行操作 =========================== //
        auto user_id = request->user_id();

        auto [success_search_active_user_id, result_search_active_user_id] = search_active_user_id(user_id);
        // 查询失败直接返回错误
        if(!success_search_active_user_id) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Can not search user_id. user_id:{}.", request->user_id()));
        }
        // 查询成功但用户已存在
        if(result_search_active_user_id) {
            return error_reply<Reply>(reply, ErrorCode::USER_ID_EXISTS, std::format("Can not register user_id:{}.", request->user_id()));
        }

        auto [success_user_register, account_id] = user_register(user_id, request->password(), request->security_question(), request->security_answer());
        if(!success_user_register) {
            return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("User register error."));
        }
        if(!account_id.empty()) {
            // 执行成功无需验证
            reply->set_account_id(account_id);
            return success_reply<Reply>(
                reply,
                SuccessCode::SUCCESS,
                std::format(
                    "User registered successfully."
                    " account_id: {}, user_id: {}.",
                    account_id,
                    user_id));
        }

        // 默认失败
        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 密码或 token 验证
    shared_ptr<AuthReply> auth_check(shared_ptr<AuthRequest> request) {
        using Request = AuthRequest;
        using Reply   = AuthReply;
        // =========================== 初始化操作 =========================== //
        // 鉴权，仅返回认证是否成功
        // 返回账号 ID 与 token 属于登陆接口的功能
        shared_ptr<Reply> reply = make_shared<Reply>();
        set_error(reply->mutable_result(), ErrorCode::ERROR_INIT);
        if(!request) {
            return error_reply<Reply>(reply, ErrorCode::ERROR_INIT, std::format("Invalid Param."));
        }
        reply->mutable_request()->CopyFrom(*request.get());
        set_error(reply->mutable_result(), ErrorCode::UNKNOWN);
        // =========================== 参数校验 =========================== //
        // 不允许空凭证：
        //   * 必须包含字符串，且字符串去除无效字符外不能为空
        //   * 字符串中的任何位置都不允许包含空格等字符
        // std::optional<std::string> user_id = std::nullopt;
        // std::optional<std::string> user_id = {};
        string user_id    = !request->has_user_id   () ? string() : request->user_id   ();
        string account_id = !request->has_account_id() ? string() : request->account_id();
        string password   = !request->has_password  () ? string() : request->password  ();
        string token      = !request->has_token     () ? string() : request->token     ();
        // 未提供用户名或账户
        if(user_id.empty() && account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::AUTH_ID_EMPTY, std::format("Auth ID is empty."));
        }
        // 未提供密码或 token
        if(password.empty() && token.empty()) {
            return error_reply<Reply>(reply, ErrorCode::AUTH_METHOD_EMPTY, std::format("Auth Method is empty."));
        }
        // =========================== 执行操作 =========================== //
        // 将 user_id 转换为 account_id
        if(account_id.empty() && !user_id.empty()) {
            auto[success_get_active_account_id, result_account_id] = get_active_account_id(user_id);
            if(!success_get_active_account_id) {
                return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Get active account ID error, user_id:{}.", user_id));
            }
            // 用户或账号 ID 未注册或停用等，返回账号不存在即可
            if(result_account_id.empty()) {
                return error_reply<Reply>(reply, ErrorCode::USER_ID_NOT_FOUND, std::format("user_id maybe not register, user_id:{}.", user_id));
            }
            account_id = result_account_id;
        }
        else {
            // 校验账号 ID 是否在用
            auto[success_check_account_id, result_check_account_id] = check_active_account_id(account_id);
            if(!success_check_account_id) {
                return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check active account ID error, account_id:{}.", account_id));
            }
            if(!result_check_account_id) {
                return error_reply<Reply>(reply, ErrorCode::ACCOUNT_ID_NOT_FOUND, std::format("Account ID not found or inactive, account_id:{}.", account_id));
            }
        }

        // 不要生成新的凭证
        // if(token.empty() && !password.empty()) {
        //     auto get_token = [](const string&account_id, const string&password) -> const string { return ""; };
        //     token = get_token(account_id, password);
        // }

        // token 与密码仅允许提供一种校验方式，优先校验 token
        // 使用 token 进行验证
        if(!token.empty()) {
            auto [success_check_token, result_check_token] = check_token(account_id, token);
            if(!success_check_token) {
                return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check token error."));
            }
            if(!result_check_token) {
                // 校验失败
                return error_reply<Reply>(reply, ErrorCode::TOKEN_ERROR, std::format("Token error."));
            }
            // 校验成功
            reply->set_account_id(account_id);  // 保存传入或获取到的账号 ID
            reply->set_token(token);  // 保存传入的 token
            return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Auth by token Successed."));
        }
        // 使用密码进行验证
        else if(!password.empty()) {
            auto [success_check_password, result_check_password] = check_password(account_id, password);
            if(!success_check_password) {
                return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check Password error."));
            }
            if(!result_check_password) {
                // 校验失败
                return error_reply<Reply>(reply, ErrorCode::PASSWORD_ERROR, std::format("Password error."));
            }
            // 校验成功
            reply->set_account_id(account_id); // 保存传入或获取到的账号 ID
            // reply->set_token(token);  // 没有传入 token 则不保存
            return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Auth by passwoord Successed."));
        }
        // 默认失败
        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 用户登录  // TODO:使用 token 登陆时延长 token 有效期，而不是生成新的
    shared_ptr<SignInReply> sign_in(shared_ptr<SignInRequest> request) {
        using Request = SignInRequest;
        using Reply   = SignInReply;
        // =========================== 初始化操作 =========================== //
        // 用户登录验证，返回是否登录成功
        // 1. 根据用户 ID 查询用户记录，如果没有找到则返回失败
        // 2. 如果找到了用户记录则比较密码和安全问题答案，如果匹配则返回成功，否则返回失败
        shared_ptr<Reply> reply = make_shared<Reply>();
        set_error(reply->mutable_result(), ErrorCode::ERROR_INIT);
        if(!request) {
            return error_reply<Reply>(reply, ErrorCode::ERROR_INIT, std::format("Invalid Param."));
        }
        reply->mutable_request()->CopyFrom(*request.get());
        set_error(reply->mutable_result(), ErrorCode::UNKNOWN);
        // =========================== 参数校验 =========================== //
        // =========================== 执行操作 =========================== //
        // 验证鉴权是否成功
        shared_ptr<AuthRequest> auth_request = make_shared<AuthRequest>(request->request_user_auth());
        auto auth_reply  = auth_check(auth_request);
        auto&auth_result = auth_reply->result();
        if(!auth_result.has_success_code()) {
            return error_reply<Reply>(reply, auth_result.error_code(), auth_result.message());
        }
        // 获取结果
        auto account_id = auth_reply->account_id();
        if(account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::AUTH_FAILED, std::format("Auth failed to get account_id."));
        }
        auto token = auth_reply->has_token() ? auth_reply->token() : string();
        if(token.empty()) {
            auto[success_get_new_token, new_token] = get_new_token(account_id);
            if(!success_get_new_token) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("get new token error."));
            }
            // 不是 token 登陆的，则保持新生成的 token
            auto[success_write_token, result_write_token] = write_token(account_id, new_token);
            if(!success_write_token) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("DB write token record error."));
            }
            if(!new_token.empty()) {
                token = new_token;
            }
        }
        if(!token.empty()) {
            reply->set_account_id(account_id);
            reply->set_token(token);
            return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Signin Successed."));
        }
        else {
            return error_reply<Reply>(reply, ErrorCode::AUTH_FAILED, std::format("Auth failed to get new token."));
        }
        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 搜索用户 ID
    shared_ptr<SearchUsersReply> search_users(shared_ptr<SearchUsersRequest> request) {
        using Request  = SearchUsersReply;
        using Reply    = SearchUsersReply;
        // =========================== 初始化操作 =========================== //
        // 复制请求
        shared_ptr<Reply> reply = make_shared<Reply>();
        set_error(reply->mutable_result(), ErrorCode::ERROR_INIT);
        if(!request) {
            return error_reply<Reply>(reply, ErrorCode::ERROR_INIT, std::format("Invalid Param."));
        }
        reply->mutable_request()->CopyFrom(*request.get());
        set_error(reply->mutable_result(), ErrorCode::UNKNOWN);
        // =========================== 参数校验 =========================== //
        // 参数校验
        if(request->user_id().empty()) {
            return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_EMPTY, std::format("Users keyword is empty."));
        }
        // =========================== 执行操作 =========================== //
        // 1. 找回账号时无法提供鉴权，这里不对鉴权进行验证
        // 2. 这里是先筛选在用用户 ID 的方式过滤的，因此搜索到的是在用的账号信息

        // // 验证鉴权是否成功
        // shared_ptr<AuthRequest> auth_request = make_shared<AuthRequest>(request->request_user_auth());
        // auto auth_reply  = auth_check(auth_request);
        // auto&auth_result = auth_reply->result();
        // if(!auth_result.has_success_code()) {
        //     return error_reply<Reply>(reply, auth_result.error_code(), auth_result.message());
        // }

        // 获取参数
        string user_id      = request->user_id    ();  // 用户 ID
        string users_filter = request->user_filter();  // 暂时无效

        // 先查询目标用户的账号 ID，再通过账号 ID 获取账号信息
        // 构造数据库查询参数
        // auto sql_params = make_shared<SqlParams>();
        // (*sql_params)[":user_id"] = user_id;
        auto [success_get_active_account_id, account_id] = get_active_account_id(user_id);
        if(!success_get_active_account_id) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("db get active account id error."));
        }
        if(!account_id.empty()) {
            auto [success_get_active_account_info, account_info] = get_active_account_info(account_id);
            if(!success_get_active_account_id) {
                return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("db get active account info error."));
            }
            reply->mutable_contacts()->CopyFrom(account_info);
            return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Search user_id Successed."));
        }
        else {
            return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_NOT_FOUND, std::format("Search target user not found."));
        }
        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

private:
    // ========================= 包含 SQL 语句的函数 ========================= //
    // 执行失败、执行成功但无匹配均返回{false, {}}，执行成功且匹配才返回 {true,xx}
    // 获取数据版本
    std::tuple<bool, string> get_meta_version() {
        // =========================== 参数校验 =========================== //
        // =========================== 执行操作 =========================== //
        auto sql_result = execute(SQL_QUERY_META_VERSION);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return get_meta_version(sql_result->rows);
        }
        return {false, string{}};
    }
    std::tuple<bool, string> get_meta_version(const SqlRows&rows) {
        // | name    | value |
        // | ------- | ----- |
        // | version | 1.0   |
        //
        // 如果包含 key="name", value="version" 的行，那么从这行中取出 key = "value" 的 value

        // 解析获取 version 并返回
        for(auto&row : rows) {
            if(row.contains("name") && row.contains("value")) {
                auto name  = get_string(row.find("name" )->second);
                auto value = get_string(row.find("value")->second);
                if(name == "version") {
                    return {true, value};
                }
            }
        }
        return {false, string{}};
    }
    // 用户注册时搜索 user_id 是否可用  // 仅查找在用的用户 ID 即可
    std::tuple<bool, bool> search_active_user_id(const string&user_id) {
        // =========================== 参数校验 =========================== //
        if(user_id.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":user_id"] = user_id;
        auto sql_result = execute(SQL_QUERY_EXIST_ACTIVE_USER_ID, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return search_active_user_id(sql_result->rows);
        }
        return {false, false};
    }
    std::tuple<bool, bool> search_active_user_id(const SqlRows&rows) {
        // 解析是否为 1
        if(rows.size()) {
            return {true, true};  // 有数据即可认为存在
            // // 找到时判断值？
            // for(auto&row : rows) {
            //     if(row.contains("1")) {
            //         auto value = get_string(row.find("1")->second);
            //         if(value == "1") {
            //             return {true, true};
            //         }
            //     }
            // }
        }
        else {
            return {true, false}; // 无数据时认为不存在
        }
        return {false, false};
    }
    // 通过用户 ID 查询账号 ID  // 账号停用时返回空，表示失败
    std::tuple<bool, string> get_active_account_id(const string&user_id) {
        // =========================== 参数校验 =========================== //
        if(user_id.empty()) {
            return {false, string{}};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":user_id"] = user_id;
        auto sql_result = execute(SQL_QUERY_GET_ACTIVE_ACCOUNT_ID, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return get_active_account_id(sql_result->rows);
        }
        return {false, string{}};
    }
    std::tuple<bool, string> get_active_account_id(const SqlRows&rows) {
        // 解析获取 account_id 并返回
        for(auto&row : rows) {
            if(row.contains("account_id")) {
                return {true, get_string(row.find("account_id" )->second)};
            }
        }
        // 调用该函数就说明执行成功，返回结果为空时是有效值
        return {true, string{}};
    }
    std::tuple<bool, Contacts> get_active_account_info(const string&account_id) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty()) {
            return {false, Contacts()};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"] = account_id;
        auto sql_result = execute(SQL_QUERY_GET_ACCOUNT_INFO, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return get_active_account_info(sql_result->rows);
        }
        return {false, Contacts()};
    }
    std::tuple<bool, Contacts> get_active_account_info(const SqlRows&rows) {
        // // 执行数据库查询命令  // 传入查询语句及相关参数
        // auto sql_reply = execute(SQL_QUERY_GET_ACCOUNT_INFO_BY_USER_ID.data(), sql_params);
        // // TODO:check


        // // 数据库查询命令执行失败
        // if(!query_search_users->done) {
        //     return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Query users keyword failed."));
        // }

        // // 数据库查询命令执行成功，但没有有效数据  // 返回的结果为空
        // if(query_search_users->results.empty()) {
        //     LOG_DEBUG("DB query_search_users result empty, message: {}", query_search_users->error_message);
        //     return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_NOT_FOUND, std::format("Search target user not found."));
        // }

        // // 查询命令执行成功且有有效数据  // 返回的结果不为空，至少包含一条有效数据
        // set_success(reply->mutable_result(), SuccessCode::SUCCESS);
        // for(auto&db_user_info : query_search_users->results) {
        //     Contact * contact = reply->mutable_contacts()->add_contact();
        //     auto ite_end      = db_user_info.end();
        //     auto ite_user_id  = db_user_info.find("user_id" );
        //     auto ite_nickname = db_user_info.find("nickname");
        //     auto ite_avatar   = db_user_info.find("avatar"  );
        //     auto ite_sign     = db_user_info.find("sign"    );
        //     auto ite_gender   = db_user_info.find("gender"  );
        //     auto ite_region   = db_user_info.find("region"  );
        //     auto ite_birthday = db_user_info.find("birthday");
        //     auto ite_phones   = db_user_info.find("phones"  );
        //     auto ite_emails   = db_user_info.find("emails"  );
        //     if(ite_user_id  != ite_end) { auto point_value = get_if<string >(&ite_user_id ->second); if(point_value) { contact->set_user_id (*point_value); } }
        //     if(ite_nickname != ite_end) { auto point_value = get_if<string >(&ite_nickname->second); if(point_value) { contact->set_nickname(*point_value); } }
        //     if(ite_avatar   != ite_end) { auto point_value = get_if<string >(&ite_avatar  ->second); if(point_value) { contact->set_avatar  (*point_value); } }
        //     if(ite_sign     != ite_end) { auto point_value = get_if<string >(&ite_sign    ->second); if(point_value) { contact->set_sign    (*point_value); } }
        //     if(ite_gender   != ite_end) { auto point_value = get_if<int32_t>(&ite_gender  ->second); if(point_value) { contact->set_gender  (*point_value); } }
        //     if(ite_region   != ite_end) { auto point_value = get_if<string >(&ite_region  ->second); if(point_value) { contact->set_region  (*point_value); } }
        //     if(ite_birthday != ite_end) { auto point_value = get_if<string >(&ite_birthday->second); if(point_value) { contact->set_birthday(*point_value); } }
        //     if(ite_phones   != ite_end) { auto point_value = get_if<string >(&ite_phones  ->second); if(point_value) { for(auto&phone : split(*point_value, ";")) { contact->add_phones(phone); } } }
        //     if(ite_emails   != ite_end) { auto point_value = get_if<string >(&ite_emails  ->second); if(point_value) { for(auto&email : split(*point_value, ";")) { contact->add_emails(email); } } }
        // }
        // TODO:
        // 默认失败
        return {false, Contacts()};
    }
    std::tuple<bool, bool> check_exist_account_id(const string&account_id) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"] = account_id;
        auto sql_result = execute(SQL_QUERY_EXIST_ACTIVE_ACCOUNTID, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return check_exist_account_id(sql_result->rows);
        }
        return {false, false};
    }
    std::tuple<bool, bool> check_exist_account_id(const SqlRows&rows) {
        // 解析是否为 1
        if(rows.size()) {
            return {true, true}; // 有数据即可认为存在
        }
        else {
            return {true, false}; // 无数据时认为不存在
        }
        return {false, false};
    }
    std::tuple<bool, bool> check_active_account_id(const string&account_id) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"] = account_id;
        auto sql_result = execute(SQL_QUERY_EXIST_ACCOUNTID, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return check_active_account_id(sql_result->rows);
        }
        return {false, false};
    }
    std::tuple<bool, bool> check_active_account_id(const SqlRows&rows) {
        // 解析是否为 1
        if(rows.size()) {
            return {true, true}; // 有数据即可认为存在
        }
        else {
            return {true, false}; // 无数据时认为不存在
        }
        return {false, false};
    }
    std::tuple<bool, bool> check_password(const string&account_id, const string&password) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty() || password.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto password_hash = hash_password(password);
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"   ] = account_id   ;
        (*sql_params)[":password_hash"] = password_hash;
        auto sql_result = execute(SQL_QUERY_CHECK_PASSWORD, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return check_password(sql_result->rows);
        }
        return {false, false};
    }
    std::tuple<bool, bool> check_password(const SqlRows&rows) {
        // 解析是否为 1
        if(rows.size()) {
            return {true, true}; // 有数据即可认为存在
        }
        else {
            return {true, false}; // 无数据时认为不存在
        }
        return {false, false};
    }
    std::tuple<bool, bool> check_token(const string&account_id, const string&token) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty() || token.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        string token_hash = hash_token(token);
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"] = account_id;
        (*sql_params)[":token_hash"] = token_hash;
        auto sql_result = execute(SQL_QUERY_CHECK_TOKEN, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return check_token(sql_result->rows);
        }
        return {false, false};
    }
    std::tuple<bool, bool> check_token(const SqlRows&rows) {
        // 解析是否为 1
        if(rows.size()) {
            return {true, true}; // 有数据即可认为存在
        }
        else {
            return {true, false}; // 无数据时认为不存在
        }
        return {false, false};
    }

    std::tuple<bool, bool> write_meta_table() {
        auto sql_result = execute(SQL_CREATE_TABLE_META.data());
        if(sql_result && sql_result->done) {
            return {true, true};
        }
        return {false, false};
    }
    std::tuple<bool, bool> write_meta_version() {
        auto[success, result] = write_meta_table();
        if(!success || !result) {
            return {false, false};
        }
        auto sql_result = execute(SQL_INSERT_META_VERSION.data());
        if(sql_result && sql_result->done) {
            return {true, true};
        }
        return {false, false};
    }
    std::tuple<bool, bool> write_token(const string&account_id, const string&token) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty() || token.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        string token_hash = hash_token(token);
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id" ] = account_id;
        (*sql_params)[":token_hash" ] = token_hash;
        (*sql_params)[":create_time"] = to_utc_string(current_time_point());;
        (*sql_params)[":expire_time"] = to_utc_string(current_time_point() + std::chrono::days(30));;
        auto sql_result = execute(SQL_INSERT_USER_TOKEN, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return {true, true};
        }
        return {false, false};
    }
    std::tuple<bool, string> get_new_account_id() {
        // 循环 1000 次还重复，说明数据库可能存在异常
        for(int i = 0; i < 1000; ++i) {
            // 生成 account_id
            string account_id = generate_account_id();
            // 账号 ID 不存在时则说明可用
            auto[success, exist] = check_exist_account_id(account_id);
            // 查询失败则返回错误
            if(!success) {
                return {false, string{}};
            }
            // 查询成功且没有该 ID
            if(!exist) {
                return {true, account_id};
            }
            // 账号 ID 存在就继续循环获取新的 ID
            // else {
            //     return {true, string()};
            // }
        }
        LOG_ERROR("database account_id maybe full.");
        return {false, string{}};
    }
    std::tuple<bool, string> get_new_token(const string&account_id) {
        if(account_id.empty()) {
            return {false, string{}};
        }
        // 循环 1000 次还重复，说明数据库可能存在异常
        for(int i = 0; i < 1000; ++i) {
            // 生成 account_id
            string token = generate_token();
            // 账号 ID 不存在时则说明可用
            auto[success, exist] = check_token(account_id, token);
            // 查询失败则返回错误
            if(!success) {
                return {false, string{}};
            }
            // 查询成功且没有该 ID
            if(!exist) {
                return {true, token};
            }
            // 账号 ID 存在就继续循环获取新的 ID
        }
        LOG_ERROR("database token maybe full. account_id:{}.", account_id);
        return {false, string{}};
    }
    std::tuple<bool, string> user_register(const string&user_id, const string&password, const string&security_question, const string&security_answer) {
        auto [success_get_new_account_id, account_id] = get_new_account_id();  // 一般不会枯竭
        if(!success_get_new_account_id) {
            return {false, string{}};
        }
        // auto user_id           = user_id;
        // 获取可用的 account_id
        // auto account_id           = get_new_account_id();  // 一般不会枯竭
        // 不管用户端是否加密，服务端都对密码进行哈希加密存储
        auto password_hash        = hash_password(password);
        // auto security_question    = security_question;
        // 安全问题答案也进行哈希加密存储，避免明文存储安全问题答案的风险
        auto security_answer_hash = hash_security_answer(security_answer);
        // 获取注册时间 // // 由于需要校验，因此手动生成便于比对
        auto register_time_utc    = to_utc_string(current_time_point());
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":user_id"             ] = user_id             ;
        (*sql_params)[":account_id"          ] = account_id          ;
        (*sql_params)[":password_hash"       ] = password_hash       ;
        (*sql_params)[":security_question"   ] = security_question   ;
        (*sql_params)[":security_answer_hash"] = security_answer_hash;
        (*sql_params)[":register_time"       ] = register_time_utc   ;
        // 插入新记录
        auto sql_result = execute(SQL_INSERT_USER_REGISTER.data(), sql_params);
        if(sql_result && sql_result->done) {
            return {true, account_id};
        }
        return {false, string{}};
    }

private:
    path          m_db_path;
    sqlite3*      m_db_sqlcipher = nullptr;
    string        m_encryption_key;
    int           SQL_RESULTS_MAX_LINES = 1000;  // 每次查询最多获取多少条数据
};
