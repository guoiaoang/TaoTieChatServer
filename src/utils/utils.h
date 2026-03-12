#pragma once
#include "headers.h"

class Utils{
    Utils() = default;
public:
    static std::shared_ptr<Utils> instance() {
        static std::shared_ptr<Utils> inst(new Utils());
        return inst;
    }
    // static Utils& instance() {
    //     static Utils inst;
    //     return inst;
    // }

    ~Utils() = default; // shared_ptr 会调用该析构函数
    Utils(const Utils&) = delete;
    Utils& operator=(const Utils&) = delete;

public:
    std::tuple<bool, const std::vector<std::string>&> storeArgs(int argc, char** argv) {
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

    static std::filesystem::path appDir() {
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
        app_dir = path.lexically_normal(); // 规范化路径，去除冗余部分
        return app_dir;
    }

    static std::filesystem::path dataDir() {
        std::filesystem::path data_dir = appDir() / "data";
        return data_dir;
    }

    static std::filesystem::path logsDir() {
        std::filesystem::path log_dir = appDir() / "logs";
        return log_dir;
    }

    std::tuple<bool, const std::filesystem::path&> mkdir(const std::filesystem::path& path) {
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

    boost::uuids::uuid generate_uuid() {
        // inline boost::uuids::uuid boost::uuids::basic_random_generator<boost::uuids::detail::chacha20_12>::operator()()
        return m_uuid_generator();
    }

    std::string to_string(const boost::uuids::uuid& u) {
        return boost::uuids::to_string(u);
    }

    boost::uuids::uuid to_uuid(const std::string& s) {
        return boost::uuids::string_generator()(s);
    }

    std::chrono::system_clock::time_point currentDateTime() {
        return std::chrono::system_clock::now();
    }

    std::string to_string(const std::chrono::system_clock::time_point& tp) {
        // 获取本地时区并创建带时区的时间对象
        auto local_time = std::chrono::zoned_time{std::chrono::current_zone(), tp};
        return std::format("{:%F %T %z}", local_time);
    }


private:
    inline static std::vector<std::string> s_args{};
    inline static std::string s_product_name{"TaoTieChatServer"};
    boost::uuids::random_generator m_uuid_generator;
};