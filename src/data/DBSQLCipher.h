#pragma once

#include "headers.h"
#include "utils.h"
#include <filesystem>
#include <format>

namespace SQL{
    namespace COMMANDS {
        // SQL 语句模板
        constexpr std::string_view SQL_SET_PRAGMA_KEY      = "PRAGMA key = '{}';";
        constexpr std::string_view SQL_CREATE_TABLE_META   = "CREATE TABLE IF NOT EXISTS meta (id INTEGER PRIMARY KEY, name TEXT, value TEXT);";
        constexpr std::string_view SQL_INSERT_META_VERSION = "INSERT INTO meta (name, value) VALUES ('version', '{}');";
        constexpr std::string_view SQL_QUERY_META_VERSION  = "SELECT name, value FROM meta where name = 'version';";
    }
}

class DBSQLCipher {
public:
    DBSQLCipher(const std::filesystem::path& db_path, const std::string& encryption_key)
        : m_db_path(db_path), m_encryption_key(encryption_key) {
    }

    bool OpenDB() {
        // 1. 检查句柄，如果不为空则表示已经正确打开并已经被初始化
        // 2. 句柄为空则使用密码打开，并读取数据版本信息
        // 3. 读取版本失败时进行创建表和写入版本信息的初始化操作
        // 4. 再次尝试读取版本号，失败则报错

        // 密码不正确也可以打开句柄，但无法读出及写入数据，因此我们通过读写数据来验证密码是否正确

        using namespace SQL::COMMANDS;

        auto absolute_db_path   = std::filesystem::absolute(m_db_path);
        auto parent_path        = absolute_db_path.parent_path();
        std::string str_db_path = absolute_db_path.string(); // 必须，否则 c_str() 返回的指针会指向一个临时对象，导致后续使用时访问无效内存
        const char* sz_db_path  = str_db_path.c_str();
        // std::cout << "Attempting to open database at path: " << absolute_db_path.string() << std::endl;
        // std::cout << "Attempting to open database at path: " << sz_db_path << std::endl;

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

        // 设置加密密钥  // 加密、修改密码都是这条语句
        std::string str_sql_set_pragma_key = std::format(SQL_SET_PRAGMA_KEY.data(), m_encryption_key);
        auto execute_set_pragma_key = Execute(str_sql_set_pragma_key);
        if(!execute_set_pragma_key->success) {
            LOG_ERROR("Failed to set encryption key: {}", execute_set_pragma_key->error_message);
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
        auto execute_query_meta_version = Execute(SQL_QUERY_META_VERSION.data());
        if (execute_query_meta_version->success) {
            auto&r = execute_query_meta_version->results;
            if(r.size() == 2 && r[0].first == "name" && r[1].first == "value" && r[0].second == "version") {
                LOG_INFO("Encryption key is correct, meta {} : {}", r[0].second, r[1].second);
            }
            else {
                for(const auto& [col_name, col_value] : r) {
                    LOG_WARNING("Unexpected column in meta version query result: {} = {}", col_name, col_value);
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

        // SQLCipher 要求必须有写入操作才会初始化数据库文件，否则为 0 字节的空文件
        // 尝试执行一个写入操作来初始化数据库文件
        // 密码错误时也可以执行到该步骤，只是会返回失败
        std::string str_sql_table_init_version = std::format("{}{}", SQL_CREATE_TABLE_META, std::format(SQL_INSERT_META_VERSION, "1.0"));
        auto execute_set_meta_version = Execute(str_sql_table_init_version);
        if (!execute_set_meta_version->success) {
            LOG_ERROR("Failed to initialize database file with write operation: {}", execute_set_meta_version->error_message);
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }

        LOG_INFO("Database file initialized successfully with write operation");

        // 再次读取验证是否正确写入了数据，如果仍然失败则说明密钥不正确
        auto query_version_verify = Execute(SQL_QUERY_META_VERSION.data());
        if (!query_version_verify->success) {
            LOG_ERROR("Failed to verify database initialization, encryption key may be incorrect");
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            return false;
        }

        LOG_INFO("Database initialization verified successfully");
        return true;
    }

    ~DBSQLCipher() {
        // 关闭数据库连接
        if (m_db_sqlcipher) {
            sqlite3_close(m_db_sqlcipher);
            m_db_sqlcipher = nullptr;
            LOG_INFO("Closed database connection");
        }
    }

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

    struct QueryResult {
        bool success;
        std::vector<std::pair<std::string, std::string>> results; // 列名 - 列值
        std::string error_message;
        QueryResult(bool success = false, const std::vector<std::pair<std::string, std::string>>& results = {}, const std::string& error_message = "")
            : success(success), results(results), error_message(error_message) {}
    };

    std::shared_ptr<QueryResult> Execute(const std::string& sql) {
        std::shared_ptr<QueryResult> results = std::make_shared<QueryResult>();

        // 回调函数将查询结果存储在 results 中
        auto callback = [](void* data, int argc, char** argv, char** azColName) -> int {
            auto* results = static_cast<std::vector<std::pair<std::string, std::string>>*>(data);
            for (int i = 0; i < argc; i++) {
                results->emplace_back(azColName[i], argv[i] ? argv[i] : "NULL");
            }
            return 0;
        };

        char* err_msg = nullptr;
        int rc = sqlite3_exec(m_db_sqlcipher, sql.c_str(), callback, &results->results, &err_msg);
        if (rc != SQLITE_OK) {
            // 查询失败，记录错误信息并返回
            results->success = false;
            results->results.clear();
            results->error_message = err_msg ? err_msg : "Unknown error";

            LOG_ERROR("SQL error: {}", err_msg);
            sqlite3_free(err_msg);
            return results;
        }

        // 查询成功
        results->success = true;
        results->error_message.clear();
        return results;
    }

    int GetLastErrorCode() {
        return sqlite3_errcode(m_db_sqlcipher);
    }

    int GetExtendedErrorCode() {
        return sqlite3_extended_errcode(m_db_sqlcipher);
    }

    const char* GetErrorMessage() {
        return sqlite3_errmsg(m_db_sqlcipher);
    }

private:
    void sqlcipher_commands() {
        static const std::map<std::string, std::string> commands = {
            {"PRAGMA cipher_version;", "获取 SQLCipher 版本信息"},
            {"PRAGMA cipher_source_id;", "获取 SQLCipher 源代码 ID"},
            {"PRAGMA cipher_libversion_number;", "获取 SQLCipher 版本号"},
            {"PRAGMA compile_options;", "获取 SQLCipher 编译选项"},
            {"PRAGMA threadsafe;", "检查 SQLCipher 是否线程安全"},
            {"PRAGMA initialize;", "初始化 SQLCipher"},
            {"PRAGMA shutdown;", "关闭 SQLCipher"},
            {"PRAGMA os_init;", "初始化 SQLCipher 的操作系统接口"},
            {"PRAGMA os_end;", "关闭 SQLCipher 的操作系统接口"},
            {"PRAGMA memory_used;", "获取 SQLCipher 当前使用的内存量"},
            {"PRAGMA keyword_count;", "获取 SQLCipher 关键字数量"}
        };
    }
private:
    std::filesystem::path m_db_path;
    sqlite3*              m_db_sqlcipher = nullptr;
    std::string           m_encryption_key;
};
