#pragma once
#include "headers.h"
// enum ::TaoTieChat::ErrorCode;
namespace TaoTieChat {
    enum ErrorCode;
} 

class Utils{
    Utils() = default;
public:
    // static std::shared_ptr<Utils> instance() {
    //     static std::shared_ptr<Utils> inst(new Utils());
    //     return inst;
    // }
    static Utils& instance();
    ~Utils() = default;  // shared_ptr 会调用该析构函数
    Utils(const Utils&) = delete;
    Utils& operator=(const Utils&) = delete;

public:
    static std::tuple<bool, const std::vector<std::string>&> store_args(int argc, char** argv);

    // ============================== 路径相关 ============================== //
    static std::filesystem::path app_path();
    static std::filesystem::path data_path();
    static std::filesystem::path logs_path();
    static std::tuple<bool, const std::filesystem::path&> mkdir(const std::filesystem::path& path);

    // ============================== UUID 及加密相关 ======================= //
    static boost::uuids::uuid generate_uuid();
    static boost::uuids::uuid to_uuid(const std::string& s);
    static std::string generate_token();
    // 计算字符串的 SHA-1 哈希值，返回 40 字符的十六进制字符串
    static std::string sha1(const std::string& input);
    // 计算字符串的 MD5 哈希值，返回 32 字符的十六进制字符串
    static std::string md5(const std::string& input);
    // 先使用 MD5 进行哈希加密，实际应用中可以考虑使用更安全的算法如 bcrypt 或 Argon2，并添加盐值以增强安全性。
    static std::string hash_password(const std::string& password);
    static std::string hash_security_answer(const std::string& answer);
    static std::string hash_token(const std::string& token);
    static std::string to_string(const boost::uuids::uuid& u);

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
    static std::chrono::system_clock::time_point current_time_point();
    // 获取 UTC 时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
    static std::string to_utc_string(const std::chrono::system_clock::time_point& tp);
    // 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
    static std::string to_local_string(const std::chrono::system_clock::time_point& tp);
    // 获取当前 UTC 时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
    static std::string utc_date_time_str();
    // 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
    static std::string local_date_time_str();
    // 将 UTC 时间字符串转换为时间点
    // static std::chrono::system_clock::time_point to_time_point(const std::string& utc_str) {
    //     return std::chrono::system_clock::from_time_t(std::mktime(std::gmtime(&utc_str[0])));
    // }
    // 将 UTC 时间字符串转换为时间点
    static std::chrono::system_clock::time_point to_time_point(const std::string& utc_str);
    // 将 UTC 时间字符串转换为本地时间字符串
    static std::string utc_to_local(const std::string& utc_str);

    // ====================================================================== //
    // 获取错误信息
    static std::string_view to_message_view  (::TaoTieChat::SuccessCode code);
    static std::string      to_message_string(::TaoTieChat::SuccessCode code);
    // 获取错误信息
    static std::string_view to_message_view  (::TaoTieChat::ErrorCode code);
    static std::string      to_message_string(::TaoTieChat::ErrorCode code);

private:
    inline static std::vector<std::string> s_args{};
    inline static std::string s_product_name{"TaoTieChatServer"};
    inline static boost::uuids::random_generator m_uuid_generator;
};