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

using std::shared_ptr;
using std::make_shared;
using std::unique_ptr;
using std::lock_guard;
using std::unique_lock;
using std::condition_variable;
using std::mutex;
using std::string;
using std::string;
using std::function;
using Context = ::grpc::ClientContext;
using Channel = ::grpc::Channel;
using Status  = ::grpc::Status;
using Stub    = ::TaoTieChat::AccountService::Stub;
using Service = ::TaoTieChat::AccountService;
using Request = ::TaoTieChat::SignInRequest;
using Reply   = ::TaoTieChat::SignInReply;

ABSL_FLAG(string, target, "localhost:50051", "Server address");

namespace Register {
using Request = ::TaoTieChat::SignUpRequest;
using Reply   = ::TaoTieChat::SignUpReply;

using Notify = function<void(Context* context, const Request* request, Reply* response, Status status)>;
void SignUp(shared_ptr<Channel> channel, const Request& request, Notify callback) {
    // 创建 gRPC 通道
    unique_ptr<Stub> stub = Service::NewStub(channel);

    Reply   response;
    Context context;

    if(callback) {
        // 防止局部变量提前销毁
        mutex mu;
        condition_variable cv;
        bool done = false;  // 必须

        // 函数参数传递
        auto lambda_callback = [callback, &context, &request, &response, &mu, &cv, &done](Status status) {
            if(callback) {
                callback(&context, &request, &response, std::move(status));
            }
            // 执行完回调后再通知销毁
            lock_guard<mutex> lock(mu);
            done = true;
            cv.notify_one();
        };

        // 执行异步调用
        stub->async()->SignUp(&context, &request, &response, lambda_callback);

        // 等待异步回调完成
        unique_lock<mutex> lock(mu);
        while (!done) {
            cv.wait(lock);
        }
    }
}

std::string sign_up(shared_ptr<Channel> channel, std::string user_id, std::string password) {
    shared_ptr<Request> request = make_shared<Request>();
    request->set_user_id(user_id);
    request->set_password(password);
    request->set_confirm(password);
    request->set_security_question("security_question");
    request->set_security_answer("security_answer");
    Reply result;
    auto notify = [&result](Context* context, const Request* request, Reply* response, Status status) {
        result.CopyFrom(*response);
    };
    SignUp(channel, *request, notify);
    CHECK_EQ(result.result().has_success_code(), true);  // 注册失败时抛出异常，退出测试
    return result.account_id();  // 其实登录时用不到 account_id
}

std::string random_string() {
    std::srand((unsigned int)std::time(nullptr));
    return fmt::format("test_user_id_{}", std::rand());
}
}

using Notify = function<void(Context* context, const Request* request, Reply* response, Status status)>;
void SignIn(shared_ptr<Channel> channel, const Request& request, Notify callback) {
    // 创建 gRPC 通道
    unique_ptr<Stub> stub = Service::NewStub(channel);

    Reply   response;
    Context context;

    if(callback) {
        // 防止局部变量提前销毁
        mutex mu;
        condition_variable cv;
        bool done = false;  // 必须

        // 函数参数传递
        auto lambda_callback = [callback, &context, &request, &response, &mu, &cv, &done](Status status) {
            callback(&context, &request, &response, std::move(status));
            // 执行完回调后再通知销毁
            lock_guard<mutex> lock(mu);
            done = true;
            cv.notify_one();
        };

        // 执行异步调用
        stub->async()->SignIn(&context, &request, &response, lambda_callback);

        // 等待异步回调完成
        unique_lock<mutex> lock(mu);
        while (!done) {
            cv.wait(lock);
        }
    }
}

