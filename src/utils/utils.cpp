#include "utils.h"
// #include "taotie_chat.h"
// enum ::TaoTieChat::ErrorCode;

Utils& Utils::instance() {
    static Utils inst;
    return inst;
}

std::tuple<bool, const std::vector<std::string>&> Utils::store_args(int argc, char** argv) {
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

std::filesystem::path Utils::app_path() {
    // 为了保证数据位置在程序启动后的一致性，这里仅返回初始化时设置的位置，后续不再更改。
    static std::filesystem::path app_dir;
    if (!app_dir.empty()) {
        return app_dir;
    }

    // 设置路径
    std::filesystem::path base_path;
    if(s_args.empty()) {
        LOG_ERROR("No command line arguments found, using current working directory as base path.");
        base_path = std::filesystem::current_path();
    }
    else {
        base_path = std::filesystem::absolute(s_args[0]).parent_path();
    }
    std::filesystem::path path = base_path / s_product_name;
    app_dir = path.lexically_normal();  // 规范化路径，去除冗余部分
    return app_dir;
}

std::filesystem::path Utils::data_path() {
    std::filesystem::path data_dir = app_path() / "data";
    return data_dir;
}

std::filesystem::path Utils::logs_path() {
    std::filesystem::path log_dir = app_path() / "logs";
    return log_dir;
}

std::tuple<bool, const std::filesystem::path&> Utils::mkdir(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        LOG_WARNING("Directory already exists: {}", path.string());
        return {true, path};
    }

    // 创建
    try {
        std::filesystem::create_directories(path);
        LOG_INFO("Directory created successfully: {}", path.string());
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR("Failed to create directory {}: {}", path.string(), e.what());
        return {false, path};
    }

    // 验证
    if(!std::filesystem::exists(path)) {
        LOG_ERROR("Directory does not exist after creation attempt: {}", path.string());
        return {false, path};
    }
    return {true, path};
}

// ============================== UUID 及加密相关 ======================= //

boost::uuids::uuid Utils::generate_uuid() {
    // inline boost::uuids::uuid boost::uuids::basic_random_generator<boost::uuids::detail::chacha20_12>::operator()()
    return m_uuid_generator();
}

std::string Utils::generate_token() {
    return to_string(Utils::generate_uuid());
}

// 计算字符串的 SHA-1 哈希值，返回 40 字符的十六进制字符串
std::string Utils::sha1(const std::string& input) {
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(input.data(), input.size());
    boost::uuids::detail::sha1::digest_type digest;
    sha1.get_digest(digest);
    std::string result;
    result.reserve(40);
    for (int i = 0; i < 5; ++i) {
        result += fmt::format("{:08x}", digest[i]);
    }
    return result;
}

// 计算字符串的 MD5 哈希值，返回 32 字符的十六进制字符串
std::string Utils::md5(const std::string& input) {
    boost::uuids::detail::md5 md5;
    md5.process_bytes(input.data(), input.size());
    boost::uuids::detail::md5::digest_type digest;
    md5.get_digest(digest);
    std::string result;
    result.reserve(32);
    for (int i = 0; i < 4; ++i) {
        result += fmt::format("{:08x}", digest[i]);
    }
    return result;
}

// 先使用 MD5 进行哈希加密，实际应用中可以考虑使用更安全的算法如 bcrypt 或 Argon2，并添加盐值以增强安全性。
std::string Utils::hash_password(const std::string& password) {
    return Utils::md5(password);
}

std::string Utils::hash_security_answer(const std::string& answer) {
    return Utils::md5(answer);
}

std::string Utils::hash_token(const std::string& token) {
    return Utils::md5(token);
}

std::string Utils::to_string(const boost::uuids::uuid& u) {
    return boost::uuids::to_string(u);
}

