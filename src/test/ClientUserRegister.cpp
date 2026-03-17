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
// service parameters
#include "DBSQLCipher.h"

ABSL_FLAG(std::string, target, "localhost:50051", "Server address");

using CallbackUserRegisterNotify = std::function<void(::grpc::ClientContext* context, const ::TaoTieChat::UserRegisterRequest* request, ::TaoTieChat::UserRegisterReply* response, ::grpc::Status status)>;
void UserRegister(std::shared_ptr<grpc::Channel> channel, const DBSQLCipher::UserRegisterParameters& params, CallbackUserRegisterNotify callback) {
    using grpc::Channel;
    using grpc::ClientContext;
    using grpc::Status;
    using TaoTieChat::AccountService;
    using TaoTieChat::UserRegisterRequest;
    using TaoTieChat::UserRegisterReply;

    // 创建 gRPC 通道
    std::unique_ptr<AccountService::Stub> stub = AccountService::NewStub(channel);

    // 创建请求并调用 UserRegister 方法
    UserRegisterRequest request;
    request.set_user_id(params.user_id);
    request.set_password(params.password);
    request.set_security_question(params.security_question);
    request.set_security_answer(params.security_answer);

    UserRegisterReply response;
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
        stub->async()->UserRegister(&context, &request, &response, lambda_callback);

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
    using ::TaoTieChat::UserRegisterRequest;
    using ::TaoTieChat::UserRegisterReply;
    using ::grpc::Status;
    DBSQLCipher::UserRegisterResult result;
    auto notify = [&result](ClientContext* context, const UserRegisterRequest* request, UserRegisterReply* response, Status status) {
        if (status.ok()) {
            std::cout << "RPC succeeded" << std::endl;
            std::cout << "Response: " << response->DebugString() << std::endl;
        }
        else {
            std::cout << "RPC failed: " << status.error_message() << std::endl;
        }

        // 取出结果
        if (status.ok()) {
            result.success       = response->success();
            result.account_id    = response->account_id();
            result.error_message = response->error_message();
        }
        else {
            result.success       = false;
            result.error_message = status.error_message();
        }
    };

    ABSL_LOG(INFO) << "UserRegister test started.";
    DBSQLCipher::UserRegisterParameters params;
    params.user_id = "user_id";
    params.password = "password";
    params.security_question = "security_question";
    params.security_answer = "security_answer";
    // 空数据集第一次注册时应当可以注册成功
    UserRegister(channel, params, notify);
    LOG_INFO("UserRegister 1 test completed. Result: success={}, account_id={}, error_message={}", result.success, result.account_id, result.error_message);
    // 再次注册时会因为账号已存在而注册失败
    UserRegister(channel, params, notify);
    LOG_INFO("UserRegister 2 test completed. Result: success={}, account_id={}, error_message={}", result.success, result.account_id, result.error_message);
    return EXIT_SUCCESS;
}
