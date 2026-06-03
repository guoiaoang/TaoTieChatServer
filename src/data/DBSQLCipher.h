#pragma once

#include "headers.h"
#include "utils.h"
#include "DBSQLCipherCmds.h"

using namespace SQL::COMMANDS::PRAGMA;
using namespace SQL::COMMANDS::CREATE;
using namespace SQL::COMMANDS::QUERY ;
using namespace SQL::COMMANDS::INSERT;
using namespace SQL::COMMANDS::UPDATE;
using namespace Utils::Transform;

using Utils::set_success         ;
using Utils::set_error           ;
using Utils::hash_token          ;
using Utils::hash_password       ;
using Utils::hash_security_answer;
using Utils::generate_uuid       ;
using Utils::generate_account_id ;
using Utils::generate_token      ;
using Utils::generate_message_id ;
using Utils::generate_file_id    ;
using Utils::current_time_point  ;
using Utils::to_utc_string       ;
using Utils::split               ;
using Utils::success_reply       ;
using Utils::error_reply         ;
using Utils::Transform::to_string;

// =========================================================================== //

// 定义一个简单的多类型值容器（C++17）
using SqlValue  = variant<int, int64_t, double, string, nullptr_t>;  // 0,1,2,3 分别表示 int、double、string、null
using SqlParams = map<string, SqlValue>;  // 参数其实不必保留顺序
using SqlRow    = map<string, SqlValue>;  // 保留顺序
using SqlRows   = vector<SqlRow>;  // 保留顺序
using SqlCommand = pair<string_view, shared_ptr<SqlParams>>;

static std::string to_string(const SqlValue& v) {
    // 1. 转换
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        ;    if constexpr (std::is_same_v<T, int           >)  { return std::to_string(arg); }
        else if constexpr (std::is_same_v<T, int           >)  { return std::to_string(arg); }
        else if constexpr (std::is_same_v<T, int64_t       >)  { return std::to_string(arg); }
        else if constexpr (std::is_same_v<T, double        >)  { return std::to_string(arg); }
        else if constexpr (std::is_same_v<T, std::string   >)  { return arg   ; }
        else if constexpr (std::is_same_v<T, std::nullptr_t>)  { return "null"; } // 或者返回空字符串 ""
        else { return ""; }
    }, v);
    // 2. 需确保所有类型都支持 format，或者在 visit 里包装  // C++ 20
    return std::visit([](auto&& arg) {
        return std::format("{}", arg);
    }, v);
}

static string  get_string(const SqlValue& v, const string& default_value = string()) { if (auto pVal = std::get_if<string >(&v)) { return *pVal; } return default_value; }
static int     get_int   (const SqlValue& v, const int     default_value = -1      ) { if (auto pVal = std::get_if<int    >(&v)) { return *pVal; } return default_value; }
static int64_t get_int64 (const SqlValue& v, const int64_t default_value = -1      ) { if (auto pVal = std::get_if<int64_t>(&v)) { return *pVal; } return default_value; }
static double  get_double(const SqlValue& v, const double  default_value = -1.0    ) { if (auto pVal = std::get_if<double >(&v)) { return *pVal; } return default_value; }

struct SQLResult{
    bool    done = false;  // 仅表示是否执行完成，不代表是否成功
    string  msg ;  // 错误消息
    SqlRows rows;  // 列名 - 列值
    SQLResult(bool done = false, const string& msg = "", const SqlRows& rows = {})
        : done(done), msg(msg), rows(rows) {}
};