int main(int argc, char** argv) {
    // 解析命令行参数并初始化日志系统
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();
    absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

    // 客户端实现
    string target_str = absl::GetFlag(FLAGS_target);
    shared_ptr<Channel> channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());

    Reply result;
    auto notify = [&result](Context* context, const Request* request, Reply* response, Status status) {
        result.CopyFrom(*response);
    };

    // 先注册用户，必须成功注册  // 其实登录时用不到 account_id
    std::string user_id    = "test_user_id_" + Register::random_string();
    std::string password   = "password";
    std::string account_id = Register::sign_up(channel, user_id, password);
    std::string token      = "error_token";

    auto get_code = [](const Reply& reply) {
        if(reply.result().has_success_code()) {
            return (int)reply.result().success_code();
        }
        else if(reply.result().has_error_code()) {
            return (int)reply.result().error_code();
        }
        return -1;
    };

    // 用不到的值需要调用 clear 函数，而不能直接设置为空字符串
    ABSL_LOG(INFO) << "SignIn test started.";
    shared_ptr<Request> request = make_shared<Request>();

    auto auth = request->mutable_request_user_auth();

    // 登陆 ID 为空
    // request->set_user_id("");
    // request->mutable_request_user_auth()->clear_user_id();
    auth->clear_user_id();
    auth->clear_password();
    auth->set_token(token);
    ABSL_LOG(INFO) <<    "SignIn by no user id. user_id=" << auth->user_id() << ", password=" << auth->password() << ", token=" << auth->token();
    SignIn(channel, *request, notify);
    LOG_INFO(fmt::format("SignIn by no user id. Result: code={:#010x}, message={}, account_id={}", get_code(result), result.result().message(), result.account_id()));

    // 登陆秘钥为空
    auth->set_user_id(user_id + "_not_registered");
    auth->clear_password();
    auth->clear_token();
    ABSL_LOG(INFO) <<    "SignIn by no token. user_id=" << auth->user_id() << ", password=" << auth->password() << ", token=" << auth->token();
    SignIn(channel, *request, notify);
    LOG_INFO(fmt::format("SignIn by no token. Result: code={:#010x}, message={}, account_id={}", get_code(result), result.result().message(), result.account_id()));

    // 未注册用户
    auth->set_user_id(user_id + "_not_registered");
    auth->clear_password();
    auth->set_token(token);
    ABSL_LOG(INFO) <<    "SignIn by user not registered. user_id=" << auth->user_id() << ", password=" << auth->password() << ", token=" << auth->token();
    SignIn(channel, *request, notify);
    LOG_INFO(fmt::format("SignIn by user not registered. Result: code={:#010x}, message={}, account_id={}", get_code(result), result.result().message(), result.account_id()));

    // 已注册用户，账户密码登录
    auth->set_user_id(user_id);
    auth->set_password(password);
    auth->clear_token();
    ABSL_LOG(INFO) <<    "SignIn by user_id + password. user_id=" << auth->user_id() << ", password=" << auth->password() << ", token=" << auth->token();
    SignIn(channel, *request, notify);
    LOG_INFO(fmt::format("SignIn by user_id + password. Result: code={:#010x}, message={}, account_id={}", get_code(result), result.result().message(), result.account_id()));

    token = result.token();
    // 已注册用户，账户 token 登录
    auth->set_user_id(user_id);
    auth->clear_password();
    auth->set_token(token);
    ABSL_LOG(INFO) <<    "SignIn by user_id + token. user_id=" << auth->user_id() << ", password=" << auth->password() << ", token=" << auth->token();
    SignIn(channel, *request, notify);
    LOG_INFO(fmt::format("SignIn by user_id + token. Result: code={:#010x}, message={}, account_id={}", get_code(result), result.result().message(), result.account_id()));

    // 已注册用户，错误密码登录
    auth->set_user_id(user_id);
    auth->set_password(password + "_not_correct");
    auth->clear_token();
    ABSL_LOG(INFO) <<    "SignIn by user_id + error password. user_id=" << auth->user_id() << ", password=" << auth->password() << ", token=" << auth->token();
    SignIn(channel, *request, notify);
    LOG_INFO(fmt::format("SignIn by user_id + error password. Result: code={:#010x}, message={}, account_id={}", get_code(result), result.result().message(), result.account_id()));

    // 已注册用户，错误 token 登录
    auth->set_user_id(user_id);
    auth->clear_password();
    auth->set_token(token + "_not_correct");
    ABSL_LOG(INFO) <<    "SignIn by user_id + error token. user_id=" << auth->user_id() << ", password=" << auth->password() << ", token=" << auth->token();
    SignIn(channel, *request, notify);
    LOG_INFO(fmt::format("SignIn by user_id + error token. Result: code={:#010x}, message={}, account_id={}", get_code(result), result.result().message(), result.account_id()));

    return EXIT_SUCCESS;
}
