// std
#include <condition_variable>
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

namespace {
void PrintReply(const TaoTieChat::StatusReply& reply, const std::string& prefix) {
    std::string request_info = absl::StrFormat("request(id=%s, token=%s)", reply.request().id(), reply.request().token());
    std::string reply_info;

    // success 信息
    if (reply.has_success()) {
        reply_info += "success(";
        const auto& success = reply.success();
        if (success.has_online_status()) {
            reply_info += absl::StrFormat("online_status=%d", success.online_status());
        } else if (success.has_success_code()) {
            reply_info += absl::StrFormat("success_code=%d", success.success_code());
        } else if (success.has_other_code()) {
            reply_info += absl::StrFormat("other_code=%d", success.other_code());
        } else {
            reply_info += "code=<unset>";
        }
        reply_info += "), ";
        reply_info += absl::StrFormat("message=%s", success.message());
        // ABSL_LOG(INFO) << prefix << " " << request_info << " " << reply_info;
    }

    // error 信息
    else if (reply.has_error()) {
        reply_info += "error(";
        const auto& error = reply.error();
        if (error.has_error_code()) {
            reply_info += absl::StrFormat("error_code=%d", error.error_code());
        } else if (error.has_other_code()) {
            reply_info += absl::StrFormat("other_code=%d", error.other_code());
        } else {
            reply_info += "code=<unset>";
        }
        reply_info += "), ";
        reply_info += absl::StrFormat("message=%s", error.message());
        // ABSL_LOG(INFO) << prefix << " " << request_info << " " << reply_info;
    }

    // 无 success 和 error 字段时输出提示
    else {
        reply_info += "no success or error field set";
    }

    ABSL_LOG(WARNING) << prefix << request_info << ", reply(" << reply_info << ")";
}
}

namespace ASYNC_AFTER_GET_STATUS {
    // 新式异步回调 API (Callback API)，官方推荐

    using std::shared_ptr;
    using std::unique_ptr;
    using grpc::Channel;
    using grpc::ClientBidiReactor;
    using TaoTieChat::StatusRequest;
    using TaoTieChat::StatusReply;
    using TaoTieChat::StatusChecker;
    using Stub = TaoTieChat::StatusChecker::Stub;

    // 继承 ClientBidiReactor
    class StatusReactor : public ClientBidiReactor<StatusRequest, StatusReply> {
    public:
        explicit StatusReactor(unique_ptr<Stub> stub) : m_stub(std::move(stub)) {
            m_status_request.set_id("[callback]");
            m_status_request.set_token("ASYNC_AFTER_GET_STATUS::AsyncGetStatus");

            m_stub->async()->GetStatus(&m_client_context, this);
            StartCall();
            StartRead(&m_status_reply);
            StartWrite(&m_status_request);
        }

        void OnReadDone(bool ok) override {
            if (!ok) {
                return;
            }

            PrintReply(m_status_reply, "[callback]");
            StartRead(&m_status_reply);
        }

        void OnWriteDone(bool ok) override {
            if (ok) {
                StartWritesDone();
            }
        }

        void OnDone(const grpc::Status& s) override {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_done = true;
                m_status = s;
            }
            m_cv.notify_one();
        }

        void Wait() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_done; });

            if (!m_status.ok()) {
                ABSL_LOG(INFO) << "[callback] rpc failed: " << m_status.error_code() << " - " << m_status.error_message();
            }
        }

    private:
        unique_ptr<Stub> m_stub;
        grpc::ClientContext m_client_context;
        StatusRequest m_status_request;
        StatusReply   m_status_reply;

        std::mutex m_mutex;
        std::condition_variable m_cv;
        bool m_done{false};
        grpc::Status m_status;
    };

    void AsyncGetStatus(shared_ptr<Channel> channel) {
        unique_ptr<Stub> stub = StatusChecker::NewStub(channel);
        StatusReactor status_reactor(std::move(stub));
        status_reactor.Wait();
    }
}

