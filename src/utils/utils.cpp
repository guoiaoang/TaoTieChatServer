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
using std::filesystem::current_path;
using std::filesystem::absolute;
using std::filesystem::create_directories;
using std::filesystem::filesystem_error;
using boost::uuids::random_generator;
using boost::uuids::string_generator;
using boost::uuids::uuid;
using time_point = std::chrono::system_clock::time_point;
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

// ============================== 路径相关 ============================== //

path app_path() {
    return boost::dll::program_location().string();
}

path app_dir() {
    return app_path().parent_path();
}

path home_dir() {
    // 防止路径冲突，改为放在父目录下的 data 文件夹中
    // Linux 下生成的可执行文件没有后缀名
    return app_dir() / "TaoTie" / "Chat" / "Server";
}

path data_dir() {
    return home_dir() / "data";
}

path logs_dir() {
    return home_dir() / "logs";
}

path upload_dir () {
    return data_dir() / "upload";
}

path storage_dir() {
    return data_dir() / "storage";
}

path downloaded_dir() {
    return data_dir() / "downloaded";
}

path temp_dir() {
    return data_dir() / "temp";
}

tuple<bool, uint64_t> file_size(const path& file_path) {
    try {
        return {true, std::filesystem::file_size(file_path)};
    } catch (const filesystem_error& e) {
        LOG_ERROR("Failed to get file size for {}: {}", file_path.string(), e.what());
        return {false, 0};
    }
}

tuple<bool, bool> mkdir(const path& path, bool create_parents) {
    if (Utils::exists(path)) {
        LOG_WARNING("Directory already exists: {}", path.string());
        return {false, true};
    }

    // 创建
    try {
        if (create_parents) {
            create_directories(path);
        } else {
            create_directory(path);
        }
        LOG_INFO("Directory created successfully: {}", path.string());
    } catch (const filesystem_error& e) {
        LOG_ERROR("Failed to create directory {}: {}", path.string(), e.what());
        return {false, false};
    }

    // 验证
    if(!Utils::exists(path)) {
        LOG_ERROR("Directory does not exist after creation attempt: {}", path.string());
        return {true, false};
    }
    return {true, true};
}

tuple<bool, bool> remove(const path& file_path) {
    try {
        if (Utils::exists(file_path)) {
            std::filesystem::remove(file_path);
            LOG_INFO("Removed file: {}", file_path.string());
            if (Utils::exists(file_path)) {
                return {true, false};  // 操作成功，但移除失败
            }
            return {true, true};  // 操作成功，且移除成功
        } else {
            LOG_WARNING("File does not exist, cannot remove: {}", file_path.string());
            return {false, true};  // 操作失败，但文件或目录不存在，视为移除成功
        }
    } catch (const filesystem_error& e) {
        LOG_ERROR("Failed to remove file {}: {}", file_path.string(), e.what());
        return {false, false};  // 操作失败，移除失败
    }
}

bool rename(const path& old_path, const path& new_path) {
    if (!Utils::exists(old_path)) {
        LOG_ERROR("Source path does not exist: {}", old_path.string());
        return false;
    }
    if (Utils::exists(new_path)) {
        LOG_ERROR("Destination path already exists: {}", new_path.string());
        return false;
    }

    try {
        std::filesystem::rename(old_path, new_path);
        LOG_INFO("Renamed {} to {}", old_path.string(), new_path.string());
        return true;
    } catch (const filesystem_error& e) {
        LOG_ERROR("Failed to rename {} to {}: {}", old_path.string(), new_path.string(), e.what());
        return false;
    }
}

bool exists(const path& file_path) {
    return std::filesystem::exists(file_path);
}

// ============================== UUID 及加密相关 ======================= //

uuid generate_uuid() {
    // inline uuid boost::uuids::basic_random_generator<boost::uuids::detail::chacha20_12>::operator()()
    return s_uuid_generator();
}

string generate_account_id() {
    return to_string(generate_uuid());
}

string generate_token() {
    return to_string(generate_uuid());
}

// TODO: 可包含时间戳，可使用雪花算法生成，保证全局唯一且有序，便于数据库索引优化和消息排序等
string generate_message_id() {
    // return to_string(generate_uuid());
    // std::random_device s_stduuid_random_device;
    auto stduuid_mt19937_seed_data = std::array<int, std::mt19937::state_size> {};
    std::generate(std::begin(stduuid_mt19937_seed_data), std::end(stduuid_mt19937_seed_data), std::ref(s_stduuid_random_device));
    std::seed_seq stduuid_mt19937_seq(std::begin(stduuid_mt19937_seed_data), std::end(stduuid_mt19937_seed_data));
    std::mt19937 stduuid_mt19937_generator(stduuid_mt19937_seq);
    uuids::uuid_random_generator stduuid_gen{stduuid_mt19937_generator};

    uuids::uuid const stduuid_id = stduuid_gen();
    // assert(!id.is_nil());
    // assert(id.as_bytes().size() == 16);
    // assert(id.version() == uuids::uuid_version::random_number_based);
    // assert(id.variant() == uuids::uuid_variant::rfc);
    return uuids::to_string(stduuid_id);
}

