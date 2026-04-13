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
#include "gtest/gtest.h"

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
using ::grpc::Channel;
using ::grpc::Status;
using ::TaoTieChat::SuccessCode;
using ::TaoTieChat::ErrorCode;
using Context = ::grpc::ClientContext;
using Stub    = ::TaoTieChat::AccountService::Stub;
using Service = ::TaoTieChat::AccountService;
using Request = ::TaoTieChat::SignUpRequest;
using Reply   = ::TaoTieChat::SignUpReply;

ABSL_FLAG(string, target, "localhost:50051", "Server address");

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

// int main(int argc, char** argv) {
TEST(Client, SignUp) {
    // 解析命令行参数并初始化日志系统
    // absl::ParseCommandLine(argc, argv);
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

    auto get_code = [](const Reply& reply) {
        if(reply.result().has_success_code()) {
            return (int)reply.result().success_code();
        }
        else if(reply.result().has_error_code()) {
            return (int)reply.result().error_code();
        }
        return -1;
    };

    std::srand((unsigned int)std::time(nullptr));
    std::string str_random_user_id = fmt::format("test_user_id_{}", std::rand());

    ABSL_LOG(INFO) << "SignUp test started.";
    shared_ptr<Request> request = make_shared<Request>();
    request->set_user_id(str_random_user_id);
    request->set_password("password");
    request->set_security_question("security_question");
    request->set_security_answer("security_answer");
    // 空数据集第一次注册时应当可以注册成功
    SignUp(channel, *request, notify);
    LOG_INFO(fmt::format("SignUp 1 test completed. Result: code={:#010x}, message={}, account_id={}", get_code(result), result.result().message(), result.account_id()));
    ASSERT_TRUE(!result.account_id().empty());
    EXPECT_EQ(result.result().success_code(), ::TaoTieChat::SuccessCode::SUCCESS);

    // 再次注册时会因为账号已存在而注册失败
    SignUp(channel, *request, notify);
    LOG_INFO(fmt::format("SignUp 2 test completed. Result: code={:#010x}, message={}, account_id={}", get_code(result), result.result().message(), result.account_id()));
    ASSERT_TRUE(!result.account_id().empty());
    EXPECT_EQ(result.result().error_code(), ::TaoTieChat::ErrorCode::USER_ID_EXISTS);
    // return EXIT_SUCCESS;
}
