#include "headers.h"
#include "utils.h"

#include "DBSQLCipherCmds.h"
#include "DBSQLCipher.h"
#include "StatusService.h"
#include "AccountService.h"
#include "SearchService.h"
#include "MessageService.h"
#include "FileService.h"

using std::filesystem::path;
using std::string;

bool initialize_spdlog();

// absl flag
ABSL_FLAG(uint16_t, port, 50051, "Server port for the service");

int main(int argc, char** argv) {
    absl::ParseCommandLine(argc, argv);

    initialize_spdlog();

    // 输出基本信息
    // 输出程序完整路径
    LOG_DEBUG("Program path: {}", Utils::app_path().string());
    // 输出当前工作目录
    LOG_DEBUG("Current working directory: {}", Utils::app_dir().string());

    // 数据库路径和加密密钥
    const path db_path = Utils::data_dir() / (TOSTRING(PROJECT_NAME) ".db");
    const string encryption_key = "your_secure_encryption_key";

    // 创建数据库目录
    auto [success, exists_data_dir] = Utils::mkdir(db_path.parent_path());
    if(!exists_data_dir) {
        LOG_ERROR("Failed to create data directory: {}", db_path.parent_path().string());
        return 0;
    }

    // 初始化数据库连接
    DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
    db_sqlcipher.set_init_params(db_path, encryption_key);
    if (!db_sqlcipher.database_open()) {
        LOG_ERROR("Failed to open database with encryption key.");
        return 0;
    }
    if (!db_sqlcipher.database_initialize()) {
        LOG_ERROR("Failed to initialize database schema.");
        return 0;
    }

    // 启用 gRPC 服务器的默认健康检查服务和反射服务
    grpc::EnableDefaultHealthCheckService(true);
    // 反射服务允许客户端在运行时查询服务器支持的服务和方法，这对于调试和开发非常有用。
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    // 构建服务器地址
    std::string server_address = absl::StrFormat("0.0.0.0:%d", absl::GetFlag(FLAGS_port));
    LOG_INFO("Server listening on {}", server_address);

    // 构建并启动 gRPC 服务器
    int selected_port = 0;
    ServerBuilder server_builder;
    server_builder.AddListeningPort(server_address, grpc::InsecureServerCredentials(), &selected_port);
    // 注册服务
    StatusServiceImpl               service_status_checker;
    server_builder.RegisterService(&service_status_checker);
    AccountServiceImpl              service_account;
    server_builder.RegisterService(&service_account);
    SearchServiceImpl               service_search;
    server_builder.RegisterService(&service_search);
    MessageServiceImpl              service_message;
    server_builder.RegisterService(&service_message);
    FileServiceImpl                 service_file;
    server_builder.RegisterService(&service_file);

    // 校验端口是否绑定成功，以防 server 为 nullptr
    if (selected_port == 0) {
        LOG_ERROR("Failed to bind listening port: {}", server_address);
    }
    std::unique_ptr<Server> server = server_builder.BuildAndStart();
    if (!server) {
        LOG_FATAL("gRPC server failed to start."
            " Check whether port {} is already in use and "
            "whether service registration is valid.", server_address);
        return 1;
    }
    LOG_INFO("gRPC server started successfully on {} (selected port: {})", server_address, selected_port);
    server->Wait();

    return 0;
}

bool initialize_spdlog() {
    // 设置日志目录
    path log_dir = Utils::logs_dir();
    auto [success, exists_log_dir] = Utils::mkdir(log_dir);
    if(!exists_log_dir) {
        LOG_ERROR("Failed to create log directory: {}", log_dir.string());
        return false;
    }

    // spdlog 初始化

    // [%n:multi_sink][%l%:info][%P:22392-%t:18300]
    // [%@:C:\workspace\projects\qt\TaoTieChatServer\src\main.cpp:351]
    // [%g:C:\workspace\projects\qt\TaoTieChatServer\src\main.cpp]
    // [%s:main.cpp][%#:351][%!:main]
    std::string log_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%!] [%P-%t] %v";
    // 创建控制台 sink
    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    // stderr：err 及以上
    stderr_sink->set_level(spdlog::level::err);
    // stdout 需要“上限过滤” -> 包一层 callback sink 做级别判断
    auto stdout_filter_sink = std::make_shared<spdlog::sinks::callback_sink_mt>(
        [stdout_sink](const spdlog::details::log_msg& msg) {
            if (msg.level < spdlog::level::err) {
                stdout_sink->log(msg);
            }
        });

    // 创建按日期轮转的文件 sink
    // auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/log.txt", 1024*1024*500, 3);  // 500MB, 3 个文件
    auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>((log_dir / "daily.log").string(), 0, 0);
    file_sink->set_level(spdlog::level::trace);
    // file_sink->set_pattern(log_pattern);
    // 组合 sinks
    spdlog::sinks_init_list sink_list = {stdout_filter_sink, stderr_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>("multi_sink", sink_list);
    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::info);     // 可选：减少“看起来没输出”的感觉
    spdlog::default_logger()->set_level(spdlog::level::trace);   // 关键：放开到最低级别
    spdlog::default_logger()->set_pattern(log_pattern);  // 关键：每个 sink 都会复制一份 pattern，确保输出格式一致
    // spdlog::flush_every(std::chrono::seconds(3));
    // spdlog::set_error_handler([](const std::string &msg) { spdlog::get("console")->error("*** LOGGER ERROR ***: {}", msg); });

    // 使用
    // spdlog::debug("debug message to default logger (multi_sink)");
    // spdlog::error("error message to default logger (multi_sink)");
    // spdlog::trace("trace message to default logger (multi_sink)");

    // 在 spdlog 中，要输出文件名、行号和函数名，不能直接调用 logger->info() 等成员函数，而必须配合 宏（Macros）和 格式化占位符 使用。
    // SPDLOG_INFO("带源码信息的日志");  // 宏版本会带上源码信息，默认日志格式里也包含了源码信息，所以这里不需要再设置一次了。

    return true;
};

// spdlog 初始化，返回值不重要，关键是要在 main() 之前执行
// static const bool g_init_spdlog = initialize_spdlog();

// callback 示例 : `helloworld/greeter_callback_server.cc`，`helloworld/greeter_callback_server.cc`
// 客户端推荐使用 `stub->async()->func()` 方式调用异步 API，
// 服务端推荐使用 `ServerBuilder` 注册服务并调用 `BuildAndStart()` 启动服务器，避免直接使用 `Server` 和 `ServerInterface` 的 API。