boost::uuids::uuid Utils::to_uuid(const std::string& s) {
    return boost::uuids::string_generator()(s);
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
std::chrono::system_clock::time_point Utils::current_time_point() {
    return std::chrono::system_clock::now();
}

// 获取 UTC 时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
std::string Utils::to_utc_string(const std::chrono::system_clock::time_point& tp) {
    // 获取 UTC 时区并创建带时区的时间对象
    return std::format("{:%Y-%m-%d %H:%M:%S}", tp);
}

// 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
std::string Utils::to_local_string(const std::chrono::system_clock::time_point& tp) {
    // 获取本地时区并创建带时区的时间对象
    auto local_time = std::chrono::zoned_time{std::chrono::current_zone(), tp};
    return std::format("{:%Y-%m-%d %H:%M:%S}", local_time);
}

// 获取当前 UTC 时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
std::string Utils::utc_date_time_str() {
    return to_utc_string(current_time_point());
}

// 获取本地时间字符串，格式为 "YYYY-MM-DD HH:MM:SS"
std::string Utils::local_date_time_str() {
    return to_local_string(current_time_point());
}

// 将 UTC 时间字符串转换为时间点
// static std::chrono::system_clock::time_point to_time_point(const std::string& utc_str) {
//     return std::chrono::system_clock::from_time_t(std::mktime(std::gmtime(&utc_str[0])));
// }

// 将 UTC 时间字符串转换为时间点
std::chrono::system_clock::time_point Utils::to_time_point(const std::string& utc_str) {
    std::tm tm = {};
    std::stringstream ss(utc_str);

    // 使用 C++20 的 parse (部分编译器需配合 std::chrono::from_stream)
    std::chrono::system_clock::time_point tp;
    ss >> std::chrono::parse("%Y-%m-%d %H:%M:%S", tp);
    // if (ss.fail()) {
    //     LOG_ERROR("Failed to parse UTC time string: {}", utc_str);
    //     return std::chrono::system_clock::time_point{};  // 返回默认时间点
    // }
    return tp;
}


// 将 UTC 时间字符串转换为本地时间字符串
std::string Utils::utc_to_local(const std::string& utc_str) {
    auto tp = Utils::to_time_point(utc_str);
    return Utils::to_local_string(tp);
}

// ============================== std 扩展 ============================== //

std::vector<std::string> Utils::split(const std::string&src, const std::string&div) {
    std::vector<std::string> result;
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

const std::string Utils::join(std::vector<std::string>&src, const std::string&div) {
    std::string result;
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
std::string_view Utils::to_message_view(::TaoTieChat::SuccessCode code) {
    using sc = ::TaoTieChat::SuccessCode;

    static const std::map<sc, std::string_view> map_codes = {
        {sc::SUCCESS       , "Success"},
        {sc::STATUS_SUCCESS, "Success"},
        {sc::STATUS_RUNNING, "Running"},
        {sc::STATUS_ONLINE , "Online"},
    };

    if (map_codes.contains(code)) {
        return map_codes.at(code);
    }

    return "Unknown Success Code";
}

std::string Utils::to_message_string(::TaoTieChat::SuccessCode code) {
    return std::string(to_message_view(code));
}

// 获取错误信息
std::string_view Utils::to_message_view(::TaoTieChat::ErrorCode code) {
    using ec = ::TaoTieChat::ErrorCode;

    static const std::map<ec, std::string_view> map_codes = {
        {ec::UNSET                         , "Unset Error Code"       },
        {ec::UNKNOWN                       , "Unknown Error Code"     },
        {ec::ERROR_INIT                    , "Error on Init"          },
        {ec::ERROR_CONN                    , "Error on connect"       },
        {ec::ERROR_RECV                    , "Error on Recv or Parse" },
        {ec::ERROR_SEND                    , "Error on Send"          },
        // 数据库相关错误码
        {ec::DB_QUERY                       , "DB Query Failed"  },
        {ec::DB_QUERY_EMPTY                 , "DB Query Empty"   },
        {ec::DB_EXECUTE                     , "DB Execute Failed"},
        {ec::DB_EXECUTE_EMPTY               , "DB Execute Empty" },
        {ec::DB_CHECK                       , "DB Check Failed"  },
        {ec::DB_CHECK_EMPTY                 , "DB Check Empty"   },
        // error
        {ec::STATUS_TARGET_ID_EMPTY         , "Target ID is empty"            },
        {ec::STATUS_REQUEST_USER_ID_EMPTY   , "Request User ID is empty"      },
        {ec::STATUS_REQUEST_USER_TOKEN_EMPTY, "Request User Token is empty"   },
        {ec::STATUS_TARGET_ID_NOT_FOUND     , "Target ID not found"           },
        {ec::STATUS_USER_ID_INVALID         , "Invalid Request User ID"       },
        {ec::STATUS_USER_TOKEN_INVALID      , "Invalid Request User Token"    },
        {ec::SIGN_UP_USER_ID_EMPTY          , "User ID is empty"              },
        {ec::SIGN_UP_PASSWORD_EMPTY         , "Password is empty"             },
        {ec::SIGN_UP_CONFIRM_PASSWORD_EMPTY , "Confirm Password is empty"     },
        {ec::SIGN_UP_SECURITY_QUESTION_EMPTY, "Security Question is empty"    },
        {ec::SIGN_UP_SECURITY_ANSWER_EMPTY  , "Security Answer is empty"      },
        {ec::SIGN_UP_DIFF_CONFIRM_PASSWORD  , "Confirm Password is not equal" },
        {ec::SIGN_UP_USER_ID_EXISTS         , "User ID already exists"        },
        {ec::SIGN_IN_USER_ID_EMPTY          , "User ID is empty"              },
        {ec::SIGN_IN_PASSWORD_EMPTY         , "Password is empty"             },
        {ec::SIGN_IN_TOKEN_EMPTY            , "Token is empty"                },
        {ec::SIGN_IN_INVALID_LOGIN_METHOD   , "Invalid login method"          },
        {ec::SIGN_IN_USER_ID_NOT_FOUND      , "User ID not found"             },
        {ec::SIGN_IN_PASSWORD_INCORRECT     , "Password is incorrect"         },
        {ec::SIGN_IN_TOKEN_INCORRECT        , "Token is incorrect"            },
        {ec::SIGN_IN_AUTH_FAILED            , "User ID or Password or Token error" },
        {ec::SEARCH_USERS_KEYWORD_EMPTY     , "Search Users is empty"         },
        {ec::SEARCH_USERS_KEYWORD_NOT_FOUND , "Search Users not found"        },
    };

    if (map_codes.contains(code)) {
        return map_codes.at(code);
    }

    return "Unknown Error Code";
}

std::string Utils::to_message_string(::TaoTieChat::ErrorCode code) {
    return std::string(to_message_view(code));
}
