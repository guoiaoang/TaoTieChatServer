// std
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
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
// protos
#include "taotie_chat.grpc.pb.h"

ABSL_FLAG(std::string, target, "localhost:50051", "Server address");

using CallbackGetStatusNotify = std::function<void(::grpc::ClientContext* context, const ::TaoTieChat::StatusRequest* request, ::TaoTieChat::StatusReply* response, ::grpc::Status status)>;
void GetStatus(std::shared_ptr<grpc::Channel> channel, const std::string& target_id, const std::string& user_id, const std::string& token, CallbackGetStatusNotify callback) {
    using grpc::Channel;
    using grpc::ClientContext;
    using grpc::Status;
    using TaoTieChat::StatusService;
    using TaoTieChat::StatusRequest;
    using TaoTieChat::StatusReply;

    // 创建 gRPC 通道
    std::unique_ptr<StatusService::Stub> stub = StatusService::NewStub(channel);

    // 创建请求并调用 GetStatus 方法
    StatusRequest request;
    request.set_target_id(target_id);
    request.set_request_user_id(user_id);
    request.set_request_user_token(token);

    StatusReply response;
    ClientContext context;

    if(callback) {
        // 防止局部变量提前销毁
        std::mutex mu;
        std::condition_variable cv;
        bool done = false;  // 必须

        // 函数参数传递
        auto lambda_callback = [callback, &context, &request, &response, &mu, &cv, &done](::grpc::Status s) {
            callback(&context, &request, &response, std::move(s));
            // 执行完回调后再通知销毁
            std::lock_guard<std::mutex> lock(mu);
            done = true;
            cv.notify_one();
        };

        // 执行异步调用
        stub->async()->GetStatus(&context, &request, &response, lambda_callback);

        // 等待异步回调完成
        std::unique_lock<std::mutex> lock(mu);
        while (!done) {
            cv.wait(lock);
        }
    }
}

int main(int argc, char** argv) {
    using std::shared_ptr;
    using std::string;
    using grpc::Channel;

    // 解析命令行参数并初始化日志系统
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();
    absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

    // 客户端实现
    string target_str = absl::GetFlag(FLAGS_target);
    shared_ptr<Channel> channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());

    using ::grpc::ClientContext;
    using ::TaoTieChat::StatusRequest;
    using ::TaoTieChat::StatusReply;
    using ::grpc::Status;
    auto notify = [](ClientContext* context, const StatusRequest* request, StatusReply* response, Status status) {
        if (status.ok()) {
            std::cout << "RPC succeeded" << std::endl;
            std::cout << "Response: " << response->DebugString() << std::endl;
        } else {
            std::cout << "RPC failed: " << status.error_message() << std::endl;
        }
    };

    ABSL_LOG(INFO) << "GetStatus target id empty";
    GetStatus(channel, "", "user1", "token1", notify);

    ABSL_LOG(INFO) << "GetStatus request user id empty";
    GetStatus(channel, "user1", "", "token1", notify);

    ABSL_LOG(INFO) << "GetStatus request user token empty";
    GetStatus(channel, "user1", "user1", "", notify);
    // 服务器先校验 user id 和 token 不为空再校验是否正确，因此需要填充 token
    ABSL_LOG(INFO) << "GetStatus request user id error";
    GetStatus(channel, "user1", "user_", "token1", notify);

    ABSL_LOG(INFO) << "GetStatus request user token error";
    GetStatus(channel, "user1", "user1", "token_", notify);

    ABSL_LOG(INFO) << "GetStatus target id error";
    GetStatus(channel, "user_", "user1", "token1", notify);

    ABSL_LOG(INFO) << "GetStatus self status";
    GetStatus(channel, "user1", "user1", "token1", notify);

    ABSL_LOG(INFO) << "GetStatus other user status";
    GetStatus(channel, "user2", "user1", "token1", notify);

    ABSL_LOG(INFO) << "GetStatus other user status";
    GetStatus(channel, "user3", "user1", "token11", notify);

    return EXIT_SUCCESS;
}
