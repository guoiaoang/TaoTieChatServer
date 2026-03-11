#pragma once
// std
#include <iostream>
#include <string>
#include <string>
#include <sstream>
#include <memory>
#include <functional>
#include <thread>
#include <chrono>
#include <iostream>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <source_location>
#include <string_view>
// absl
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/absl_log.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/globals.h"
#include "absl/strings/str_format.h"
// grpc
#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
// sqlcipher
#include <sqlcipher/sqlite3.h>
// nlohmann json
#include <nlohmann/json.hpp>
// glog
#include <glog/logging.h>
// spdlog
// 1. 必须在 include 之前定义，指定允许输出的最低日志级别
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE // 允许输出 trace 级别及以上的日志
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <spdlog/async.h>
#include <spdlog/mdc.h>
#include <spdlog/cfg/env.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/callback_sink.h>
// #include "spdlog/sinks/qt_sinks.h"
// #include "spdlog/sinks/syslog_sink.h"
// server
#include "taotie_chat.grpc.pb.h"

// using
using grpc::Status;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerUnaryReactor;
using grpc::CallbackServerContext;
using grpc::ServerReaderWriter;
using TaoTieChat::StatusChecker;
using TaoTieChat::StatusRequest;
using TaoTieChat::StatusReply;

#define LOGGER_USE_SPDLOG
// #define LOGGER_USE_GLOG
// #define LOGGER_USE_ABSL_LOG

// VERBOSE / NOTICE / EMERGENCY / PANIC / AUDIT / ALERT
// FATAL / CRITICAL / ERROR / WARNING / INFO / DEBUG / TRACE
#ifdef LOGGER_USE_SPDLOG
#define LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARNING(...)  SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
#define LOG_FATAL(...)    SPDLOG_CRITICAL(__VA_ARGS__)
#elif defined(LOGGER_USE_GLOG)
#define LOG_TRACE(...)    LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_DEBUG(...)    LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_INFO(...)     LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_WARNING(...)  LOG(GLOG_WARNING) << __VA_ARGS__
#define LOG_ERROR(...)    LOG(GLOG_ERROR) << __VA_ARGS__
#define LOG_CRITICAL(...) LOG(GLOG_FATAL) << __VA_ARGS__
#define LOG_FATAL(...)    LOG(GLOG_FATAL) << __VA_ARGS__
#elif defined(LOGGER_USE_ABSL_LOG)
#define LOG_TRACE(...)    ABSL_LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_DEBUG(...)    ABSL_LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_INFO(...)     ABSL_LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_WARNING(...)  ABSL_LOG(GLOG_WARNING) << __VA_ARGS__
#define LOG_ERROR(...)    ABSL_LOG(GLOG_ERROR) << __VA_ARGS__
#define LOG_CRITICAL(...) ABSL_LOG(GLOG_FATAL) << __VA_ARGS__
#define LOG_FATAL(...)    ABSL_LOG(GLOG_FATAL) << __VA_ARGS__
#else
#define LOG_TRACE(...)    std::cout << __VA_ARGS__ << std::endl
#define LOG_DEBUG(...)    std::cout << __VA_ARGS__ << std::endl
#define LOG_INFO(...)     std::cout << __VA_ARGS__ << std::endl
#define LOG_WARNING(...)  std::cout << __VA_ARGS__ << std::endl
#define LOG_ERROR(...)    std::cerr << __VA_ARGS__ << std::endl
#define LOG_CRITICAL(...) std::cerr << __VA_ARGS__ << std::endl
#define LOG_FATAL(...)    std::cerr << __VA_ARGS__ << std::endl
#endif  // LOGGER_USE_SPDLOG