struct MessageRecord {
    string server_message_id  ;
    string client_message_id  ;
    string sender_account_id  ;
    string receiver_account_id;
    string short_text         ;
    string timestamp          ;
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
        database_close(); // 关闭数据库连接
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
            // SQL_CREATE_TABLE_META      ,
            SQL_CREATE_TABLE_USERS        ,
            SQL_CREATE_INDEX_ACCOUNT_ID   ,
            SQL_CREATE_INDEX_USER_ID      ,
            SQL_CREATE_TRIGGER_UPDATE     ,
            SQL_CREATE_TABLE_TOKENS       ,
            SQL_CREATE_INDEX_TOKEN        ,
            SQL_CREATE_INDEX_USER_SESSIONS,
            //
            SQL_CREATE_TABLE_FRIENDS          ,
            // SQL_CREATE_TABLE_GROUPS        ,
            SQL_CREATE_TABLE_MESSAGES         ,
            // SQL_CREATE_TABLE_GROUP_MESSAGES,
            SQL_CREATE_TABLE_FRIEND_REQUESTS  ,
            SQL_CREATE_TABLE_FILE_METADATAS          ,
            SQL_CREATE_INDEX_FILE_METADATAS_FILE_MD5 ,
            SQL_CREATE_TABLE_FILE_PERMISSIONS        ,
            SQL_CREATE_INDEX_FILE_PERMISSIONS_FILE_ID,
            // SQL_CREATE_TABLE_FILE_STORAGE         ,
            SQL_CREATE_TABLE_PTS_RECORDS             ,
            SQL_CREATE_TABLE_PTS_STATS
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
        } while(false);
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
    // transaction=true 时：
    // 1) 若当前不在事务中，则自动 BEGIN IMMEDIATE
    // 2) 语句成功后自动 COMMIT
    // 3) 任意失败自动 ROLLBACK
    shared_ptr<SQLResult> execute(const string_view& sql, const shared_ptr<SqlParams> params = nullptr, bool transaction = false) {
        // 辅助：根据 variant 类型执行绑定
        auto bind_variant = [](sqlite3_stmt* stmt, int idx, const SqlValue& v)->void {
            visit([&](auto&& arg) {
                using T = decay_t<decltype(arg)>;
                ;    if constexpr (is_same_v<T, int      >) sqlite3_bind_int   (stmt, idx, arg);
                else if constexpr (is_same_v<T, int64_t  >) sqlite3_bind_int64 (stmt, idx, arg);
                else if constexpr (is_same_v<T, double   >) sqlite3_bind_double(stmt, idx, arg);
                else if constexpr (is_same_v<T, string   >) sqlite3_bind_text  (stmt, idx, arg.c_str(), -1, SQLITE_TRANSIENT);
                else if constexpr (is_same_v<T, nullptr_t>) sqlite3_bind_null  (stmt, idx);
                else if constexpr (is_same_v<T, vector<uint8_t>>) sqlite3_bind_blob  (stmt, idx, arg.data(), arg.size(), SQLITE_TRANSIENT);
            }, v);
        };
        // 辅助：从列中提取数据存入 variant
        auto extract_column = [](sqlite3_stmt* stmt, int idx)->SqlValue {
            int type = sqlite3_column_type(stmt, idx);
            if (type == SQLITE_INTEGER) { return sqlite3_column_int   (stmt, idx); }
            if (type == SQLITE_FLOAT  ) { return sqlite3_column_double(stmt, idx); }
            if (type == SQLITE_TEXT   ) { return string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx))); }
            if (type == SQLITE_BLOB   ) { return string(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, idx)), sqlite3_column_bytes(stmt, idx)); }
            if (type == SQLITE_NULL   ) { return nullptr; }
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
        bool tx_started_here = false;
        if(transaction && sqlite3_get_autocommit(m_db_sqlcipher) != 0) {
            // 当前不在事务中，由 execute 自行开启事务
            // sqlite3_exec(m_db_sqlcipher, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
            auto begin_result = execute("BEGIN IMMEDIATE TRANSACTION;", nullptr, false);  // 回调，OK，没有冲突
            if(!begin_result || !begin_result->done) {
                results->done = false;
                results->msg  = begin_result ? begin_result->msg : sqlite3_errmsg(m_db_sqlcipher);
                return results;
            }
            tx_started_here = true;
        }

        auto rollback_if_needed = [this, &tx_started_here]() -> void {
            if(!tx_started_here) {
                return;
            }
            auto rollback_result = execute("ROLLBACK;", nullptr, false);  // 回调，OK，没有冲突
            if(!rollback_result || !rollback_result->done) {
                LOG_ERROR("Rollback transaction failed. msg:{}", sqlite3_errmsg(m_db_sqlcipher));
            }
            tx_started_here = false;
        };

        // **每次调用 `sqlite3_stmt` 都必须调用 `sqlite3_finalize()` **来清理，
        // 否则 `sqlite3_close(db) == SQLITE_BUSY`，无法关闭数据库
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db_sqlcipher, sql.data(), -1, &stmt, nullptr) != SQLITE_OK) {
            results->done = false;
            results->msg  = sqlite3_errmsg(m_db_sqlcipher);
            if(stmt) {
                sqlite3_finalize(stmt);
            }
            rollback_if_needed();
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
            if(tx_started_here) {
                auto commit_result = execute("COMMIT;", nullptr, false);  // 回调，OK，没有冲突
                if(!commit_result || !commit_result->done) {
                    rollback_if_needed();
                    results->done = false;
                    results->msg  = commit_result ? commit_result->msg : sqlite3_errmsg(m_db_sqlcipher);
                    sqlite3_finalize(stmt);
                    return results;
                }
                tx_started_here = false;
            }
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
            LOG_TRACE("SQL execution failed. SQL: {}, msg: {}", sql, results->msg);
            sqlite3_finalize(stmt);
            rollback_if_needed();
            return results;
        }

        results->done = false;
        sqlite3_finalize(stmt);
        return results;
    }

    // 批量执行 SQL；transaction=true 时整批语句在同一事务中原子执行
    shared_ptr<SQLResult> execute(const vector<SqlCommand>& commands, bool transaction) {
        auto results = make_shared<SQLResult>();
        if(!m_db_sqlcipher) {
            results->done = false;
            results->msg  = "Database not opened.";
            return results;
        }
        if(commands.empty()) {
            results->done = false;
            results->msg  = "sql commands is empty.";
            return results;
        }

        bool tx_started_here = false;
        if(transaction && sqlite3_get_autocommit(m_db_sqlcipher) != 0) {
            auto begin_result = execute("BEGIN IMMEDIATE TRANSACTION;", nullptr, false);
            if(!begin_result || !begin_result->done) {
                results->done = false;
                results->msg  = begin_result ? begin_result->msg : sqlite3_errmsg(m_db_sqlcipher);
                return results;
            }
            tx_started_here = true;
        }

        auto rollback_if_needed = [this, &tx_started_here]() -> void {
            if(!tx_started_here) {
                return;
            }
            auto rollback_result = execute("ROLLBACK;", nullptr, false);
            if(!rollback_result || !rollback_result->done) {
                LOG_ERROR("Rollback transaction failed. msg:{}", sqlite3_errmsg(m_db_sqlcipher));
            }
            tx_started_here = false;
        };

        for(const auto& [sql, params] : commands) {
            auto one_result = execute(sql, params, false);
            if(!one_result || !one_result->done) {
                rollback_if_needed();
                results->done = false;
                results->msg  = one_result ? one_result->msg : sqlite3_errmsg(m_db_sqlcipher);
                return results;
            }
            if(!one_result->rows.empty()) {
                results->rows.insert(results->rows.end(), one_result->rows.begin(), one_result->rows.end());
            }
        }

        if(tx_started_here) {
            auto commit_result = execute("COMMIT;", nullptr, false);
            if(!commit_result || !commit_result->done) {
                rollback_if_needed();
                results->done = false;
                results->msg  = commit_result ? commit_result->msg : sqlite3_errmsg(m_db_sqlcipher);
                return results;
            }
            tx_started_here = false;
        }

        results->done = true;
        return results;
    }

    int         get_last_error_code    () { return sqlite3_errcode         (m_db_sqlcipher); }
    int         get_extended_error_code() { return sqlite3_extended_errcode(m_db_sqlcipher); }
    const char* get_error_message      () { return sqlite3_errmsg          (m_db_sqlcipher); }

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
    // TODO: account_id 建议全局全时段唯一，即无论该账号是否注销、是否为用户、群组、服务号等的 ID 都不应重复。
    // TODO: user_id 在在用状态下，即是否为用户、群组、服务号等的 ID 时，也应该唯一。
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

    // 内部使用
    // 密码或 token 验证  // 验证身份并返回 account_id  // 传入 user_id 时进行转换
    shared_ptr<AuthReply> auth_check(shared_ptr<AuthRequest> request) {
        using Request = AuthRequest;
        using Reply   = AuthReply;
        // =========================== 初始化操作 =========================== //
        // 鉴权，仅返回认证是否成功
        // 返回账号 ID 与 token 属于登陆接口的功能
        shared_ptr<Reply> reply = make_shared<Reply>();
        set_error(reply->mutable_result(), ErrorCode::ERROR_INIT);
        if(!request) {
            return error_reply<Reply>(reply, ErrorCode::AUTH_INFO_NONE, std::format("Invalid Param."));
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

    // 搜索用户 ID，该接口可获取公开的信息及好友信息。
    // 陌生人查询或查询好友以是否有鉴权信息作为区分依据。
    // 1. 使用目标用户 ID 获取公开信息，无鉴权信息时为陌生人查询，此时可返回查询成功；
    // 2. 有鉴权信息时执行查询好友的操作，先验证鉴权再进行查询。
    shared_ptr<SearchUserReply> search_user(shared_ptr<SearchUserRequest> request) {
        using Request  = SearchUserRequest;
        using Reply    = SearchUserReply;
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
        // 目标用户 ID 为空，则查询无效
        if(request->user_id().empty()) {
            return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_EMPTY, std::format("Users keyword is empty."));
        }
        // =========================== 执行操作 =========================== //

        // ==== 1. 查询公开的信息

        // 获取参数
        string target_user_id      = request->user_id    ();  // 用户 ID
        // 先查询目标用户的账号 ID，再通过账号 ID 获取账号信息
        auto [target_success_get_active_account_id, target_account_id] = get_active_account_id(target_user_id);
        if(!target_success_get_active_account_id) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("db get active target account id error."));
        }
        // 没有目标账号则返回错误
        if(target_account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_NOT_FOUND, std::format("Search target user not found."));
        }
        // 获取到目标账号 ID 后获取公开信息  // 只要查询到账号，就至少返回陌生人信息
        auto [target_success_get_active_account_info, target_account_info] = get_active_account_info(target_account_id);
        if(!target_success_get_active_account_info) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("db get active target account info error."));
        }
        reply->mutable_contact_info()->CopyFrom(target_account_info); // 添加公开信息到回应
        // 没有鉴权信息则为查询陌生人，此时查询即可结束，可返回查询成功
        if(!request->has_request_user_auth()) {
            return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Search target user_id Successed."));
        }

        // ==== 2. 有鉴权信息时校验鉴权，进行好友查询的操作

        // 校验鉴权，失败返回
        shared_ptr<AuthRequest> auth_request = make_shared<AuthRequest>(request->request_user_auth());
        auto auth_reply  = auth_check(auth_request);
        auto&auth_result = auth_reply->result();
        if(!auth_result.has_success_code()) {
            return error_reply<Reply>(reply, auth_result.error_code(), auth_result.message());
        }
        // 鉴权通过，查询好友  // TODO:非好友关系是否要返回提示信息？
        // 获取结果
        auto self_account_id = auth_reply->account_id();
        if(self_account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::AUTH_FAILED, std::format("Auth failed to get account_id."));
        }
        auto [success_get_friend_info, friend_info] = get_friend_info(self_account_id, target_account_id);
        if(!success_get_friend_info) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("db get friend info error."));
        }
        else {
            // 复制有效的查询信息
            // 查询到的结果为空即代表非好友关系，此时不复制好友信息，以便客户端区分好友与非好友
            if(friend_info) {
                reply->mutable_contact_info()->mutable_friend_info()->CopyFrom(*friend_info);
                reply->mutable_contact_info()->mutable_friend_info()->set_user_id(target_user_id);  // 好友信息里也保存用户 ID，方便客户端使用
            }
            return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Search friends info Successed."));
        }

        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 添加为联系人的申请
    shared_ptr<ContactRequestReply> contact_request(shared_ptr<ContactRequestRequest> request) {
        using Request  = ContactRequestRequest;
        using Reply    = ContactRequestReply;
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
        if(request->target_user_id().empty()) {
            return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_EMPTY, std::format("Contact user ID is empty."));
        }
        // =========================== 执行操作 =========================== //
        // 验证鉴权是否成功
        shared_ptr<AuthRequest> auth_request = make_shared<AuthRequest>(request->request_user_auth());
        auto auth_reply  = auth_check(auth_request);
        auto&auth_result = auth_reply->result();
        if(!auth_result.has_success_code()) {
            return error_reply<Reply>(reply, auth_result.error_code(), auth_result.message());
        }
        // 获取当前用户的账号 ID
        auto request_account_id = auth_reply->account_id();
        if(request_account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::AUTH_FAILED, std::format("Auth failed to get account_id."));
        }
        // 获取要添加的联系人的账号 ID
        auto target_user_id = request->target_user_id();
        auto [success_get_active_account_id, target_account_id] = get_active_account_id(target_user_id);
        if(!success_get_active_account_id) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("db get active account id error."));
        }
        if(target_account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_NOT_FOUND, std::format("Search target user not found."));
        }

        // 好友关系以对方的列表为准
        // 1. 被对方拉黑时直接返回失败
        // 2. 对方好友包含自己，自己好友不包含对方时可直接添加
        // 3. 双方好友列表均不包含对方时进行申请
        // 单删对方可直接加回的好处是发消息不用再校验自己好友列表状态

        // 在自己的表中检查是否存在对方为好友
        auto [success_self_check_contact, result_self_check_contact] = get_contact_status(request_account_id, target_account_id);
        if(!success_self_check_contact) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check contact info error."));
        }
        auto status_self_associated = contact_status_to_error_code(result_self_check_contact);
        // 1. 对方已经是自己的好友时返回失败
        if(status_self_associated == ErrorCode::ASSOCIATED) {
            return error_reply<Reply>(reply, status_self_associated, std::format("Contact status is associated."));
        }
        // // 拉黑时也返回失败
        // if(status_self_associated == ErrorCode::BLOCKED) {
        //     return error_reply<Reply>(reply, status_self_associated, std::format("Contact status is blocked."));
        // }

        // 在对方的好友列表中查找自己是否为对方的好友
        auto [success_friend_check_contact, result_friend_check_contact] = get_contact_status(target_account_id, request_account_id);
        if(!success_friend_check_contact) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check contact info error."));
        }
        auto status_friend_associated = contact_status_to_error_code(result_friend_check_contact);
        // // 已经是好友时返回失败
        // if(associated_status == ErrorCode::ASSOCIATED) {
        //     return error_reply<Reply>(reply, associated_status, std::format("Contact status is associated."));
        // }
        // 2. 自己被对方拉黑时返回失败
        if(status_friend_associated == ErrorCode::BLOCKED) {
            return error_reply<Reply>(reply, status_friend_associated, std::format("Contact status is blocked."));
        }

        // 3. 对方不是自己的好友，但自己是对方好友时，直接添加对方到自己的好友列表
        if(status_friend_associated == ErrorCode::ASSOCIATED && status_self_associated != ErrorCode::ASSOCIATED) {
            auto[success_contact_add, result_contact_add] = contact_add(request_account_id, target_account_id);
            if(!success_contact_add) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Add contact info error."));
            }
            if(result_contact_add) {
                return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Add contact info Successed."));
            }
            else {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Add contact info error."));
            }
        }

        // 4. 对方不是自己的好友，自己也不是对方好友，且自己未被对方拉黑时，进行好友申请
        if(status_friend_associated != ErrorCode::ASSOCIATED && status_self_associated != ErrorCode::ASSOCIATED) {
            string message = request->has_message() ? request->message() : string();
            auto[success_contact_request, result_contact_request] = contact_request(request_account_id, target_account_id, message);
            if(!success_contact_request) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Add contact info error."));
            }
            if(result_contact_request) {
                // 已经记录添加好友的消息
                return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Add contact info Successed."));
            }
        }
        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 添加为联系人的申请
    shared_ptr<ContactReplyReply> contact_reply(shared_ptr<ContactReplyRequest> request) {
        using Request  = ContactReplyRequest;
        using Reply    = ContactReplyReply;
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
        if(request->target_user_id().empty()) {
            return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_EMPTY, std::format("Contact user ID is empty."));
        }
        // =========================== 执行操作 =========================== //
        // 验证鉴权是否成功
        shared_ptr<AuthRequest> auth_request = make_shared<AuthRequest>(request->request_user_auth());
        auto auth_reply  = auth_check(auth_request);
        auto&auth_result = auth_reply->result();
        if(!auth_result.has_success_code()) {
            return error_reply<Reply>(reply, auth_result.error_code(), auth_result.message());
        }
        // 获取当前用户的账号 ID
        auto request_account_id = auth_reply->account_id();
        if(request_account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::AUTH_FAILED, std::format("Auth failed to get account_id."));
        }

        // 获取要添加的联系人的账号 ID
        auto target_user_id = request->target_user_id();
        auto [success_get_active_account_id, target_account_id] = get_active_account_id(target_user_id);
        if(!success_get_active_account_id) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("db get active account id error."));
        }
        if(target_account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_NOT_FOUND, std::format("Search target user not found."));
        }

        // 1. 验证对方是否申请了加自己为好友，没有被申请时不能直接添加对方为好友
        auto [success_check_request, result_check_request] = get_friend_request_status(target_account_id, request_account_id);
        if(!success_check_request) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check friend request error."));
        }
        auto result_check_request_code = contact_request_status_to_error_code(result_check_request);
        if(result_check_request_code == ErrorCode::DB_RECORD_NOT_FOUND) {
            return error_reply<Reply>(reply, ErrorCode::FRIEND_REQUEST_NOT_FOUND, std::format("Friend request not found."));
        }
        // 拒绝或忽略好友申请时的操作
        if(request->reply() == ContactReplyCode::CONTACT_REPLY_REJECT || request->reply() == ContactReplyCode::CONTACT_REPLY_IGNORE) {
            auto [success_request_status, result_check_request] = set_contact_request_status(target_account_id, request_account_id, request->reply());
            if(!success_request_status) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Set friend request status error."));
            }
            return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Operator record."));
        }
        // 其他情况返回错误
        else if(request->reply() != ContactReplyCode::CONTACT_REPLY_ACCEPT) {
            return error_reply<Reply>(reply, ErrorCode::UNSUPPORTED, std::format("Unsupported contact reply status."));
        }

        // 好友关系以对方的列表为准
        // 1. 被对方拉黑时直接返回失败
        // 2. 对方好友包含自己，自己好友不包含对方时可直接添加
        // 3. 双方好友列表均不包含对方时进行申请
        // 单删对方可直接加回的好处是发消息不用再校验自己好友列表状态

        // 在自己的表中检查是否存在对方为好友
        auto [success_self_check_contact, result_self_check_contact] = get_contact_status(request_account_id, target_account_id);
        if(!success_self_check_contact) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check contact info error."));
        }
        auto status_self_associated = contact_status_to_error_code(result_self_check_contact);
        // 1. 对方已经是自己的好友时返回失败
        if(status_self_associated == ErrorCode::ASSOCIATED) {
            return error_reply<Reply>(reply, status_self_associated, std::format("Contact status is associated."));
        }
        // // 拉黑时也返回失败
        // if(status_self_associated == ErrorCode::BLOCKED) {
        //     return error_reply<Reply>(reply, status_self_associated, std::format("Contact status is blocked."));
        // }

        // 在对方的好友列表中查找自己是否为对方的好友
        auto [success_friend_check_contact, result_friend_check_contact] = get_contact_status(target_account_id, request_account_id);
        if(!success_friend_check_contact) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check contact info error."));
        }
        auto status_friend_associated = contact_status_to_error_code(result_friend_check_contact);
        // // 已经是好友时返回失败
        // if(associated_status == ErrorCode::ASSOCIATED) {
        //     return error_reply<Reply>(reply, associated_status, std::format("Contact status is associated."));
        // }
        // 2. 自己被对方拉黑时返回失败
        if(status_friend_associated == ErrorCode::BLOCKED) {
            return error_reply<Reply>(reply, status_friend_associated, std::format("Contact status is blocked."));
        }

        // 3. 对方不是自己的好友，但自己是对方好友时，直接添加对方到自己的好友列表
        if(status_friend_associated == ErrorCode::ASSOCIATED && status_self_associated != ErrorCode::ASSOCIATED) {
            auto[success_contact_add, result_contact_add] = contact_add(request_account_id, target_account_id);
            if(!success_contact_add) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Add contact info error."));
            }
            if(result_contact_add) {
                return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Add contact info Successed."));
            }
            else {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Add contact info error."));
            }
        }

        // 4. 对方不是自己的好友，自己也不是对方好友，且自己未被对方拉黑时，因为有合法的好友申请，也进行好友添加
        if(status_friend_associated != ErrorCode::ASSOCIATED && status_self_associated != ErrorCode::ASSOCIATED) {
            auto[success_contact_add, result_contact_add] = contact_add(request_account_id, target_account_id);
            if(!success_contact_add) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Add contact info error."));
            }
            if(result_contact_add) {
                return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Add contact info Successed."));
            }
            else {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Add contact info error."));
            }
        }
        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 客户端发送消息，服务端写入消息到数据库
    shared_ptr<MessageReply> write_message(const shared_ptr<MessageContext>&request) {
        using Request  = MessageContext;
        using Reply    = MessageReply;
        // =========================== 初始化操作 =========================== //
        // 复制请求
        shared_ptr<Reply> reply = make_shared<Reply>();
        set_error(reply->mutable_result(), ErrorCode::ERROR_INIT);
        if(!request) {
            return error_reply<Reply>(reply, ErrorCode::ERROR_INIT, std::format("Invalid Param."));
        }
        reply->mutable_request()->CopyFrom(*request.get());
        set_error(reply->mutable_result(), ErrorCode::UNKNOWN);
        // =================================================================== //
        auto has_terget_id = [](const shared_ptr<MessageContext>&request) -> bool {
            if(request->has_group_id()) { return true; }
            if(request->has_user_id ()) { return true; }
            return false;
        };
        auto has_message_context = [](const shared_ptr<MessageContext>&request) -> bool {
            if(request->has_short_text () && !request->short_text ().empty()) { return true; }
            // if(request->has_image() && !request->image().empty()) { return true; }
            // if(request->has_audio() && !request->audio().empty()) { return true; }
            // if(request->has_video() && !request->video().empty()) { return true; }
            // if(request->has_file () && !request->file ().empty()) { return true; }
            return false;
        };
        auto has_client_message_id = [](const shared_ptr<MessageContext>&request) -> bool {
            if(!request->client_message_id().empty()) { return true; }
            return false;
        };
        auto is_short_text_message = [](const shared_ptr<MessageContext>&request) -> bool {
            if(!request->has_short_text()) { return false; }
            string short_text = request->short_text();
            if(short_text.empty()) { return false; }
            // 客户端需要注意不同字符串类型占用的空间大小不一致的问题
            // [0, 8192) 为短文本，[8192, ...) 为长文本
            // 8KB 的限制是为了兼容绝大多数数据库的单行数据大小限制
            if(short_text.size() < SHORT_MESSAGE_MAX_LENGTH) { return true; }
            return false;
        };
        auto is_context_message = [](const shared_ptr<MessageContext>&request) -> bool {
            if(request->has_user_id()) { return true; }
            return false;
        };
        // auto is_long_text_message = [](const shared_ptr<MessageContext>&request) -> bool {
        //     if(request->has_long_text () && !request->long_text().empty()) { return true; }
        //     return false;
        // };
        auto get_receiver_account_id = [this](const shared_ptr<MessageContext>&request) -> string {
            if(request->has_user_id() && !request->user_id().empty()) {
                auto user_id = request->user_id();
                auto[success_get_active_account_id, result_account_id] = get_active_account_id(user_id);
                if(!success_get_active_account_id) {
                    return string{};
                }
                // 用户或账号 ID 未注册或停用等，返回账号不存在即可
                if(result_account_id.empty()) {
                    return string{};
                }
                return result_account_id;
            }
            if(request->has_group_id() && !request->group_id().empty()) {
                return request->group_id();
            }
            return string{};
        };
        // =========================== 参数校验 =========================== //
        // 参数校验
        if(!has_terget_id(request)) {
            return error_reply<Reply>(reply, ErrorCode::TARGET_ID_REQUESTED, std::format("Message receiver is empty."));
        }
        if(!has_message_context(request)){
            return error_reply<Reply>(reply, ErrorCode::CONTEXT_REQUIRED, std::format("Message content is empty."));
        }
        if(!has_client_message_id(request)) {
            return error_reply<Reply>(reply, ErrorCode::CLIENT_MESSAGE_ID_REQUIRED, std::format("Client message ID is empty."));
        }
        // =========================== 执行操作 =========================== //
        // ============== 身份验证
        // 验证鉴权是否成功
        shared_ptr<AuthRequest> auth_request = make_shared<AuthRequest>(request->request_user_auth());
        auto auth_reply  = auth_check(auth_request);
        auto&auth_result = auth_reply->result();
        if(!auth_result.has_success_code()) {
            return error_reply<Reply>(reply, auth_result.error_code(), auth_result.message());
        }
        // 获取鉴权结果
        auto account_id = auth_reply->account_id();
        if(account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::AUTH_FAILED, std::format("Auth failed to get account_id."));
        }
        // 获取目标用户账号 ID
        auto receiver_account_id = get_receiver_account_id(request);
        if(receiver_account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_NOT_FOUND, std::format("Message receiver is empty."));
        }
        // 检查是否是好友
        auto [success_check_contact, result_check_contact] = get_contact_status(account_id, receiver_account_id);
        if(!success_check_contact) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check contact info error."));
        }
        // 判断是否是好友关系
        auto associated_status = contact_status_to_error_code(result_check_contact);
        if(associated_status != ErrorCode::ASSOCIATED) {
            return error_reply<Reply>(reply, associated_status, std::format("Contact status error."));
        }
        // ============== 消息验证
        // 获取客户端消息 ID
        auto client_message_id = request->client_message_id();
        // 先验证消息是否已经写入过了，避免重复写入
        auto [success_check_exist_client_message_id, result_check_exist_client_message_id] = check_exist_client_message_id(account_id, client_message_id);
        if(!success_check_exist_client_message_id) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check message ID error."));
        }
        if(result_check_exist_client_message_id) {
            return error_reply<Reply>(reply, ErrorCode::DUPLICATE_MESSAGE_ID, std::format("Message ID already exists."));
        }
        // 生成服务端消息 ID
        auto [success_get_new_message_id, server_message_id] = get_new_message_id();
        if(!success_get_new_message_id) {
            return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Get new message ID error."));
        }
        // 数据库的默认时间格式是 UTC 时间
        auto time_point = current_time_point();
        string timestamp = to_utc_string(time_point);
        // 根据消息类型执行不同的写入操作
        if(is_short_text_message(request)) {
            auto&sender_account_id = account_id;
            auto[success_write_short_message, result_write_short_message] = write_short_message(server_message_id, client_message_id, sender_account_id, receiver_account_id, request->short_text(), timestamp);
            if(!success_write_short_message || !result_write_short_message) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Write short message error."));
            }
            // 写入成功，返回消息 ID 和时间戳等信息
            reply->set_server_message_id(server_message_id);
            set_timestamp(*reply->mutable_timestamp(), time_point);
            return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Write short message Successed."));
        }
        else {
            return error_reply<Reply>(reply, ErrorCode::UNSUPPORTED, std::format("Message type is not supported."));
        }
        // if(is_long_text_message(request)) {
        //     LOG_ERROR("TODO:write_long_text_message");
        // }
        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 更新用户信息
    // 1. 校验身份
    // 2. 有好友信息时则认为更新的是好友的信息，否则才认为更新的是自己的信息
    shared_ptr<UpdateProfileReply> update_profile(const shared_ptr<UpdateProfileRequest>&request) {
        using Request  = UpdateProfileRequest;
        using Reply    = UpdateProfileReply;
        // =========================== 初始化操作  =========================== //
        // 复制请求
        shared_ptr<Reply> reply = make_shared<Reply>();
        set_error(reply->mutable_result(), ErrorCode::ERROR_INIT);
        if(!request) {
            return error_reply<Reply>(reply, ErrorCode::ERROR_INIT, std::format("Invalid Param."));
        }
        reply->mutable_request()->CopyFrom(*request.get());
        set_error(reply->mutable_result(), ErrorCode::UNKNOWN);
        // ============================ 参数校验 ============================ //
        // ============================ 执行操作 ============================ //
        // ============== 身份验证
        // 验证鉴权是否成功
        shared_ptr<AuthRequest> auth_request = make_shared<AuthRequest>(request->request_user_auth());
        auto auth_reply  = auth_check(auth_request);
        auto&auth_result = auth_reply->result();
        if(!auth_result.has_success_code()) {
            return error_reply<Reply>(reply, auth_result.error_code(), auth_result.message());
        }
        // 获取鉴权结果
        auto account_id = auth_reply->account_id();
        if(account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::AUTH_FAILED, std::format("Auth failed to get account_id."));
        }

        // 提取用户信息
        auto contact_info  = request->contact_info();
        // 有自身信息时认为是修改自身资料的操作
        if(contact_info.has_self_info()) {
            // // 不是当前账号则无权修改
            // if(contact_info.base_info().account_id() != account_id) {
            //     return error_reply<Reply>(reply, ErrorCode::AUTH_FAILED, std::format("Base info account_id does not match authenticated account_id."));
            // }

            // 必须提供要修改为的用户 ID
            auto target_user_id  = contact_info.base_info().user_id();
            if(target_user_id.empty()) {
                return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_EMPTY, std::format("Base info user_id is empty."));
            }

            // 只有用户 ID + 密码登陆的方式支持修改用户 ID
            // auto auth_account_id = auth_request->has_account_id() ? auth_request->account_id() : string();
            auto auth_user_id    = auth_request->has_user_id   () ? auth_request->user_id   () : string();
            auto auth_password   = auth_request->has_password  () ? auth_request->password  () : string{};
            auto [success_get_active_user_id, current_user_id] = get_active_user_id(account_id);
            if(!success_get_active_user_id) {
                return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("db get active user id error."));
            }

            // 非用户 ID + 密码登陆时不允许修改用户 ID
            if(auth_user_id.empty() || auth_password.empty()) {
                if(current_user_id != target_user_id) {
                    return error_reply<Reply>(reply, ErrorCode::UNSUPPORTED, std::format("Authenticated user ID does not match user ID in auth request."));
                }
            }

            // 如果要修改用户 ID 的话，需要检查目标的用户 ID 是否可用
            if(target_user_id != current_user_id) {
                auto [success_check_user_id, result_check_user_id] = search_active_user_id(target_user_id);
                if(!success_check_user_id) {
                    return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check user ID error."));
                }
                if(result_check_user_id) {
                    return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_EXISTS, std::format("Target user ID already exists."));
                }
            }

            // 不需要 base_info 中的 account_id 字段
            // // 保存参数
            // if(contact_info.base_info().account_id().empty()) {
            //     contact_info.mutable_base_info()->set_account_id(account_id);
            // }

            // 更新个人资料
            auto [success_update_self_profile, result_update_self_profile] = update_self_profile(account_id, contact_info.base_info());
            if(!success_update_self_profile || !result_update_self_profile) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Update self profile error."));
            }
            else {
                return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Update self profile Successed."));
            }
        }
        // 有好友信息时认为是修改好友信息的操作
        if(contact_info.has_friend_info()) {
            auto friend_info = contact_info.friend_info();
            // 通常传入的是好友的用户 ID，此时需要获取好友的账号 ID
            if(friend_info.account_id().empty() && friend_info.user_id().empty()) {
                return error_reply<Reply>(reply, ErrorCode::TARGET_ID_REQUESTED, std::format("friend info account_id and user_id are both empty."));
            }
            // 获取好友的 account_id
            if(friend_info.account_id().empty() && !friend_info.user_id().empty()) {
                auto [success_get_active_account_id, result_get_active_account_id] = get_active_account_id(friend_info.user_id());
                if(!success_get_active_account_id) {
                    return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Get active account ID error."));
                }
                if(result_get_active_account_id.empty()) {
                    return error_reply<Reply>(reply, ErrorCode::TARGET_USER_ID_NOT_FOUND, std::format("Friend user ID not found."));
                }
                // 填充回去
                friend_info.set_account_id(result_get_active_account_id);
            }
            // 使用好友的 account_id 更新好友信息
            auto [success_update_friend_profile, result_update_friend_profile] = update_friend_profile(account_id, friend_info); // contact_info.friend_info() 与 friend_info 不是同一个内存
            if(!success_update_friend_profile || !result_update_friend_profile) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Update friend profile error."));
            }
            else {
                return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Update friend profile Successed."));
            }
        }

        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 获取安全问题
    shared_ptr<GetSecurityQuestionReply> get_security_question(const shared_ptr<AuthRequest>&request) {
        using Request  = AuthRequest;
        using Reply    = GetSecurityQuestionReply;
        // =========================== 初始化操作  =========================== //
        // 复制请求
        shared_ptr<Reply> reply = make_shared<Reply>();
        set_error(reply->mutable_result(), ErrorCode::ERROR_INIT);
        if(!request) {
            return error_reply<Reply>(reply, ErrorCode::ERROR_INIT, std::format("Invalid Param."));
        }
        reply->mutable_request()->CopyFrom(*request.get());
        set_error(reply->mutable_result(), ErrorCode::UNKNOWN);
        // ============================ 参数校验 ============================ //
        // 仅允许用户 ID，不允许账号 ID，不需要密码或身份校验
        if(request->user_id().empty()) {
            return error_reply<Reply>(reply, ErrorCode::USER_ID_EMPTY, std::format("User ID is empty."));
        }
        // ============================ 执行操作 ============================ //
        // 先转换为账号 ID 再操作

        // 1. 获取用户 ID 是否存在
        auto user_id = request->user_id();
        if(user_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::USER_ID_EMPTY, std::format("User ID is empty."));
        }
        auto [success_account, account_id] = get_active_account_id(user_id);
        if(!success_account) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Get active account ID error."));
        }
        if(account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::USER_ID_NOT_FOUND, std::format("User ID not found."));
        }

        // 2. 获取安全问题
        auto [success_question, question] = get_active_account_security_question(account_id);
        if(!success_question) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Get security question error."));
        }
        if(question.empty()) {
            return error_reply<Reply>(reply, ErrorCode::SECURITY_QUESTION_NOT_FOUND, std::format("Security question not found."));
        }
        else {
            reply->set_question(question);
            return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Get security question Successed."));
        }

        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 更新用户安全信息
    shared_ptr<UpdateSecurityInfoReply> update_security_info(const shared_ptr<UpdateSecurityInfoRequest>&request) {
        using Request  = UpdateSecurityInfoRequest;
        using Reply    = UpdateSecurityInfoReply;
        // =========================== 初始化操作  =========================== //
        // 复制请求
        shared_ptr<Reply> reply = make_shared<Reply>();
        set_error(reply->mutable_result(), ErrorCode::ERROR_INIT);
        if(!request) {
            return error_reply<Reply>(reply, ErrorCode::ERROR_INIT, std::format("Invalid Param."));
        }
        reply->mutable_request()->CopyFrom(*request.get());
        set_error(reply->mutable_result(), ErrorCode::UNKNOWN);
        // ============================ 参数校验 ============================ //
        // ============================ 执行操作 ============================ //

        // 之所以不直接使用子语句，是为了能够返回具体的错误码

        // 1. 验证是否是允许的操作类型
        auto old_auth_case = request->old_auth_case();
        auto new_auth_case = request->new_auth_case();
        auto auth_method_support = [&]() -> bool {
            // 三种允许的操作
            // 使用密码进行密码更新
            ;    if(old_auth_case == Request::kPasswordVerify     && new_auth_case == Request::kPasswordUpdate    ) { return true ; }
            // 使用安全问题进行密码更新
            else if(old_auth_case == Request::kSecurityInfoVerify && new_auth_case == Request::kPasswordUpdate    ) { return true ; }
            // 不允许使用密码进行安全问题更新
            else if(old_auth_case == Request::kPasswordVerify     && new_auth_case == Request::kSecurityInfoUpdate) { return false; }
            // 使用安全问题进行安全问题更新
            else if(old_auth_case == Request::kSecurityInfoVerify && new_auth_case == Request::kSecurityInfoUpdate) { return true ; }
            else { return false; }
            return false;
        };
        if(!auth_method_support()) {
            return error_reply<Reply>(reply, ErrorCode::UNSUPPORTED, std::format("Unsupported update type."));
        }

        // 2. 参数的基础校验

        // 2.1. 获取并校验用户 ID 是否为空
        auto user_id = request->user_id();
        if(user_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::USER_ID_EMPTY, std::format("User ID is empty."));
        }

        // 2.2. 参数校验

        // 下一步可能用到的变量
        string old_password;
        string old_security_question;
        string old_security_answer;
        string new_password;
        string new_confirm ;
        string new_security_question;
        string new_security_answer;

        // 2.2.1. 简单校验

        // 2.2.1.1. 身份信息的校验
        if(old_auth_case == Request::kPasswordVerify) {
            // 获取验证参数
            old_password = request->password_verify().old_password();
            if(old_password.empty()) {
                return error_reply<Reply>(reply, ErrorCode::PASSWORD_EMPTY, std::format("Old password is empty."));
            }
        }
        else if(old_auth_case == Request::kSecurityInfoVerify) {
            // 获取验证参数
            old_security_question = request->security_info_verify().old_security_question();
            old_security_answer   = request->security_info_verify().old_security_answer();
            if(old_security_question.empty()) {
                return error_reply<Reply>(reply, ErrorCode::SECURITY_QUESTION_EMPTY, std::format("Old security question is empty."));
            }
            if(old_security_answer.empty()) {
                return error_reply<Reply>(reply, ErrorCode::SECURITY_ANSWER_EMPTY, std::format("Old security answer is empty."));
            }
        }

        // 2.2.1.2. 更新信息的校验
        if(new_auth_case == Request::kPasswordUpdate) {
            new_password = request->password_update().new_password();
            if(new_password.empty()) {
                return error_reply<Reply>(reply, ErrorCode::TARGET_PASSWORD_EMPTY, std::format("New password is empty."));
            }
            new_confirm = request->password_update().new_confirm();
            if(new_confirm.empty()) {
                return error_reply<Reply>(reply, ErrorCode::TARGET_CONFIRM_PASSWORD_EMPTY, std::format("New password confirm is empty."));
            }
            if(new_password != new_confirm) {
                return error_reply<Reply>(reply, ErrorCode::DIFF_PASSWORD_WITH_CONFIRM, std::format("New password confirm does not match new password."));
            }
        }
        else if(new_auth_case == Request::kSecurityInfoUpdate) {
            new_security_question = request->security_info_update().new_security_question();
            new_security_answer   = request->security_info_update().new_security_answer();
            if(new_security_question.empty()) {
                return error_reply<Reply>(reply, ErrorCode::TARGET_SECURITY_QUESTION_EMPTY, std::format("New security question is empty."));
            }
            if(new_security_answer.empty()) {
                return error_reply<Reply>(reply, ErrorCode::TARGET_SECURITY_ANSWER_EMPTY, std::format("New security answer is empty."));
            }
        }

        // 2.2.2. 身份信息的校验

        // 先获取账号 ID，后续的操作都基于账号 ID 进行
        auto [success_account, account_id] = get_active_account_id(user_id);
        if(!success_account) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Get active account ID error."));
        }
        if(account_id.empty()) {
            return error_reply<Reply>(reply, ErrorCode::USER_ID_NOT_FOUND, std::format("Account ID not found."));
        }

        if(old_auth_case == Request::kPasswordVerify) {
            // 使用原先的流程进行密码校验，理论上只能返回 `SuccessCode::SUCCESS` 或 `ErrorCode::PASSWORD_ERROR`
            shared_ptr<AuthRequest> auth_request = make_shared<AuthRequest>();
            auth_request->set_user_id(user_id);
            auth_request->set_password(old_password);
            auto auth_reply  = auth_check(auth_request);
            auto&auth_result = auth_reply->result();
            if(!auth_result.has_success_code()) {
                return error_reply<Reply>(reply, auth_result.error_code(), auth_result.message());
            }
        }
        else if(old_auth_case == Request::kSecurityInfoVerify) {
            // 验证安全问题是否一致
            // 使用已有的接口进行安全问题的获取
            shared_ptr<AuthRequest> auth_request = make_shared<AuthRequest>();
            auth_request->set_user_id(user_id);
            auto security_reply = get_security_question(auth_request);
            auto db_question = security_reply->question();
            if(db_question.empty() || old_security_question != db_question) {
                // 密保问题不一致
                return error_reply<Reply>(reply, ErrorCode::SECURITY_QUESTION_ERROR, std::format("Security question is incorrect."));
            }

            // 验证安全答案是否正确
            auto [success_check_security_answer, result_check_security_answer] = check_active_account_security(account_id, old_security_question, old_security_answer);
            if(!success_check_security_answer) {
                return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Check security answer error."));
            }
            if(!result_check_security_answer) {
                return error_reply<Reply>(reply, ErrorCode::SECURITY_ANSWER_ERROR, std::format("Security answer is incorrect."));
            }
        }

        // 2.3. 数据更新
        // 使用密码进行密码更新
        ;    if(old_auth_case == Request::kPasswordVerify     && new_auth_case == Request::kPasswordUpdate    ) {
            // 验证成功后再进行密码更新
            auto [success_update_password, result_update_password] = update_account_password_by_password(account_id, old_password, new_password);
            if(!success_update_password || !result_update_password) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Update password by password error."));
            }
            else {
                return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Update password by password Successed."));
            }
        }
        // 使用安全问题进行密码更新
        else if(old_auth_case == Request::kSecurityInfoVerify && new_auth_case == Request::kPasswordUpdate    ) {
            // 验证成功后再进行密码更新
            auto [success_update_password, result_update_password] = update_account_password_by_security(account_id, old_security_question, old_security_answer, new_password);
            if(!success_update_password || !result_update_password) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Update password by security info error."));
            }
            else {
                return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Update password by security info Successed."));
            }
        }
        // 使用安全问题进行安全问题更新
        else if(old_auth_case == Request::kSecurityInfoVerify && new_auth_case == Request::kSecurityInfoUpdate) {
            // 验证成功后再进行安全问题和答案的更新
            auto [success_update_security_info, result_update_security_info] = update_account_security_by_security(account_id, old_security_question, old_security_answer, new_security_question, new_security_answer);
            if(!success_update_security_info || !result_update_security_info) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Update security info by security info error."));
            }
            else {
                return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("Update security info by security info Successed."));
            }
        }
        return error_reply<Reply>(reply, ErrorCode::DEFAULT, std::format("Default error."));
    }

    // 当前接口目前仅是获取一个上传 ID，不校验文件是否已经存在及是否正确
    // 秒传功能在上传接口中
    // 当前接口会查询 metadata 获取到文件的 ID，如果不存在就返回一个新的文件 ID
    //
    // 用户使用文件上传接口进行上传完成时会更新文件的 metadata 和权限信息，
    // 如果没有数据库中 metadata 信息则认为需要进行上传，
    // 有 metadata 信息则认为已经上传完成。
    shared_ptr<GetFileIDReply> get_upload_file_id(const shared_ptr<GetFileIDRequest>&request) {
        using Request = GetFileIDRequest;
        using Reply   = GetFileIDReply;
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

        // 流程：查询 metadata --> 更新 permission --> 获取文件状态

        // 获取参数
        auto file_metadata = request->file_metadata();
        auto file_md5  = file_metadata.file_md5 ();
        auto file_name = file_metadata.file_name();
        auto file_size = file_metadata.file_size();

        // 文件 hash 是否存在只影响是否上传，不影响记录元数据与权限

        // 获取文件 ID
        auto [success_get_file_id, file_id] = get_exist_file_id(file_name, file_size, file_md5);
        if(!success_get_file_id) {
            return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("GetFileID failed."));
        }
        // 文件 hash 不存在或文件 metadata 不存在时都需要记录文件 metadata，生成新的文件 ID
        if(file_id.empty()) {
            auto [success_get_new_file_id, new_file_id] = get_new_file_id();
            if(!success_get_new_file_id) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Get new file ID failed."));
            }
            file_id = new_file_id;
            // 记录文件元数据
            auto [success_write_file_metadata, result_write_file_metadata] = write_file_metadata(file_id, file_name, file_size, file_md5, FileStatusCode::FILE_STATUS_NOT_FOUND);
            if(!success_write_file_metadata || !result_write_file_metadata) {
                return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Write file metadata error."));
            }
        }
        // 记录权限关联
        auto [success_write_file_permission, result_write_file_permission] = write_file_permission(account_id, file_id);
        if(!success_write_file_permission || !result_write_file_permission) {
            return error_reply<Reply>(reply, ErrorCode::DB_EXECUTE, std::format("Write file permission error."));
        }

        // 文件上传相关状态变化计入 MESSAGES 增量，事件 ID 使用 file_id。
        write_pts_event(account_id, "MESSAGES", "FILE_UPLOAD", file_id);

        // 这里是数据库的操作，只读取文件记录，不校验真实文件是否存在，
        // 文件的上传和下载接口都不校验文件记录是否存在，只根据文件记录的状态来判断是否需要上传或下载
        auto [success_get_file_info, check_file_metadata, check_file_status] = get_file_info(file_id);
        if(!success_get_file_info) {
            return error_reply<Reply>(reply, ErrorCode::DB_QUERY, std::format("Get file info error."));
        }

        // 保存需要的数据
        reply->set_file_id(file_id);
        reply->set_file_status(check_file_status);

        // 返回是否需要上传的消息
        return success_reply<Reply>(reply, SuccessCode::SUCCESS, std::format("File hash does not exist, write metadata and permission Successed."));
    }

    // 执行上传前检查文件是否存在
    // 1. 只处理数据库相关的操作，文件读写不在这里；
    // 2. 需要检查文件元数据防止上传文件不一致，但不检查上传用户，谁都可以上传文件内容。
    //    1. 文件 ID 用来判断用户是否有权进行上传;
    //    2. 文件状态判断用户是否需要上传；
    //    3. 文件大小用来防止用户上传文件异常，便于截断，防止撑爆服务器；
    //    4. 文件 md5 用来判断上传的文件是否正确。
    std::tuple<bool, bool         > update_file_status_interface     (const string&file_id           , FileStatusCode file_status) { return update_file_status     (file_id          , file_status); }
    std::tuple<bool, int          > get_file_permission_interface    (const string&account_id        , const string  &file_id    ) { return get_file_permission    (account_id       , file_id    ); }
    std::tuple<bool, MessageRecord> get_message_record_interface     (const string& server_message_id                            ) { return get_message_record     (server_message_id             ); }
    std::tuple<bool, ContactInfo  > get_active_account_info_interface(const string&account_id                                    ) { return get_active_account_info(account_id                    ); }
    std::tuple<bool, FileMetadata , FileStatusCode     > get_file_info_interface  (const string&file_id                          ) { return get_file_info          (file_id                       ); }
    std::tuple<bool, std::shared_ptr<ContactInfoFriend>> get_friend_info_interface(const string&self_account_id, const string&friend_account_id) {
        return get_friend_info(self_account_id, friend_account_id);
    }
    // std::tuple<bool, bool      > record_file_path_interface       (const string&file_md5          , const string  &file_path  ) { return write_file_path        (file_md5         , file_path  ); }
    // std::tuple<bool, string    > get_file_path_interface          (const string&file_md5                                      ) { return get_file_path          (file_md5                      ); }

    // 获取指定账号各事件类型的最新 PTS 值（用于客户端同步）
    shared_ptr<GetLastIDReply> get_last_id(const shared_ptr<AuthRequest>& request) {
        using Reply = GetLastIDReply;
        auto reply = make_shared<Reply>();
        if(!request) { return reply; }

        // 身份验证
        auto auth_reply = auth_check(make_shared<AuthRequest>(*request));
        if(!auth_reply->result().has_success_code()) { return reply; }
        auto account_id = auth_reply->account_id();
        if(account_id.empty()) { return reply; }

        // 查询该账号所有 pts_stats 记录
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"] = account_id;
        auto sql_result = execute(SQL_QUERY_GET_PTS_STATS, sql_params);
        if(!sql_result || !sql_result->done) { return reply; }

        // 解析结果，填充 GetLastIDReply
        for(auto& row : sql_result->rows) {
            auto ite_type  = row.find("pts_type" );
            auto ite_value = row.find("pts_value");
            if(ite_type == row.end() || ite_value == row.end()) { continue; }
            auto pts_type_str = get_string(ite_type ->second);
            auto pts_value    = get_int   (ite_value->second);
            auto* tag = reply->add_tags();
            if     (pts_type_str == "CONTACTS") { tag->set_type(SyncType::CONTACTS); }
            else if(pts_type_str == "GROUPS"  ) { tag->set_type(SyncType::GROUPS  ); }
            else if(pts_type_str == "MESSAGES") { tag->set_type(SyncType::MESSAGES); }
            else { continue; }  // 跳过未知类型
            tag->set_id(pts_value);
        }
        return reply;
    }

    // 查询指定账号与类型的增量事件明细
    std::tuple<bool, SqlRows> get_pts_records(const string& account_id, const string& pts_type, int64_t id_start, int64_t id_end) {
        if(account_id.empty() || pts_type.empty()) { return {false, SqlRows{}}; }
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"] = account_id;
        (*sql_params)[":pts_type"  ] = pts_type;
        (*sql_params)[":id_start"  ] = id_start;
        (*sql_params)[":id_end"    ] = id_end;
        auto sql_result = execute(SQL_QUERY_GET_PTS_RECORDS, sql_params);
        if(!sql_result || !sql_result->done) {
            return {false, SqlRows{}};
        }
        return {true, sql_result->rows};
    }