// 获取文件 ID // 用于保存文件的 metadata
string generate_file_id() {
    return to_string(generate_uuid());
}

// 先使用 MD5 进行哈希加密，实际应用中可以考虑使用更安全的算法如 bcrypt 或 Argon2，并添加盐值以增强安全性。
string hash_password(const string& password) {
    using namespace Utils::Algorithm;
    return md5(password);
}

string hash_security_answer(const string& answer) {
    using namespace Utils::Algorithm;
    return md5(answer);
}

string hash_token(const string& token) {
    using namespace Utils::Algorithm;
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
string to_utc_string(const time_point& tp, const string& fmt) {
    // 获取 UTC 时区并创建带时区的时间对象
    return dynamic_format(fmt, tp);
}

// 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string to_local_string(const time_point& tp, const string& fmt) {
    // 获取本地时区并创建带时区的时间对象
    auto local_time = zoned_time{current_zone(), tp};
    return dynamic_format(fmt, local_time);
}

// 获取当前 UTC 时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string utc_date_time_str(const string& fmt) {
    return to_utc_string(current_time_point(), fmt);
}

// 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
string local_date_time_str(const string& fmt) {
    return to_local_string(current_time_point(), fmt);
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
string utc_to_local(const string& utc_str, const string& fmt) {
    auto tp = to_time_point(utc_str);
    return to_local_string(tp, fmt);
}

void set_timestamp(TaoTieChat::Timestamp& timestamp, const std::chrono::system_clock::time_point& time_point) {
    // 1. 获取自 Unix 纪元以来的持续时间
    auto duration = time_point.time_since_epoch();

    // 2. 提取秒数
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    timestamp.mutable_timestamp()->set_seconds(seconds.count());

    // 3. 提取剩余的纳秒数
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds);
    timestamp.mutable_timestamp()->set_nanos(static_cast<int32_t>(nanos.count()));
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

// protobuf repeated string 的 join
const string join(const google::protobuf::RepeatedPtrField<std::string>&src, const string&div) {
    string result;
    for (int i = 0; i < src.size(); ++i) {
        result += src.Get(i);
        if (i != src.size() - 1) {
            result += div;
        }
    }
    return result;
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
    if (auth_ctx && auth_ctx->IsPeerAuthenticated()) {
        vector<string_ref> peers = auth_ctx->GetPeerIdentity();
        for (auto it = peers.begin(); it != peers.end(); ++it) {
        string value(it->data(), it->size());
            LOG_TRACE("SignUp peer identity: {}", value);
        }
    }
}

}  // namespace Utils

namespace Utils::Transform {
using std::map;
using ::TaoTieChat::ErrorCode;
using ::TaoTieChat::FileStatusCode;

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
    };

    if (map_codes.contains(code)) {
        return map_codes.at(code);
    }
    return std::format("[{}(0x{:010x})]", ::TaoTieChat::ErrorCode_Name(code), int{code});
}

// ====================================================================== //

string to_string(const TaoTieChat::Result&result) {
    if(result.has_success_code()) {
        const auto&code = result.success_code();
        const auto&msg  = result.message     ();
        return std::format("success, code: 0x{:010x}, message: {}", int{code}, msg);
    }
    if(result.has_error_code()) {
        const auto&code = result.error_code();
        const auto&msg  = result.message   ();
        return std::format("error, code: 0x{:010x}, message: {}", int{code}, msg);
    }
    return "[UNDEFINED]";
}

// ====================================================================== //