namespace DIRECT_ASYNC_GET_STATUS {
    // 传统异步 API (Asynchronous API)，需要手动开启线程维护 CompletionQueue
    void AsyncGetStatus(std::shared_ptr<grpc::Channel> channel) {
        enum class EventTag {
            START = 1,
            WRITE,
            WRITES_DONE,
            READ,
            FINISH,
        };

        auto MakeTag = [](EventTag tag) -> void* {
            return reinterpret_cast<void*>(static_cast<intptr_t>(tag));
        };

        using std::unique_ptr;
        using grpc::ClientAsyncReaderWriter;
        using grpc::ClientContext;
        using grpc::Status;
        using grpc::CompletionQueue;
        using TaoTieChat::StatusRequest;
        using TaoTieChat::StatusReply;
        using TaoTieChat::StatusChecker;
        using Stub = TaoTieChat::StatusChecker::Stub;
        // using std::shared_ptr;
        // using grpc::ClientAsyncReaderWriterInterface;
        // using grpc::ClientAsyncReaderInterface;
        // using grpc::ClientAsyncWriterInterface;
        // using grpc::StatusCode;
        // using Stream = ClientAsyncReaderWriterInterface<StatusRequest, StatusReply>;
        // using Reader = ClientAsyncReaderInterface<StatusReply>;
        // using Writer = ClientAsyncWriterInterface<StatusRequest>;

        ClientContext   client_context  ;
        CompletionQueue completion_queue;
        Status          status          ;
        StatusReply     status_reply    ;
        StatusRequest   status_request  ;
        status_request.set_id("[async-cq]");
        status_request.set_token("DIRECT_ASYNC_GET_STATUS::AsyncGetStatus");

        unique_ptr<Stub> stub = StatusChecker::NewStub(channel);
        unique_ptr<ClientAsyncReaderWriter<StatusRequest, StatusReply>> stream =
            stub->AsyncGetStatus(&client_context, &completion_queue, MakeTag(EventTag::START));

        void* tag = nullptr;
        bool ok = false;

        if (!completion_queue.Next(&tag, &ok) || !ok || tag != MakeTag(EventTag::START)) {
            ABSL_LOG(INFO) << "[async-cq] failed to start stream";
            completion_queue.Shutdown();
            return;
        }

        stream->Write(status_request, MakeTag(EventTag::WRITE));
        if (!completion_queue.Next(&tag, &ok) || !ok || tag != MakeTag(EventTag::WRITE)) {
            ABSL_LOG(INFO) << "[async-cq] write failed";
            completion_queue.Shutdown();
            return;
        }

        stream->WritesDone(MakeTag(EventTag::WRITES_DONE));
        if (!completion_queue.Next(&tag, &ok) || !ok || tag != MakeTag(EventTag::WRITES_DONE)) {
            ABSL_LOG(INFO) << "[async-cq] writes done failed";
            completion_queue.Shutdown();
            return;
        }

        stream->Read(&status_reply, MakeTag(EventTag::READ));
        while (completion_queue.Next(&tag, &ok) && tag == MakeTag(EventTag::READ)) {
            if (!ok) {
                break;
            }

            PrintReply(status_reply, "[async-cq]");
            stream->Read(&status_reply, MakeTag(EventTag::READ));
        }

        stream->Finish(&status, MakeTag(EventTag::FINISH));
        if (completion_queue.Next(&tag, &ok) && ok && tag == MakeTag(EventTag::FINISH)) {
            if (!status.ok()) {
                ABSL_LOG(INFO) << "[async-cq] rpc failed: " << status.error_code() << " - " << status.error_message();
            }
        } else {
            ABSL_LOG(INFO) << "[async-cq] finish failed";
        }

        completion_queue.Shutdown();
    }
}

namespace DIRECT_GET_STATUS {
    // 传统同步 API (Synchronous API)
    void GetStatus(std::shared_ptr<grpc::Channel> channel) {
        using std::unique_ptr;
        using grpc::ClientContext;
        using TaoTieChat::StatusRequest;
        using TaoTieChat::StatusReply;
        using TaoTieChat::StatusChecker;
        using Stub = TaoTieChat::StatusChecker::Stub;
        using grpc::ClientReaderWriter;
        // using grpc::Status;
        // using TaoTieChatServer::StatusChecker;

        ClientContext client_context;
        StatusRequest status_request;
        StatusReply   status_reply  ;
        status_request.set_id("[sync]");
        status_request.set_token("DIRECT_GET_STATUS::GetStatus");
        unique_ptr<Stub> stub = StatusChecker::NewStub(channel);
        unique_ptr<ClientReaderWriter<StatusRequest, StatusReply>> stream = stub->GetStatus(&client_context);
        stream->Write(status_request);
        stream->WritesDone();
        while (stream->Read(&status_reply)) {
            PrintReply(status_reply, "[sync]");
        }

        grpc::Status status = stream->Finish();
        if (!status.ok()) {
            ABSL_LOG(INFO) << "[sync] rpc failed: " << status.error_code() << " - " << status.error_message();
        } else {
            ABSL_LOG(INFO) << "[sync] rpc finished successfully";
        }
    }
}

int main(int argc, char** argv) {
    // using std::unique_ptr;
    using std::shared_ptr;
    using std::string;
    using grpc::Channel;

    absl::ParseCommandLine(argc, argv);
	absl::InitializeLog();
	absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);
	absl::SetStderrThreshold(absl::LogSeverityAtLeast::kError);

    string target_str = absl::GetFlag(FLAGS_target);
    shared_ptr<Channel> channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());

    // 新式异步回调 API (Callback API)，官方推荐
    ABSL_LOG(INFO) << "Testing ASYNC_AFTER_GET_STATUS::AsyncGetStatus...";
    ASYNC_AFTER_GET_STATUS::AsyncGetStatus(channel);
    // 传统异步 API (Asynchronous API)，需要手动开启线程维护 CompletionQueue
    ABSL_LOG(INFO) << "Testing DIRECT_ASYNC_GET_STATUS::AsyncGetStatus...";
    DIRECT_ASYNC_GET_STATUS::AsyncGetStatus(channel);
    // 同步 API (Synchronous API)
    ABSL_LOG(INFO) << "Testing DIRECT_GET_STATUS::GetStatus...";
    DIRECT_GET_STATUS::GetStatus(channel);

    return EXIT_SUCCESS;
}