private:
    void set_timestamp(TaoTieChat::Timestamp& timestamp, const std::chrono::system_clock::time_point& time_point) {
        // timestamp.mutable_timestamp()->set_seconds(time_point.time_since_epoch().count() / 1000); // 转换为秒
        // timestamp.mutable_timestamp()->set_nanos((time_point.time_since_epoch().count() % 1000) * 1000000); // 转换为纳秒
        // 1. 获取自 Unix 纪元以来的持续时间
        auto duration = time_point.time_since_epoch();

        // 2. 提取秒数
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        timestamp.mutable_timestamp()->set_seconds(seconds.count());

        // 3. 提取剩余的纳秒数
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds);
        timestamp.mutable_timestamp()->set_nanos(static_cast<int32_t>(nanos.count()));
    }
    // 从传入参数中提取用户 ID
    // 注意：未执行鉴权操作，且直接传入用户 ID 时未对该用户 ID 进行状态判断
    string get_active_user_id(shared_ptr<AuthRequest>&auth_request) {
        // 无信息返回空字符串
        if(!auth_request) {
            return string{};
        }
        // 直接存了用户 ID 就直接返回
        if(auth_request->has_user_id()) {
            return auth_request->user_id();
        }
        else if(auth_request->has_account_id()) {
            // 没有存用户 ID 但存了账号 ID 就查找
            auto account_id = auth_request->account_id();
            auto [success_get_user_id, result_user_id] = get_active_user_id(account_id);
            if(!success_get_user_id) {
                return string{};
            }
            return result_user_id;
        }
        // 没有存用户 ID 也没有存账号 ID 就返回空字符串
        return string{};
    }

