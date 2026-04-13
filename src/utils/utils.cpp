#include "utils.h"

namespace Utils {

using std::string;
using std::stringstream;
using std::string_view;
using std::vector;
using std::tuple;
using std::format;
using std::chrono::zoned_time;
using std::chrono::current_zone;
using std::chrono::parse;
using std::filesystem::path;
using std::filesystem::exists;
using std::filesystem::current_path;
using std::filesystem::absolute;
using std::filesystem::create_directories;
using std::filesystem::filesystem_error;
using boost::uuids::random_generator;
using boost::uuids::string_generator;
using boost::uuids::uuid;
using time_point = std::chrono::system_clock::time_point;
using HashSha1 = boost::uuids::detail::sha1;
using HashMd5  = boost::uuids::detail::md5;
using stm = std::tm;
auto now = std::chrono::system_clock::now;

using ::TaoTieChat::Result;
using ::TaoTieChat::SuccessCode;
using ::TaoTieChat::ErrorCode;
using ::TaoTieChat::SignUpRequest;
using ::TaoTieChat::SignUpReply;
using ::TaoTieChat::AuthRequest;
using ::TaoTieChat::AuthReply;
using ::TaoTieChat::SignInRequest;
using ::TaoTieChat::SignInReply;

tuple<bool, const vector<string>&> store_args(int argc, char** argv) {
    if(!s_args.empty()) {
        LOG_DEBUG("Command line arguments have already been stored, skipping.");
        return {false, s_args};
    }
    if(argc <= 0 || argv == nullptr) {
        LOG_ERROR("No command line arguments provided.");
        return {false, s_args};
    }
    for (int i = 0; i < argc; ++i) {
        s_args.push_back(argv[i]);
    }
    return {true, s_args};
}

// ============================== 路径相关 ============================== //

path app_path() {
    // 为了保证数据位置在程序启动后的一致性，这里仅返回初始化时设置的位置，后续不再更改。
    static path app_dir;
    if (!app_dir.empty()) {
        return app_dir;
    }

    // 设置路径
    path base_path;
    if(s_args.empty()) {
        LOG_ERROR("No command line arguments found, using current working directory as base path.");
        base_path = current_path();
    }
    else {
        base_path = absolute(s_args[0]).parent_path();
    }
    path path = base_path / s_product_name;
    app_dir = path.lexically_normal();  // 规范化路径，去除冗余部分
    return app_dir;
}

path data_path() {
    path data_dir = app_path() / "data";
    return data_dir;
}

path logs_path() {
    path log_dir = app_path() / "logs";
    return log_dir;
}

tuple<bool, const path&> mkdir(const path& path) {
    if (exists(path)) {
        LOG_WARNING("Directory already exists: {}", path.string());
        return {true, path};
    }

    // 创建
    try {
        create_directories(path);
        LOG_INFO("Directory created successfully: {}", path.string());
    } catch (const filesystem_error& e) {
        LOG_ERROR("Failed to create directory {}: {}", path.string(), e.what());
        return {false, path};
    }

    // 验证
    if(!exists(path)) {
        LOG_ERROR("Directory does not exist after creation attempt: {}", path.string());
        return {false, path};
    }
    return {true, path};
}

// ============================== UUID 及加密相关 ======================= //

uuid generate_uuid() {
    // inline uuid boost::uuids::basic_random_generator<boost::uuids::detail::chacha20_12>::operator()()
    return m_uuid_generator();
}

string generate_account_id() {
    return to_string(generate_uuid());
}

string generate_token() {
    return to_string(generate_uuid());
}

// 计算字符串的 SHA-1 哈希值，返回 40 字符的十六进制字符串
string sha1(const string& input) {
    HashSha1 hash_sha1;
    hash_sha1.process_bytes(input.data(), input.size());
    HashSha1::digest_type digest;
    hash_sha1.get_digest(digest);
    string result;
    result.reserve(40);
    for (int i = 0; i < 5; ++i) {
        result += format("{:08x}", digest[i]);
    }
    return result;
}

// 计算字符串的 MD5 哈希值，返回 32 字符的十六进制字符串
string md5(const string& input) {
    HashSha1 hash_md5;
    hash_md5.process_bytes(input.data(), input.size());
    HashSha1::digest_type digest;
    hash_md5.get_digest(digest);
    string result;
    result.reserve(32);
    for (int i = 0; i < 4; ++i) {
        result += format("{:08x}", digest[i]);
    }
    return result;
}

// 先使用 MD5 进行哈希加密，实际应用中可以考虑使用更安全的算法如 bcrypt 或 Argon2，并添加盐值以增强安全性。
string hash_password(const string& password) {
    return md5(password);
}

string hash_security_answer(const string& answer) {
    return md5(answer);
}

string hash_token(const string& token) {
    return md5(token);
}

// string to_string(const uuid& u) {
//     return boost::uuids::to_string(u);
// }

uuid to_uuid(const string& s) {
    return string_generator()(s);
}

// ============================== 时间相关 ============================== //

// UTC 时间，格式为 "YYYY-MM-DD HH:MM:SS"       : std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now());
// UTC 时间，格式为 "YYYY-MM-DD HH:MM:SS"       : std::format("{:%F %T}"            , std::chrono::system_clock::now());
// UTC 时间，格式为 "YYYY-MM-DD HH:MM:SS(时区)" : std::format("{:%F %T %z}"         , std::chrono::system_clock::now());
// 本地时间，格式为 "YYYY-MM-DD HH:MM:SS(时区)" : std::format("{:%F %T %z}"         , std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()});
//
// SQLite 中的 DATETIME 类型实际上是一个文本类型，存储格式为 "YYYY-MM-DD HH:MM:SS"(UTC)。
// 因此在存储时需要将时间转换为 UTC 格式的字符串，在读取时需要将 UTC 格式的字符串转换为时间对象。

// 获取当前时间点
time_point current_time_point() {
    return now();
}

// 获取 UTC 时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string to_utc_string(const time_point& tp) {
    // 获取 UTC 时区并创建带时区的时间对象
    return format("{:%Y-%m-%d %H:%M:%S}", tp);
}

