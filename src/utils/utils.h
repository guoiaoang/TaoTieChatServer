#pragma once
#include "headers.h"

namespace Utils {

using std::string;
using std::string_view;
using std::vector;
using std::tuple;
using boost::uuids::random_generator;
using boost::uuids::uuid;
using std::filesystem::path;
using time_point = std::chrono::system_clock::time_point;
// auto now = std::chrono::system_clock::now;

using std::multimap;
using std::shared_ptr;
using string_ref = ::grpc::string_ref;
using ::grpc::AuthContext;

namespace {
    static vector<string>     s_args{};
    static string             s_product_name{"TaoTieChatServer"};
    static random_generator   s_uuid_generator;
    static std::random_device s_stduuid_random_device;
};

tuple<bool, const vector<string>&> store_args(int argc, char** argv);

// ============================== 路径相关 ============================== //
path app_path();
path file_path();
path data_path();
path logs_path();
tuple<bool, const path&> mkdir(const path& path);

// ============================== UUID 及加密相关 ======================= //
uuid generate_uuid();
uuid to_uuid(const string& s);
string generate_account_id();
string generate_token();
// 可包含时间戳，可使用雪花算法等生成，保证全局唯一且有序，便于数据库索引优化和消息排序等
string generate_message_id();
// 获取文件 ID // 用于保存文件的 metadata
string generate_file_id();
// 计算字符串的 SHA-1 哈希值，返回 40 字符的十六进制字符串
string sha1(const string& input);
// 计算字符串的 MD5 哈希值，返回 32 字符的十六进制字符串
string md5(const string& input);
// 先使用 MD5 进行哈希加密，实际应用中可以考虑使用更安全的算法如 bcrypt 或 Argon2，并添加盐值以增强安全性。
string hash_password(const string& password);
string hash_security_answer(const string& answer);
string hash_token(const string& token);  // API Token 可能会重复提供，但登陆 Token 不需要再次提供

// ============================== 时间相关 ============================== //
// UTC 时间，格式为 "YYYY-MM-DD HH:MM:SS"       : std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now());
// UTC 时间，格式为 "YYYY-MM-DD HH:MM:SS"       : std::format("{:%F %T}"            , std::chrono::system_clock::now());
// UTC 时间，格式为 "YYYY-MM-DD HH:MM:SS(时区)" : std::format("{:%F %T %z}"         , std::chrono::system_clock::now());
// 本地时间，格式为 "YYYY-MM-DD HH:MM:SS(时区)" : std::format("{:%F %T %z}"         , std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()});
//
// SQLite 中的 DATETIME 类型实际上是一个文本类型，存储格式为 "YYYY-MM-DD HH:MM:SS"(UTC)。
// 因此在存储时需要将时间转换为 UTC 格式的字符串，在读取时需要将 UTC 格式的字符串转换为时间对象。
//
// 获取当前时间点
time_point current_time_point();
// 获取 UTC 时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string to_utc_string(const time_point& tp);
// 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string to_local_string(const time_point& tp);
// 获取当前 UTC 时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string utc_date_time_str();
// 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string local_date_time_str();
// 将 UTC 时间字符串转换为时间点
// static time_point to_time_point(const std::string& utc_str) {
//     return from_time_t(std::mktime(std::gmtime(&utc_str[0])));
// }
// 将 UTC 时间字符串转换为时间点
time_point to_time_point(const string& utc_str);
// 将 UTC 时间字符串转换为本地时间字符串
string utc_to_local(const string& utc_str);
// 将时间点设置到 Timestamp 消息中
void set_timestamp(TaoTieChat::Timestamp& timestamp, const std::chrono::system_clock::time_point& time_point);

// ============================== std 扩展 ============================== //
vector<string> split(const string&src, const string&div);
const string join(vector<string>&src, const string&div);
// protobuf repeated string 的 join
const string join(const google::protobuf::RepeatedPtrField<std::string>&src, const string&div);

// ====================================================================== //
// 获取错误信息
string to_message(SuccessCode code);
// 获取错误信息
string to_message(ErrorCode code);
// ====================================================================== //
void log_grpc_context(::grpc::ServerContext* context);
//
void set_success(Result *result, const SuccessCode&code = SuccessCode::SUCCESS_INIT);
void set_error  (Result *result, const ErrorCode  &code = ErrorCode::ERROR_INIT    );
string to_string(const Result&result);

// for service
template<class T>
static Status success_status(T*response, const SuccessCode&code, const string&desc) {
    set_success(response->mutable_result(), code);
    LOG_INFO("{} {}", desc, to_string(response->result()));
    return Status::OK;
};
template<class T>
static Status error_status(T*response, const ErrorCode&code, const string&desc) {
    set_error(response->mutable_result(), code);
    LOG_INFO("{} {}", desc, to_string(response->result()));
    return Status::OK;
};
// for db
template<class T>
static shared_ptr<T> success_reply(shared_ptr<T> reply, const SuccessCode&code, const string&desc) {
    set_success(reply->mutable_result(), code);
    LOG_INFO("{} {}", desc, to_string(reply->result()));
    return reply;
};
template<class T>
static shared_ptr<T> error_reply(shared_ptr<T> reply, const ErrorCode&code, const string&desc) {
    set_error(reply->mutable_result(), code);
    LOG_INFO("{} {}", desc, to_string(reply->result()));
    return reply;
};

}
