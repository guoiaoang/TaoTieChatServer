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
// server
#include "taotie_chat.grpc.pb.h"

ABSL_FLAG(uint16_t, port, 50051, "Server port for the service");

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

// 用户在线状态缓存
static std::map<std::string, std::string> user_status = {
    {"user1", "online"},
    {"user2", "offline"},
    {"user3", "away"},
};
// 用户 token 缓存
static std::map<std::string, std::set<std::string>> user_tokens = {
    {"user1", {"token1", "token11"}},
    {"user2", {"token2"}},
    {"user3", {"token3"}},
};

// 服务端实现
class StatusCheckerServiceImpl final : public StatusChecker::Service {
public:
    // GetStatus 返回给定请求的状态信息。
    // 暂不校验是否有查询权限
    Status GetStatus(ServerContext* context, const StatusRequest* request, StatusReply* response) override {
        // DEBUG // 记录请求日志
        // auto&target_id          = request->target_id();
        // auto&request_user_id    = request->request_user_id();
        // auto&request_user_token = request->request_user_token();
        // ABSL_LOG(INFO) << absl::StrFormat(
        //     "Received GetStatus request: target_id=%s, request_user_id=%s, request_user_token=%s",
        //     target_id, request_user_id, request_user_token);

        response->mutable_request()->CopyFrom(*request);
        // 被查询用户 id 为空时返回服务器的状态信息
        if(request->target_id().empty()) {
            response->mutable_success()->set_message("Server is running");
        }
        // 查询用户 id 不为空时要求必须有查询者的用户 ID
        else if(request->request_user_id().empty()) {
            response->mutable_error()->set_message("Request User ID is required");
        }
        // 查询用户 id 不为空时要求必须有查询者的用户 ID
        else if(request->request_user_token().empty()) {
            response->mutable_error()->set_message("Request User Token is required");
        }
        // id 和 token 都不为空时返回用户的状态信息
        else {
            auto user_tokens_it = user_tokens.find(request->request_user_id());
            // 校验用户 ID 的合法性
            if (user_tokens_it == user_tokens.end()) {
                response->mutable_error()->set_message("Invalid Request User ID");
            }
            // 校验 token 的合法性  // 同一用户可以有多个 token
            else if (user_tokens_it->second.find(request->request_user_token()) == user_tokens_it->second.end()) {
                response->mutable_error()->set_message("Invalid Request User Token");
            }
            else {
                // 查询用户 ID 和 token 都合法时返回被查询用户的状态信息
                auto it = user_status.find(request->target_id());
                if (it != user_status.end()) {
                    response->mutable_success()->set_message(absl::StrFormat("User %s is %s", request->target_id(), it->second));
                } else {
                    response->mutable_error()->set_message("Invalid target User ID");
                }
            }
        }
        return Status::OK;
    }
};

int main(int argc, char** argv) {
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();
    // 设置日志记录的最小等级
    absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);
    // 设置日志输出到标准（错误）输出的最小等级 // 可能造成重复输出
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    std::string server_address = absl::StrFormat("0.0.0.0:%d", absl::GetFlag(FLAGS_port));
    ABSL_LOG(INFO) << "Server listening on " << server_address;

    ServerBuilder server_builder;
    server_builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // 注册服务
    StatusCheckerServiceImpl service_status_checker;
    server_builder.RegisterService(&service_status_checker);
    std::unique_ptr<Server> server = server_builder.BuildAndStart();
    server->Wait();

    return 0;
}

// callback 示例 : `helloworld/greeter_callback_server.cc`，`helloworld/greeter_callback_server.cc`
// 客户端推荐使用 `stub->async()->func()` 方式调用异步 API，
// 服务端推荐使用 `ServerBuilder` 注册服务并调用 `BuildAndStart()` 启动服务器，避免直接使用 `Server` 和 `ServerInterface` 的 API。