// 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string to_local_string(const time_point& tp) {
    // 获取本地时区并创建带时区的时间对象
    auto local_time = zoned_time{current_zone(), tp};
    return format("{:%Y-%m-%d %H:%M:%S}", local_time);
}

// 获取当前 UTC 时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string utc_date_time_str() {
    return to_utc_string(current_time_point());
}

// 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string local_date_time_str() {
    return to_local_string(current_time_point());
}

// 将 UTC 时间字符串转换为时间点
// static time_point to_time_point(const string& utc_str) {
//     return std::chrono::system_clock::from_time_t(std::mktime(std::gmtime(&utc_str[0])));
// }

// 将 UTC 时间字符串转换为时间点
time_point to_time_point(const string& utc_str) {
    stm tm = {};
    stringstream ss(utc_str);

    // 使用 C++20 的 parse (部分编译器需配合 std::chrono::from_stream)
    time_point tp;
    ss >> parse("%Y-%m-%d %H:%M:%S", tp);
    // if (ss.fail()) {
    //     LOG_ERROR("Failed to parse UTC time string: {}", utc_str);
    //     return time_point{};  // 返回默认时间点
    // }
    return tp;
}

// 将 UTC 时间字符串转换为本地时间字符串
string utc_to_local(const string& utc_str) {
    auto tp = to_time_point(utc_str);
    return to_local_string(tp);
}

// ============================== std 扩展 ============================== //

vector<string> split(const string&src, const string&div) {
    vector<string> result;
    size_t start = 0;
    while (true) {
        size_t pos = src.find(div, start);
        if (pos == std::string::npos) {
            result.push_back(src.substr(start));
            break;
        }
        result.push_back(src.substr(start, pos - start));
        start = pos + div.length();
    }
    return result;
};

const string join(vector<string>&src, const string&div) {
    string result;
    for (size_t i = 0; i < src.size(); ++i) {
        result += src[i];
        if (i != src.size() - 1) {
            result += div;
        }
    }
    return result;
}

// ====================================================================== //

// 获取错误信息
string to_message(SuccessCode code) {
    static const std::map<SuccessCode, string> map_codes = {
        {SuccessCode::SUCCESS       , "Success"},
        {SuccessCode::STATUS_SUCCESS, "Success"},
        {SuccessCode::STATUS_RUNNING, "Running"},
        {SuccessCode::STATUS_ONLINE , "Online" },
        {SuccessCode::SUCCESS_INIT  , "Success on Init" },
    };

    if (map_codes.contains(code)) {
        return map_codes.at(code);
    }

    return std::format("[{}(0x{:010x})]", ::TaoTieChat::SuccessCode_Name(code), int{code});
}

