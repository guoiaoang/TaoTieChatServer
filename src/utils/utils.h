#pragma once
#include "headers.h"

class Utils{
    Utils() = default;
public:
    static std::shared_ptr<Utils> instance() {
        static std::shared_ptr<Utils> inst(new Utils());
        return inst;
    }

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

private:
    inline static std::vector<std::string> s_args{};
    inline static std::string s_product_name{"TaoTieChatServer"};
};