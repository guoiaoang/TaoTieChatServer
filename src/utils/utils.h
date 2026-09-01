#pragma once
#include "headers.h"

namespace Utils {

namespace {
    // static vector<string>     s_args{};
    static string             s_product_name{"TaoTieChatServer"};
    static random_generator   s_uuid_generator;
    static std::random_device s_stduuid_random_device;
};

// 用于支持 format 的动态格式化函数
template <typename... Args>
std::string dynamic_format(std::string_view fmt, Args&&... args) {
    return std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
}

// ============================== 路径相关 ============================== //
path app_path();
path app_dir ();
path home_dir();
path data_dir();
path logs_dir();
path upload_dir    ();
path storage_dir   ();
path downloaded_dir();
path temp_dir      ();

// 文件及目录操作  // 第二个参数表示结果
// 返回值 tuple<bool, uint64_t> 中第一个 bool 表示操作是否异常，第二个 uint64_t 表示文件大小
tuple<bool, uint64_t> file_size(const path& file_path);
// 返回值 tuple<bool, bool> 中第一个 bool 表示操作是否崩溃，第二个 bool 表示目录是否存在
// {false, true } 表示非当前函数的操作，但目录已存在时  // 比如目录已经存在时进行创建
// {true , true } 表示当前函数创建目录成功，且目录存在
// {true , false} 表示当前函数执行成功，但目录不存在   // 比如无权限等导致创建失败
tuple<bool, bool    > mkdir    (const path& path    , bool create_parents = true);
// 返回值 tuple<bool, bool> 中第一个 bool 表示操作是否成功，第二个 bool 表示文件或目录是否已经移除
tuple<bool, bool    > remove   (const path& path);

bool rename(const path& old_path, const path& new_path);
bool exists(const path& file_path);

// ============================== UUID 及加密相关 ======================= //
uuid generate_uuid();
uuid to_uuid(const string& s);
string generate_account_id();
string generate_token();
// 可包含时间戳，可使用雪花算法等生成，保证全局唯一且有序，便于数据库索引优化和消息排序等
string generate_message_id();
// 获取文件 ID // 用于保存文件的 metadata
string generate_file_id();
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
string to_utc_string      (const time_point& tp, const string& fmt = "{:%Y-%m-%d %H:%M:%S}");
// 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string to_local_string    (const time_point& tp, const string& fmt = "{:%Y-%m-%d %H:%M:%S}");
// 获取当前 UTC 时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string utc_date_time_str  (                      const string& fmt = "{:%Y-%m-%d %H:%M:%S}");
// 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string local_date_time_str(                      const string& fmt = "{:%Y-%m-%d %H:%M:%S}");
// 将 UTC 时间字符串转换为时间点
// static time_point to_time_point(const std::string& utc_str) {
//     return from_time_t(std::mktime(std::gmtime(&utc_str[0])));
// }
// 将 UTC 时间字符串转换为时间点  // 解析字符串不包含 "{" / "}"
time_point to_time_point(const string& utc_str, const string& fmt = "%Y-%m-%d %H:%M:%S");
// 将 UTC 时间字符串转换为本地时间字符串
string utc_to_local(const string& utc_str, const string& fmt = "{:%Y-%m-%d %H:%M:%S}");
// 将时间点设置到 Timestamp 消息中
void set_timestamp(TaoTieChat::Timestamp& timestamp, const std::chrono::system_clock::time_point& time_point);
// 转换为 protobuf 中的时间戳
google::protobuf::Timestamp to_timestamp(const time_point& tp);

// ============================== std 扩展 ============================== //
vector<string> split(const string&src, const string&div);
const string join(vector<string>&src, const string&div);
// protobuf repeated string 的 join
const string join(const google::protobuf::RepeatedPtrField<std::string>&src, const string&div);

// ====================================================================== //
void log_grpc_context(::grpc::ServerContext* context);

} // namespace Utils