// 获取错误信息
string to_message(ErrorCode code) {
    static const std::map<ErrorCode, string> map_codes = {
        {ErrorCode::UNSET                         , "Unset Error Code"       },
        {ErrorCode::UNKNOWN                       , "Unknown Error Code"     },
        {ErrorCode::ERROR_INIT                    , "Error on Init"          },
        {ErrorCode::ERROR_CONN                    , "Error on connect"       },
        {ErrorCode::ERROR_RECV                    , "Error on Recv or Parse" },
        {ErrorCode::ERROR_SEND                    , "Error on Send"          },
        // 数据库相关错误码
        {ErrorCode::DB_QUERY                       , "DB Query Failed"  },
        {ErrorCode::DB_QUERY_EMPTY                 , "DB Query Empty"   },
        {ErrorCode::DB_EXECUTE                     , "DB Execute Failed"},
        {ErrorCode::DB_EXECUTE_EMPTY               , "DB Execute Empty" },
        {ErrorCode::DB_CHECK                       , "DB Check Failed"  },
        {ErrorCode::DB_CHECK_EMPTY                 , "DB Check Empty"   },
        // error
        // {ErrorCode::STATUS_TARGET_ID_EMPTY         , "Target ID is empty"            },
        // {ErrorCode::STATUS_REQUEST_USER_ID_EMPTY   , "Request User ID is empty"      },
        // {ErrorCode::STATUS_REQUEST_USER_TOKEN_EMPTY, "Request User Token is empty"   },
        // {ErrorCode::STATUS_TARGET_ID_NOT_FOUND     , "Target ID not found"           },
        // {ErrorCode::STATUS_USER_ID_INVALID         , "Invalid Request User ID"       },
        // {ErrorCode::STATUS_USER_TOKEN_INVALID      , "Invalid Request User Token"    },
        // {ErrorCode::SIGNUP_USER_ID_EMPTY           , "User ID is empty"              },
        // {ErrorCode::SIGNUP_PASSWORD_EMPTY          , "Password is empty"             },
        // {ErrorCode::SIGNUP_CONFIRM_PASSWORD_EMPTY  , "Confirm Password is empty"     },
        // {ErrorCode::SIGNUP_SECURITY_QUESTION_EMPTY , "Security Question is empty"    },
        // {ErrorCode::SIGNUP_SECURITY_ANSWER_EMPTY   , "Security Answer is empty"      },
        // {ErrorCode::SIGNUP_DIFF_CONFIRM_PASSWORD   , "Confirm Password is not equal" },
        // {ErrorCode::SIGNUP_USER_ID_EXISTS          , "User ID already exists"        },
        // {ErrorCode::SIGNIN_USER_ID_EMPTY           , "User ID is empty"              },
        // {ErrorCode::SIGNIN_PASSWORD_EMPTY          , "Password is empty"             },
        // {ErrorCode::SIGNIN_TOKEN_EMPTY             , "Token is empty"                },
        // {ErrorCode::SIGNIN_INVALID_LOGIN_METHOD    , "Invalid login method"          },
        // {ErrorCode::SIGNIN_USER_ID_NOT_FOUND       , "User ID not found"             },
        // {ErrorCode::SIGNIN_PASSWORD_INCORRECT      , "Password is incorrect"         },
        // {ErrorCode::SIGNIN_TOKEN_INCORRECT         , "Token is incorrect"            },
        // {ErrorCode::SIGNIN_AUTH_FAILED             , "User ID or Password or Token error" },
        // {ErrorCode::SEARCH_USERS_KEYWORD_EMPTY     , "Search Users is empty"         },
        // {ErrorCode::SEARCH_USERS_KEYWORD_NOT_FOUND , "Search Users not found"        },
    };

    if (map_codes.contains(code)) {
        return map_codes.at(code);
    }
#ifdef DEBUG
    auto debug_string = std::format("[{}(0x{:010x})]", ::TaoTieChat::ErrorCode_Name(code), int{code});
#endif // DEBUG
    return std::format("[{}({:010x})]", ::TaoTieChat::ErrorCode_Name(code), int{code});
}

//
void set_success(Result *result, const SuccessCode&code) {
    if(result) {
        result->set_success_code(code);
        result->set_message(to_message(code));
    }
}

void set_error(Result *result, const ErrorCode&code) {
    if(result) {
        result->set_error_code(code);
        result->set_message(to_message(code));
    }
}

string to_string(const TaoTieChat::Result&result) {
    if(result.has_success_code()) {
        const auto&code = result.success_code();
        const auto&msg  = result.message     ();
        return std::format("success, code: {:010x}, message: {}", int{code}, msg);
    }
    if(result.has_error_code()) {
        const auto&code = result.error_code();
        const auto&msg  = result.message   ();
        return std::format("success, code: {:010x}, message: {}", int{code}, msg);
    }
    return "[UNDEFINED]";
}

//
void log_grpc_context(::grpc::ServerContext* context) {
    if(!context) {
        return;
    }

    // 打印客户端 IP 地址
    // “ipv4:127.0.0.1:11669”
    string client_peer = context->peer();
    LOG_TRACE("SignUp peer: {}", client_peer.data());
    // 打印元数据
    // "accept-encoding" = "identity,gzip"
    // "service-name" = "TaoTieChat.AccountService"
    const multimap<string_ref, string_ref>& metadata = context->client_metadata();
    for(const auto& item : metadata) {
        string key(item.first.data(), item.first.size());  // 直接打印会导致长度越界
        string value(item.second.data(), item.second.size());
        LOG_TRACE("SignUp metadata: {} = {}", key, value);
    }
    // SSL 证书验证
    shared_ptr<const AuthContext> auth_ctx = context->auth_context();
    if (auth_ctx->IsPeerAuthenticated()) {
        vector<string_ref> peers = auth_ctx->GetPeerIdentity();
        for (auto it = peers.begin(); it != peers.end(); ++it) {
        string value(it->data(), it->size());
            LOG_TRACE("SignUp peer identity: {}", value);
        }
    }
}

}