private:
    // ========================= 包含 SQL 语句的函数 ========================= //
    // 执行失败、执行成功但无匹配均返回{false, {}}，执行成功且匹配才返回 {true,xx}

    // ============================ 解析查询结果 ============================ //

    std::tuple<bool, string> get_version_value(const SqlRows&rows) {
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
    std::tuple<bool, bool  > check_not_empty  (const SqlRows&rows) {
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
    std::tuple<bool, string> get_first_string (const SqlRows&rows, const string&key) {
        // 解析获取指定 key 的值并返回
        for(auto&row : rows) {
            if(row.contains(key)) {
                return {true, get_string(row.find(key)->second)};
            }
        }
        // 调用该函数就说明执行成功，返回结果为空时是有效值
        return {true, string{}};
    }
    std::tuple<bool, int   > get_first_int    (const SqlRows&rows, const string&key) {
        // 解析获取指定 key 的值并返回
        for(auto&row : rows) {
            if(row.contains(key)) {
                return {true, get_int(row.find(key)->second)};
            }
        }
        // 调用该函数就说明执行成功，返回结果为空时是有效值
        return {true, -1};
    }
    std::tuple<bool, ContactInfo> get_contact_info(const SqlRows&rows) {
        // 调用该函数就说明执行成功，返回结果为空时依然是有效值
        ContactInfo contact_info;
        // 解析获取 account_id 并返回
        for(auto&row : rows) {
            auto*base_info = contact_info.mutable_base_info();
            auto ite_end = row.end();
            auto ite_account_id   = row.find("account_id"  );
            auto ite_user_id      = row.find("user_id"     );
            auto ite_avatar_id    = row.find("avatar_id"   );
            auto ite_nickname     = row.find("nickname"    );
            auto ite_gender       = row.find("gender"      );
            auto ite_birthday     = row.find("birthday"    );
            auto ite_signature    = row.find("signature"   );
            auto ite_region       = row.find("region"      );
            auto ite_description  = row.find("description" );
            auto ite_phones       = row.find("phones"      );
            auto ite_emails       = row.find("emails"      );
            auto ite_social_links = row.find("social_links");
            if(ite_account_id   != ite_end) { base_info->set_account_id (get_string(ite_account_id ->second)); }
            if(ite_user_id      != ite_end) { base_info->set_user_id    (get_string(ite_user_id    ->second)); }
            if(ite_avatar_id    != ite_end) { base_info->set_avatar_id  (get_string(ite_avatar_id  ->second)); }
            if(ite_nickname     != ite_end) { base_info->set_nickname   (get_string(ite_nickname   ->second)); }
            if(ite_gender       != ite_end) { base_info->set_gender     (get_int   (ite_gender     ->second)); }
            if(ite_birthday     != ite_end) { base_info->set_birthday   (get_string(ite_birthday   ->second)); }
            if(ite_signature    != ite_end) { base_info->set_signature  (get_string(ite_signature  ->second)); }
            if(ite_region       != ite_end) { base_info->set_region     (get_string(ite_region     ->second)); }
            if(ite_description  != ite_end) { base_info->set_description(get_string(ite_description->second)); }
            if(ite_phones       != ite_end) { for(auto&phone       : split(get_string(ite_phones      ->second), ";")) { base_info->add_phones      (phone      ); } }
            if(ite_emails       != ite_end) { for(auto&email       : split(get_string(ite_emails      ->second), ";")) { base_info->add_emails      (email      ); } }
            if(ite_social_links != ite_end) { for(auto&social_link : split(get_string(ite_social_links->second), ";")) { base_info->add_social_links(social_link); } }
            return {true, contact_info};  // 一次只支持一条结果
        }
        return {true, contact_info};
    }
    // 获取好友关联信息  // 一次查询实际只有一条  // 查询结果为空时要进行标志
    std::tuple<bool, std::shared_ptr<ContactInfoFriend>> get_friend_info(const SqlRows&rows) {
        // 调用该函数就说明执行成功，返回结果为空时依然是有效值
        std::shared_ptr<ContactInfoFriend> friend_info;
        // 解析获取 account_id 并返回
        for(auto&row : rows) {
            friend_info = std::make_shared<ContactInfoFriend>();
            auto ite_end = row.end();
            auto ite_id                = row.find("id"               );
            auto ite_account_id        = row.find("account_id"       );
            auto ite_friend_account_id = row.find("friend_account_id");
            auto ite_status            = row.find("status"           );
            auto ite_starred           = row.find("starred"          );
            auto ite_silent            = row.find("silent"           );
            auto ite_notes             = row.find("notes"            );
            auto ite_added_source      = row.find("added_source"     );
            auto ite_tags              = row.find("tags"             );
            auto ite_added_time        = row.find("added_time"       );
            auto ite_removed_time      = row.find("removed_time"     );  //
            auto ite_common            = row.find("common"           );  //
            if(ite_account_id   != ite_end) { friend_info->set_account_id  (get_string(ite_account_id  ->second)); }
            if(ite_status       != ite_end) { friend_info->set_status      (get_int   (ite_status      ->second) == -1 ? 1 : get_int(ite_status ->second)); }
            if(ite_starred      != ite_end) { friend_info->set_starred     (get_int   (ite_starred     ->second) == -1 ? 0 : get_int(ite_starred->second)); }
            if(ite_silent       != ite_end) { friend_info->set_silent      (get_int   (ite_silent      ->second) == -1 ? 0 : get_int(ite_silent ->second)); }
            if(ite_notes        != ite_end) { friend_info->set_notes       (get_string(ite_notes       ->second)); }
            if(ite_added_source != ite_end) { friend_info->set_added_source(get_string(ite_added_source->second)); }
            if(ite_added_time   != ite_end) { friend_info->set_added_time  (get_string(ite_added_time  ->second)); }
            if(ite_tags         != ite_end) { for(auto&tag : split(get_string(ite_tags->second), ";")) { friend_info->add_tags(tag); } }
            return {true, friend_info};  // 一次查询实际只有一条
        }
        return {true, friend_info};
    }
    // 获取文件 ID 对应的信息
    std::tuple<bool, FileMetadata, FileStatusCode> get_file_info(const SqlRows&rows) {
        // 调用该函数就说明执行成功，返回结果为空时依然是有效值
        std::tuple<bool, FileMetadata, FileStatusCode> file_info;
        // 解析获取 file_info 并返回
        for(auto&row : rows) {
            file_info = std::make_tuple(true, FileMetadata{}, FileStatusCode::FILE_STATUS_NONE);
            auto ite_end = row.end();
            // auto ite_file_id     = row.find("file_id"    );
            auto ite_file_name      = row.find("file_name"  );
            auto ite_file_size      = row.find("file_size"  );
            auto ite_file_md5       = row.find("file_md5"   );
            auto ite_file_status    = row.find("file_status");
            // auto ite_upload_time = row.find("upload_time");
            // auto ite_common      = row.find("common"     );
            // if(ite_file_id    != ite_end) { std::get<1>(file_info).set_file_id    (get_string(ite_file_id    ->second)); }
            if(ite_file_name     != ite_end) { std::get<1>(file_info).set_file_name  (get_string(ite_file_name  ->second)); }
            if(ite_file_size     != ite_end) { std::get<1>(file_info).set_file_size  (get_int   (ite_file_size  ->second)); }
            if(ite_file_md5      != ite_end) { std::get<1>(file_info).set_file_md5   (get_string(ite_file_md5   ->second)); }
            if(ite_file_status   != ite_end) { std::get<2>(file_info) = file_status_to_file_status_code(get_int(ite_file_status->second)); }
            // if(ite_file_status!= ite_end) { std::get<1>(file_info).set_file_status(get_int   (ite_file_status->second)); }
            // if(ite_upload_time!= ite_end) { std::get<1>(file_info).set_upload_time(get_string(ite_upload_time->second)); }
            // if(ite_common     != ite_end) { std::get<1>(file_info).set_common     (get_string(ite_common     ->second)); }
            return file_info;  // 一次查询实际只有一条
        }
        return file_info;
    }
    std::tuple<bool, MessageRecord> get_message_record(const SqlRows& rows) {
        MessageRecord record{};
        for(auto& row : rows) {
            auto ite_end = row.end();
            auto ite_server_id = row.find("server_message_id");
            auto ite_client_id = row.find("client_message_id");
            auto ite_sender    = row.find("sender_account_id");
            auto ite_receiver  = row.find("receiver_account_id");
            auto ite_short     = row.find("short_text");
            auto ite_time      = row.find("timestamp");
            if(ite_server_id != ite_end) { record.server_message_id   = get_string(ite_server_id->second); }
            if(ite_client_id != ite_end) { record.client_message_id   = get_string(ite_client_id->second); }
            if(ite_sender    != ite_end) { record.sender_account_id   = get_string(ite_sender   ->second); }
            if(ite_receiver  != ite_end) { record.receiver_account_id = get_string(ite_receiver ->second); }
            if(ite_short     != ite_end) { record.short_text          = get_string(ite_short    ->second); }
            if(ite_time      != ite_end) { record.timestamp           = get_string(ite_time     ->second); }
            return {true, record};
        }
        return {false, MessageRecord{}};
    }

    // ============================== 读数据库 ============================== //

    // 获取数据版本
    std::tuple<bool, string> get_meta_version() {
        // =========================== 参数校验 =========================== //
        // =========================== 执行操作 =========================== //
        auto sql_result = execute(SQL_QUERY_META_VERSION);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return get_version_value(sql_result->rows);
        }
        return {false, string{}};
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
    // 可包含时间戳，可使用雪花算法等
    std::tuple<bool, string> get_new_message_id() {
        // 循环 1000 次还重复，说明数据库可能存在异常
        for(int i = 0; i < 1000; ++i) {
            // 生成 message_id
            string message_id = generate_message_id();
            // message_id 不存在时则说明可用
            auto[success, exist] = check_exist_server_message_id(message_id);
            // 查询失败则返回错误
            if(!success) {
                return {false, string{}};
            }
            // 查询成功且没有该 ID
            if(!exist) {
                return {true, message_id};
            }
            // 账号 ID 存在就继续循环获取新的 ID
        }
        LOG_ERROR("database message_id maybe full.");
        return {false, string{}};
    }
    std::tuple<bool, string> get_new_file_id() {
        // 循环 1000 次还重复，说明数据库可能存在异常
        for(int i = 0; i < 1000; ++i) {
            // 生成 file_id
            string file_id = generate_file_id();
            // 文件 ID 不存在时则说明可用
            auto[success, exist] = check_exist_file_id(file_id);
            // 查询失败则返回错误
            if(!success) {
                return {false, string{}};
            }
            // 查询成功且没有该 ID
            if(!exist) {
                return {true, file_id};
            }
            // 文件 ID 存在就继续循环获取新的 ID
        }
        LOG_ERROR("database file_id maybe full.");
        return {false, string{}};
    }
    // 用户注册时搜索 user_id 是否可用  // 仅查找在用的用户 ID 即可
    std::tuple<bool, bool  > search_active_user_id(const string&user_id) {
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
            return check_not_empty(sql_result->rows);
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
            return get_first_string(sql_result->rows, "account_id");
        }
        return {false, string{}};
    }
    // 通过账号 ID 查询用户 ID  // 账号停用时返回空，表示失败
    std::tuple<bool, string> get_active_user_id   (const string&account_id) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty()) {
            return {false, string{}};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"] = account_id;
        auto sql_result = execute(SQL_QUERY_GET_ACTIVE_USERID, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return get_first_string(sql_result->rows, "user_id");
        }
        return {false, string{}};
    }
    // 通过账号 ID 查询安全问题  // 安全问题不允许为空，为空则代表查询失败  // 账号停用时也返回空
    std::tuple<bool, string> get_active_account_security_question(const string&account_id) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty()) {
            return {false, string{}};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"] = account_id;
        auto sql_result = execute(SQL_QUERY_GET_ACTIVE_ACCOUNT_SECURITY_QUESTION, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return get_first_string(sql_result->rows, "security_question");
        }
        return {false, string{}};
    }
    std::tuple<bool, ContactInfo> get_active_account_info(const string&account_id) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty()) {
            return {false, ContactInfo()};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"] = account_id;
        auto sql_result = execute(SQL_QUERY_GET_ACCOUNT_INFO, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return get_contact_info(sql_result->rows);
        }
        return {false, ContactInfo()};
    }
    std::tuple<bool, std::shared_ptr<ContactInfoFriend>> get_friend_info(const string&self_account_id, const string&friend_account_id) {
        // =========================== 参数校验 =========================== //
        if(self_account_id.empty() || friend_account_id.empty()) {
            return {false, nullptr};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"       ] = self_account_id  ;
        (*sql_params)[":friend_account_id"] = friend_account_id;
        auto sql_result = execute(SQL_QUERY_GET_FRIEND_INFO, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return get_friend_info(sql_result->rows);
        }
        return {false, nullptr};
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
            return check_not_empty(sql_result->rows);
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
            return check_not_empty(sql_result->rows);
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
            return check_not_empty(sql_result->rows);
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
            return check_not_empty(sql_result->rows);
        }
        return {false, false};
    }
    // 检查服务端生成的消息 ID 是否重复，重复则代表消息已经存入
    std::tuple<bool, bool> check_exist_server_message_id(const string&message_id) {
        // =========================== 参数校验 =========================== //
        if(message_id.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":message_id"] = message_id;
        auto sql_result = execute(SQL_QUERY_EXIST_SERVER_MESSAGE_ID, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return check_not_empty(sql_result->rows);
        }
        return {false, false};
    }
    std::tuple<bool, MessageRecord> get_message_record(const string&server_message_id) {
        if(server_message_id.empty()) {
            return {false, MessageRecord{}};
        }
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":server_message_id"] = server_message_id;
        auto sql_result = execute(SQL_QUERY_GET_MESSAGE_BY_SERVER_ID, sql_params);
        if(sql_result && sql_result->done) {
            return get_message_record(sql_result->rows);
        }
        return {false, MessageRecord{}};
    }
    // 客户端生成的消息 ID // 同一账号生成的 ID 不重复即可认为是新消息，不需要全局唯一性校验
    // ID 存在代表消息重复发送
    std::tuple<bool, bool> check_exist_client_message_id(const string&sender_account_id, const string&message_id) {
        // =========================== 参数校验 =========================== //
        if(sender_account_id.empty() || message_id.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":sender_account_id"] = sender_account_id;
        (*sql_params)[":message_id"       ] = message_id       ;
        auto sql_result = execute(SQL_QUERY_EXIST_CLIENT_MESSAGE_ID, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return check_not_empty(sql_result->rows);
        }
        return {false, false};
    }
    // 检查文件 ID 是否已经存在
    std::tuple<bool, bool> check_exist_file_id(const string&file_id) {
        // =========================== 参数校验 =========================== //
        if(file_id.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":file_id"] = file_id;
        auto sql_result = execute(SQL_QUERY_EXIST_FILE_ID, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return check_not_empty(sql_result->rows);
        }
        return {false, false};
    }

    // 获取好友状态。0：非好友，1：好友，2：被对方拉黑了（可能没有这个状态，先预留）
    std::tuple<bool, int > get_contact_status(const string&request_account_id, const string&target_account_id) {
        // =========================== 参数校验 =========================== //
        if(request_account_id.empty() || target_account_id.empty()) {
            return {false, -1};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"       ] = request_account_id;
        (*sql_params)[":friend_account_id"] = target_account_id;
        auto sql_result = execute(SQL_QUERY_GET_FRIEND_STATUS, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return check_not_empty(sql_result->rows);
        }
        return {false, -1};
    }
    std::tuple<bool, int > get_friend_request_status(const string&request_account_id, const string&target_account_id) {
        // =========================== 参数校验 =========================== //
        if(request_account_id.empty() || target_account_id.empty()) {
            return {false, -1};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":request_account_id"] = request_account_id;
        (*sql_params)[":target_account_id" ] = target_account_id ;
        auto sql_result = execute(SQL_QUERY_GET_FRIEND_REQUEST_STATUS, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return get_first_int(sql_result->rows, "status");
        }
        return {false, -1};
    }
    std::tuple<bool, bool> check_active_account_security(const string&account_id, const string&question, const string&answer) {
        if(account_id.empty() || question.empty() || answer.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto answer_hash = hash_security_answer(answer);
        auto sql_params  = make_shared<SqlParams>();
        (*sql_params)[":account_id"          ] = account_id ;
        (*sql_params)[":security_question"   ] = question   ;
        (*sql_params)[":security_answer_hash"] = answer_hash;
        auto sql_result = execute(SQL_QUERY_EXIST_ACTIVE_ACCOUNT_SECURITY, sql_params);
        if(sql_result && sql_result->done) {
            return check_not_empty(sql_result->rows);
        }
        return {false, false};
    };
    // 获取已经上传文件的文件 ID
    std::tuple<bool, string> get_exist_file_id(const string& file_name, int64_t file_size, const string& file_md5) {
        // =========================== 参数校验 =========================== //
        if(file_name.empty() || file_size < 0 || file_md5.empty()) {
            return {false, string{}};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":file_name" ] = file_name ;
        (*sql_params)[":file_size" ] = file_size ;
        (*sql_params)[":file_md5"  ] = file_md5  ;
        auto sql_result = execute(SQL_QUERY_GET_FILE_ID, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return get_first_string(sql_result->rows, "file_id");
        }
        return {false, string{}};
    }
    std::tuple<bool, FileMetadata, FileStatusCode> get_file_info(const string&file_id) {
        // =========================== 参数校验 =========================== //
        if(file_id.empty()) {
            return {false, FileMetadata{}, FileStatusCode::FILE_STATUS_NONE};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":file_id"] = file_id;
        auto sql_result = execute(SQL_QUERY_GET_FILE_INFO, sql_params);
        if(sql_result && sql_result->done) {
            return get_file_info(sql_result->rows);
        }
        return {false, FileMetadata{}, FileStatusCode::FILE_STATUS_NONE};
    }
    std::tuple<bool, int> get_file_permission(const string&account_id, const string&file_id) {
        // =========================== 参数校验 =========================== //
        if(file_id.empty() || account_id.empty()) {
            return {false, 0};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":file_id"   ] = file_id   ;
        (*sql_params)[":account_id"] = account_id;
        auto sql_result = execute(SQL_QUERY_GET_FILE_PERMISSION, sql_params);
        if(sql_result && sql_result->done) {
            auto [success, permission_code] = get_first_int(sql_result->rows, "permission_code");
            if(success) {
                return {true, permission_code};
            }
        }
        return {false, 0};
    }
    // std::tuple<bool, string> get_file_path(const string&file_md5) {
    //     // =========================== 参数校验 =========================== //
    //     if(file_md5.empty()) {
    //         return {false, string{}};
    //     }
    //     // =========================== 执行操作 =========================== //
    //     // 构造参数
    //     auto sql_params = make_shared<SqlParams>();
    //     (*sql_params)[":file_md5"] = file_md5;
    //     auto sql_result = execute(SQL_QUERY_GET_FILE_PATH, sql_params);
    //     if(sql_result && sql_result->done) {
    //         // 查询命令执行成功
    //         return get_first_string(sql_result->rows, "file_path");
    //     }
    //     return {false, string{}};
    // }

    // ============================== 写数据库 ============================== //

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
    std::tuple<bool, bool> contact_request(const string&request_account_id, const string&target_account_id, const string&message = string{}) {
        // =========================== 参数校验 =========================== //
        if(request_account_id.empty() || target_account_id.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":request_account_id"] = request_account_id;
        (*sql_params)[":target_account_id" ] = target_account_id ;
        (*sql_params)[":message"           ] = message           ;
        auto sql_result = execute(SQL_INSERT_FRIEND_REQUEST, sql_params);
        if(sql_result && sql_result->done) {
            // 通知目标用户有新的好友申请（CONTACTS pts）
            // 事件 ID 使用申请方账号 ID，便于客户端拉取申请人资料。
            write_pts_event(target_account_id, "CONTACTS", "FRIEND_REQUEST", request_account_id);
            return {true, true};
        }
        return {false, false};
    }
    std::tuple<bool, bool> contact_add(const string&request_account_id, const string&target_account_id) {
        // =========================== 参数校验 =========================== //
        if(request_account_id.empty() || target_account_id.empty()) {
            return {false, false};
        }
        // // 自己也可以添加自己为好友
        // if(request_account_id == target_account_id) {
        //     return {false, false};
        // }
        // =========================== 执行操作 =========================== //
        // 事务内原子执行：
        // 1) 更新/插入 A->B 好友关系
        // 2) 更新/插入 B->A 好友关系
        // 3) 更新好友申请状态为已接受
        auto params_ab = make_shared<SqlParams>();
        (*params_ab)[":account_id"       ] = request_account_id;
        (*params_ab)[":friend_account_id"] = target_account_id;

        auto params_ba = make_shared<SqlParams>();
        (*params_ba)[":account_id"       ] = target_account_id;
        (*params_ba)[":friend_account_id"] = request_account_id;

        auto params_request_accepted = make_shared<SqlParams>();
        // 好友申请是 target -> request，当前 request 正在执行同意动作。
        (*params_request_accepted)[":request_account_id"] = target_account_id;
        (*params_request_accepted)[":target_account_id" ] = request_account_id;

        vector<SqlCommand> commands = {
            {SQL_UPDATE_FRIEND_ASSOCIATED, params_ab},
            {SQL_INSERT_FRIEND_IF_ABSENT,  params_ab},
            {SQL_UPDATE_FRIEND_ASSOCIATED, params_ba},
            {SQL_INSERT_FRIEND_IF_ABSENT,  params_ba},
            {SQL_UPDATE_FRIEND_REQUEST_ACCEPTED, params_request_accepted},
        };

        auto sql_result = execute(commands, true);
        if(!sql_result || !sql_result->done) {
            return {false, false};
        }
        // 通知双方联系人列表已更新（CONTACTS pts）
        // 各自事件 ID 使用“对端账号 ID”，便于客户端按对端刷新联系人卡片。
        write_pts_event(request_account_id, "CONTACTS", "FRIEND_ACCEPT", target_account_id);
        write_pts_event(target_account_id,  "CONTACTS", "FRIEND_ACCEPT", request_account_id);
        return {true, true};
    }
    // 记录文本消息到数据库
    std::tuple<bool, bool> write_short_message(
        const string&server_message_id, const string&client_message_id,
        const string&sender_account_id, const string&receiver_account_id,
        const string&short_text, const string&timestamp
    ) {
        // =========================== 参数校验 =========================== //
        if( server_message_id.empty() || client_message_id  .empty() ||
            sender_account_id.empty() || receiver_account_id.empty() ||
            short_text.empty() || timestamp.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":server_message_id"  ] = server_message_id;
        (*sql_params)[":client_message_id"  ] = client_message_id;
        (*sql_params)[":sender_account_id"  ] = sender_account_id;
        (*sql_params)[":receiver_account_id"] = receiver_account_id;
        (*sql_params)[":short_text"         ] = short_text;
        (*sql_params)[":timestamp"          ] = timestamp;
        auto sql_result = execute(SQL_INSERT_SHORT_MESSAGE, sql_params);
        if(sql_result && sql_result->done) {
            // 通知接收方有新消息（MESSAGES pts）
            // 事件 ID 使用 server_message_id，便于增量同步时直接回填完整消息明细。
            write_pts_event(receiver_account_id, "MESSAGES", "MESSAGE_TEXT", server_message_id);
            return {true, true};
        }
        return {false, false};
    }
    // 设置好友请求的状态为同意/拒绝/忽略
    std::tuple<bool, bool> set_contact_request_status(const string&request_account_id, const string&target_account_id, const ContactReplyCode&code) {
        // =========================== 参数校验 =========================== //
        if(request_account_id.empty() || target_account_id.empty()) {
            return {false, false};
        }
        int status = to_int(code);
        if(status == -1) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":request_account_id"] = request_account_id;
        (*sql_params)[":target_account_id" ] = target_account_id ;
        (*sql_params)[":status"            ] = status;
        auto sql_result = execute(SQL_UPDATE_FRIEND_REQUEST_STATUS, sql_params);
        if(sql_result && sql_result->done) {
            return {true, true};
        }
        return {false, false};
    }
    // 更新用户个人的非安全信息
    std::tuple<bool, bool> update_self_profile(const string&account_id, const ContactInfoBase&self_base_info) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty() || self_base_info.user_id().empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"  ] = account_id                ;  // 必须包含，用于查找
        (*sql_params)[":user_id"     ] = self_base_info.user_id    ();  // 必须包含，用于查找
        (*sql_params)[":nickname"    ] = self_base_info.nickname   ();
        (*sql_params)[":avatar_id"   ] = self_base_info.avatar_id  ();
        (*sql_params)[":signature"   ] = self_base_info.signature  ();
        (*sql_params)[":gender"      ] = self_base_info.gender     ();  // 0:未设置，1:男，2:女，3:保密
        (*sql_params)[":region"      ] = self_base_info.region     ();
        (*sql_params)[":birthday"    ] = self_base_info.birthday   ();
        (*sql_params)[":description" ] = self_base_info.description();
        (*sql_params)[":phones"      ] = Utils::join(self_base_info.phones      (), ";");
        (*sql_params)[":emails"      ] = Utils::join(self_base_info.emails      (), ";");
        (*sql_params)[":social_links"] = Utils::join(self_base_info.social_links(), ";");
        auto sql_result = execute(SQL_UPDATE_USER_PROFILE, sql_params);
        if(sql_result && sql_result->done) {
            return {true, true};
        }
        return {false, false};
    }
    // 更新好友信息
    std::tuple<bool, bool> update_friend_profile(const string&account_id, const ContactInfoFriend&friend_info) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty() || friend_info.account_id().empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 好友信息只能修改备注和分组
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"       ] = account_id;
        (*sql_params)[":friend_account_id"] = friend_info.account_id();
        (*sql_params)[":status"           ] = friend_info.status    ();
        (*sql_params)[":starred"          ] = friend_info.starred   ();
        (*sql_params)[":silent"           ] = friend_info.silent    ();
        (*sql_params)[":notes"            ] = friend_info.notes     ();
        (*sql_params)[":tags"             ] = Utils::join(friend_info.tags(), ";");
        auto sql_result = execute(SQL_UPDATE_FRIEND_PROFILE, sql_params);
        if(sql_result && sql_result->done) {
            return {true, true};
        }
        return {false, false};
    }

    // 通过验证账号的旧密码来更新新的密码
    std::tuple<bool, bool> update_account_password_by_password(const string&account_id, const string&old_password, const string&new_password) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty() || old_password.empty() || new_password.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto old_password_hash = hash_password(old_password);
        auto new_password_hash = hash_password(new_password);
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"       ] = account_id       ;
        (*sql_params)[":old_password_hash"] = old_password_hash;
        (*sql_params)[":new_password_hash"] = new_password_hash;
        auto sql_result = execute(SQL_UPDATE_USER_PASSWORD_BY_PASSWORD, sql_params);
        if(sql_result && sql_result->done) {
            return {true, true};
        }
        return {false, false};
    }
    // 通过验证账号的安全问题来更新新的密码
    std::tuple<bool, bool> update_account_password_by_security(const string&account_id, const string&question, const string&answer, const string&password) {
        // =========================== 参数校验 =========================== //
        if(account_id.empty() || question.empty() || answer.empty() || password.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto answer_hash   = hash_security_answer(answer);
        auto password_hash = hash_password(password);
        auto sql_params    = make_shared<SqlParams>();
        (*sql_params)[":account_id"          ] = account_id   ;
        (*sql_params)[":security_question"   ] = question     ;
        (*sql_params)[":security_answer_hash"] = answer_hash  ;
        (*sql_params)[":new_password_hash"   ] = password_hash;
        auto sql_result = execute(SQL_UPDATE_USER_PASSWORD_BY_SECURITY, sql_params);
        if(sql_result && sql_result->done) {
            return {true, true};
        }
        return {false, false};
    }
    // 通过验证账号的安全问题来更新新的安全问题和答案
    std::tuple<bool, bool> update_account_security_by_security(
        const string&account_id,
        const string&old_question, const string&old_answer,
        const string&new_question, const string&new_answer
    ) {
        // =========================== 参数校验 =========================== //
        if(    account_id  .empty()
            || old_question.empty() || old_answer.empty()
            || new_question.empty() || new_answer.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto old_answer_hash = hash_security_answer(old_answer);
        auto new_answer_hash = hash_security_answer(new_answer);
        auto sql_params  = make_shared<SqlParams>();
        (*sql_params)[":account_id"              ] = account_id     ;
        (*sql_params)[":old_security_question"   ] = old_question   ;
        (*sql_params)[":old_security_answer_hash"] = old_answer_hash;
        (*sql_params)[":new_security_question"   ] = new_question   ;
        (*sql_params)[":new_security_answer_hash"] = new_answer_hash;
        auto sql_result = execute(SQL_UPDATE_USER_SECURITY_BY_SECURITY, sql_params);
        if(sql_result && sql_result->done) {
            return {true, true};
        }
        return {false, false};
    }
    std::tuple<bool, bool> write_file_metadata(const string&file_id, const string&file_name, const int64_t file_size, const string&file_md5, FileStatusCode status) {
        // =========================== 参数校验 =========================== //
        if( file_id.empty() || file_name.empty() || file_size < 0 || file_md5.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":file_id"    ] = file_id  ;
        (*sql_params)[":file_name"  ] = file_name;
        (*sql_params)[":file_size"  ] = file_size;
        (*sql_params)[":file_md5"   ] = file_md5 ;
        (*sql_params)[":file_status"] = Utils::Transform::to_int(status);
        auto sql_result = execute(SQL_INSERT_FILE_METADATA, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return {true, true};
        }
        return {false, false};
    }
    std::tuple<bool, bool> write_file_permission(const std::string& account_id, const std::string& file_id) {
        // =========================== 参数校验 =========================== //
        if( account_id.empty() || file_id.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 获取权限代码
        int permission_code = Utils::Transform::to_int(PermissionCode::PERMISSION_NONE);
        permission_code += Utils::Transform::to_int(PermissionCode::PERMISSION_NONE   ); // =  0;  // 0000 0000 // 无权限
        permission_code += Utils::Transform::to_int(PermissionCode::PERMISSION_CREATE ); // =  1;  // 0000 0001 //
        permission_code += Utils::Transform::to_int(PermissionCode::PERMISSION_READ   ); // =  2;  // 0000 0010 //
        permission_code += Utils::Transform::to_int(PermissionCode::PERMISSION_WRITE  ); // =  4;  // 0000 0100 //
        permission_code += Utils::Transform::to_int(PermissionCode::PERMISSION_MODIFY ); // =  8;  // 0000 1000 //
        permission_code += Utils::Transform::to_int(PermissionCode::PERMISSION_DELETE ); // = 16;  // 0001 0000 //
        permission_code += Utils::Transform::to_int(PermissionCode::PERMISSION_SHARE  ); // = 32;  // 0010 0000 //
        permission_code += Utils::Transform::to_int(PermissionCode::PERMISSION_EXECUTE); // = 64;  // 0100 0000 //

        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"     ] = account_id;
        (*sql_params)[":file_id"        ] = file_id   ;
        (*sql_params)[":permission_code"] = permission_code; // 暂时先给全部权限
        auto sql_result = execute(SQL_UPDATE_FILE_PERMISSION, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return {true, true};
        }
        return {false, false};
    }
    // std::tuple<bool, bool> write_file_path(const std::string& file_md5, const std::string& file_path) {
    //     // =========================== 参数校验 =========================== //
    //     if(file_md5.empty() || file_path.empty()) {
    //         return {false, false};
    //     }
    //     // =========================== 执行操作 =========================== //
    //     // 构造参数
    //     auto sql_params = make_shared<SqlParams>();
    //     (*sql_params)[":file_md5" ] = file_md5 ;
    //     (*sql_params)[":file_path"] = file_path;
    //     auto sql_result = execute(SQL_INSERT_FILE_PATH, sql_params);
    //     if(sql_result && sql_result->done) {
    //         // 查询命令执行成功
    //         return {true, true};
    //     }
    //     return {false, false};
    // }
    // 写入或递增 pts_stats 中指定账号、类型的计数器
    std::tuple<bool, bool> write_pts_stats(const string& account_id, const string& pts_type) {
        if(account_id.empty() || pts_type.empty()) { return {false, false}; }
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":account_id"] = account_id;
        (*sql_params)[":pts_type"  ] = pts_type;
        auto sql_result = execute(SQL_UPSERT_PTS_STATS, sql_params);
        if(sql_result && sql_result->done) { return {true, true}; }
        return {false, false};
    }

    // 写入单条增量事件：先递增计数，再写入明细记录。
    std::tuple<bool, bool> write_pts_event(const string& account_id, const string& pts_type, const string& pts_sub_type, const string& pts_event_id) {
        if(account_id.empty() || pts_type.empty()) { return {false, false}; }

        auto [success_update_stats, result_update_stats] = write_pts_stats(account_id, pts_type);
        if(!success_update_stats || !result_update_stats) {
            return {false, false};
        }

        auto sql_get_params = make_shared<SqlParams>();
        (*sql_get_params)[":account_id"] = account_id;
        (*sql_get_params)[":pts_type"  ] = pts_type;
        auto sql_get_result = execute(SQL_QUERY_GET_PTS_VALUE, sql_get_params);
        if(!sql_get_result || !sql_get_result->done) {
            return {false, false};
        }
        auto [success_get_value, pts_value] = get_first_int(sql_get_result->rows, "pts_value");
        if(!success_get_value || pts_value <= 0) {
            return {false, false};
        }

        auto sql_insert_params = make_shared<SqlParams>();
        (*sql_insert_params)[":account_id"  ] = account_id;
        (*sql_insert_params)[":pts_type"    ] = pts_type;
        (*sql_insert_params)[":pts_sub_type"] = pts_sub_type;
        (*sql_insert_params)[":pts_value"   ] = static_cast<int64_t>(pts_value);
        (*sql_insert_params)[":pts_event_id"] = pts_event_id;
        auto sql_insert_result = execute(SQL_INSERT_PTS_RECORD, sql_insert_params);
        if(!sql_insert_result || !sql_insert_result->done) {
            return {false, false};
        }
        return {true, true};
    }

    std::tuple<bool, bool> update_file_status(const std::string& file_id, const FileStatusCode& status) {
        // =========================== 参数校验 =========================== //
        if(file_id.empty()) {
            return {false, false};
        }
        // =========================== 执行操作 =========================== //
        // 构造参数
        auto sql_params = make_shared<SqlParams>();
        (*sql_params)[":file_id"    ] = file_id;
        (*sql_params)[":file_status"] = Utils::Transform::to_int(status);
        auto sql_result = execute(SQL_UPDATE_FILE_STATUS, sql_params);
        if(sql_result && sql_result->done) {
            // 查询命令执行成功
            return {true, true};
        }
        return {false, false};
    }

private:
    path     m_db_path;
    sqlite3* m_db_sqlcipher = nullptr;
    string   m_encryption_key;
    int      SQL_RESULTS_MAX_LINES = 1000;  // 每次查询最多获取多少条数据
};