// 不涉及计算或函数的类型转换
namespace Utils::Transform {
// ====================================================================== //
// 获取错误信息
string to_message(SuccessCode code);
// 获取错误信息
string to_message(ErrorCode   code);
// ====================================================================== //
string to_string(const Result  & result);
string to_string(const SyncType& sync_type);
// ====================================================================== //
// int to enum code
ErrorCode      contact_status_to_error_code        (int result);
ErrorCode      contact_request_status_to_error_code(int result);
FileStatusCode file_status_to_file_status_code     (int result);
// enum code to int
int to_int(const ContactReplyCode& code           );
int to_int(const FileStatusCode  & file_status    );
int to_int(const PermissionCode  & permission_code);
int to_int(const SyncType        & sync_type      );
SyncType to_sync_type(const int& sync_type);
SyncType to_sync_type(const string& str_sync_type);

}  // namespace Utils::Transform

// 需要用到 Utils::Transform 中的函数
namespace Utils {
//
void set_success(Result *result, const SuccessCode&code = SuccessCode::SUCCESS_INIT);
void set_error  (Result *result, const ErrorCode  &code = ErrorCode::ERROR_INIT    );
// for service
template<class T>
static Status success_status(T*response, const SuccessCode&code, const string&description) {
    set_success(response->mutable_result(), code);
    LOG_INFO("{} {}", description, Utils::Transform::to_string(response->result()));
    return Status::OK;
};
template<class T>
static Status error_status(T*response, const ErrorCode&code, const string&description) {
    set_error(response->mutable_result(), code);
    LOG_INFO("{} {}", description, Utils::Transform::to_string(response->result()));
    return Status::OK;
};
// 服务端流式的成功和错误状态
template<class Reply, class Header>
static Status success_status(ServerWriter<Reply>*stream, Reply* reply, Header*header, const SuccessCode&code, const string&description) {
    set_success(header->mutable_result(), code);
    LOG_INFO("{} {}", description, Utils::Transform::to_string(header->result()));
    reply->mutable_header()->CopyFrom(*header);
    stream->Write(*reply);
    return Status::OK;
};
template<class Reply, class Header>
static Status error_status(ServerWriter<Reply>*stream, Reply* reply, Header*header, const ErrorCode&code, const string&description) {
    set_error(header->mutable_result(), code);
    LOG_INFO("{} {}", description, Utils::Transform::to_string(header->result()));
    reply->mutable_header()->CopyFrom(*header);
    stream->Write(*reply);
    return Status::OK;
};
// 双向流式服务的成功和错误状态
template<class Reply, class Request>
static Status success_status(ServerReaderWriter<Reply, Request>* stream, Reply*reply, const SuccessCode&code, const string&description) {
    set_success(reply->mutable_result(), code);
    LOG_INFO("{} {}", description, Utils::Transform::to_string(reply->result()));
    stream->Write(*reply);
    return Status::OK;
};
template<class Reply, class Request>
static Status error_status(ServerReaderWriter<Reply, Request>* stream, Reply*reply, const ErrorCode&code, const string&description) {
    set_error(reply->mutable_result(), code);
    LOG_INFO("{} {}", description, Utils::Transform::to_string(reply->result()));
    stream->Write(*reply);
    return Status::OK;
};
// for db
template<class T>
static shared_ptr<T> success_reply(shared_ptr<T> reply, const SuccessCode&code, const string&description) {
    set_success(reply->mutable_result(), code);
    LOG_INFO("{} {}", description, Utils::Transform::to_string(reply->result()));
    return reply;
};
template<class T>
static shared_ptr<T> error_reply(shared_ptr<T> reply, const ErrorCode&code, const string&description) {
    set_error(reply->mutable_result(), code);
    LOG_INFO("{} {}", description, Utils::Transform::to_string(reply->result()));
    return reply;
};

} // namespace Utils

// 算法相关的工具函数
namespace Utils::Algorithm {
// 计算字符串的 MD5 哈希值，返回 32 字符的十六进制字符串
string md5(const string& input);
string calculate_file_md5(const path& file_path);

// 流式即时计算 MD5 的工具类
class BytesMd5 {
public:
    void reset();
    void update(const char* data, size_t len);
    string get_hex_string();
private:
    MD5 m_md5;
};

}  // namespace Utils::Algorithm