// int to enum code
ErrorCode contact_status_to_error_code        (int result) {
    static map<int, ErrorCode> status_error_code_map = {
        {-1, ErrorCode::DB_RECORD_NOT_FOUND },  // 好友请求未找到
        {0, ErrorCode::PENDING   },  // 存在好友申请
        {1, ErrorCode::ASSOCIATED},  // 已经是好友了
        {2, ErrorCode::REFUSED   },  // 好友请求被拒绝
        {3, ErrorCode::BLOCKED   },  // 被对方拉黑了
    };
    if(status_error_code_map.find(result) != status_error_code_map.end()) {
        return status_error_code_map[result];
    }
    return ErrorCode::DEFAULT;
}
ErrorCode contact_request_status_to_error_code(int result) {
    static map<int, ErrorCode> status_error_code_map = {
        {-1, ErrorCode::DB_RECORD_NOT_FOUND },  // 好友请求未找到
        {0, ErrorCode::PENDING },  // 好友请求待处理
        {1, ErrorCode::ACCEPTED},  // 同意好友申请
        {2, ErrorCode::REFUSED },  // 拒绝好友申请
        {3, ErrorCode::IGNORED },  // 忽略好友申请（不回复，既不接受也不拒绝）
    };
    if(status_error_code_map.find(result) != status_error_code_map.end()) {
        return status_error_code_map[result];
    }
    return ErrorCode::DEFAULT;
}
FileStatusCode file_status_to_file_status_code(int result) {
    static map<int, FileStatusCode> status_code_map = {
        {0, FileStatusCode::FILE_STATUS_NONE      },  // 文件状态未知，或查询失败
        {1, FileStatusCode::FILE_STATUS_UPLOADED  },  // 文件已存在，无需上传
        {2, FileStatusCode::FILE_STATUS_PENDING   },  // 文件不存在或未完成上传，需要上传
        {3, FileStatusCode::FILE_STATUS_NOT_FOUND },  // 文件不存在或已删除
    };
    if(status_code_map.find(result) != status_code_map.end()) {
        return status_code_map[result];
    }
    return FileStatusCode::FILE_STATUS_NONE;
}
int to_int(const ContactReplyCode& code       ) {
    ;    if(code == ContactReplyCode::CONTACT_REPLY_ACCEPT) { return 1; }
    else if(code == ContactReplyCode::CONTACT_REPLY_REJECT) { return 2; }
    else if(code == ContactReplyCode::CONTACT_REPLY_IGNORE) { return 3; }
    return -1;
}
int to_int(const FileStatusCode  & file_status) {
    static map<FileStatusCode, int> file_status_code_map = {
        {FileStatusCode::FILE_STATUS_NONE     , 0},
        {FileStatusCode::FILE_STATUS_UPLOADED , 1},
        {FileStatusCode::FILE_STATUS_PENDING  , 2},
        {FileStatusCode::FILE_STATUS_NOT_FOUND, 3},
    };
    if(file_status_code_map.find(file_status) != file_status_code_map.end()) {
        return file_status_code_map[file_status];
    }
    return 0;
}

int to_int(const PermissionCode& permission_code) {
    static map<PermissionCode, int> permission_code_map = {
        {PermissionCode::PERMISSION_NONE   , 0},  // 0000 0000 // 无权限
        {PermissionCode::PERMISSION_CREATE , 1},  // 0000 0001 //
        {PermissionCode::PERMISSION_READ   , 2},  // 0000 0010 //
        {PermissionCode::PERMISSION_WRITE  , 4},  // 0000 0100 //
        {PermissionCode::PERMISSION_MODIFY , 8},  // 0000 1000 //
        {PermissionCode::PERMISSION_DELETE ,16},  // 0001 0000 //
        {PermissionCode::PERMISSION_SHARE  ,32},  // 0010 0000 //
        {PermissionCode::PERMISSION_EXECUTE,64},  // 0100 0000 //
    };
    if(permission_code_map.find(permission_code) != permission_code_map.end()) {
        return permission_code_map[permission_code];
    }
    return 0;
}

}  // namespace Utils::Transform

namespace Utils {
void set_success(Result *result, const SuccessCode&code) {
    if(result) {
        result->set_success_code(code);
        result->set_message(Utils::Transform::to_message(code));
    }
}
void set_error(Result *result, const ErrorCode&code) {
    if(result) {
        result->set_error_code(code);
        result->set_message(Utils::Transform::to_message(code));
    }
}

} // namespace Utils

// 算法相关的工具函数
namespace Utils::Algorithm {

// 计算字符串的 MD5 哈希值，返回 32 字符的十六进制字符串
string md5(const string& input) {
    MD5 md5;
    return md5(input);
}

string calculate_file_md5(const path& file_path) {
    if (!Utils::exists(file_path)) {
        LOG_ERROR("File does not exist: {}", file_path.string());
        return string{};
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        LOG_ERROR("Failed to open file for MD5 calculation: {}", file_path.string());
        return string{};
    }

    BytesMd5 bytes_md5;
    const size_t buffer_size = 8192;
    char buffer[buffer_size] = {};
    while (file.read(buffer, buffer_size) || file.gcount() > 0) {
        bytes_md5.update(buffer, file.gcount());
    }
    return bytes_md5.get_hex_string();
}

// 流式即时计算 MD5 的工具类
void BytesMd5::reset() {
    m_md5.reset();
}
void BytesMd5::update(const char* data, size_t len) {
    m_md5.add(data, len);
}
string BytesMd5::get_hex_string() {
    return m_md5.getHash();
}

}  // namespace Utils::Algorithm
