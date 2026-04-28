#include "headers.h"
#include "utils.h"

using std::string;
using std::map;
using std::set;

using ::grpc::Server;
using ::grpc::ServerBuilder;
using ::grpc::ServerContext;
using ::grpc::Status;
using ::grpc::StatusCode;
using ::grpc::ServerWriter;
using ::grpc::ServerSplitStreamer;

// ph.h
using ::grpc::ByteBuffer;
using ::grpc::CallbackServerContext;
using ::grpc::ChannelInterface;
using ::grpc::ClientAsyncReader;
using ::grpc::ClientAsyncReaderInterface;
using ::grpc::ClientAsyncResponseReader;
using ::grpc::ClientAsyncResponseReaderInterface;
using ::grpc::ClientAsyncWriter;
using ::grpc::ClientAsyncWriterInterface;
using ::grpc::ClientContext;
using ::grpc::ClientReader;
using ::grpc::ClientReaderInterface;
using ::grpc::ClientReadReactor;
using ::grpc::ClientUnaryReactor;
using ::grpc::ClientWriter;
using ::grpc::ClientWriteReactor;
using ::grpc::ClientWriterInterface;
using ::grpc::CompletionQueue;
using ::grpc::internal::CallbackClientStreamingHandler;
using ::grpc::internal::CallbackServerStreamingHandler;
using ::grpc::internal::CallbackUnaryHandler;
using ::grpc::internal::RpcMethod;
using ::grpc::internal::SplitServerStreamingHandler;
using ::grpc::internal::StreamedUnaryHandler;
using ::grpc::MessageAllocator;
using ::grpc::ServerCompletionQueue;
using ::grpc::ServerContext;
using ::grpc::ServerReader;
using ::grpc::ServerReadReactor;
using ::grpc::ServerSplitStreamer;
using ::grpc::ServerUnaryReactor;
using ::grpc::ServerUnaryStreamer;
using ::grpc::ServerWriter;
using ::grpc::ServerWriteReactor;
using ::grpc::Service;
// using ::grpc::Service::GetHandler;
// using ::grpc::Service::MarkMethodAsync;
// using ::grpc::Service::MarkMethodCallback;
// using ::grpc::Service::MarkMethodGeneric;
// using ::grpc::Service::MarkMethodRaw;
// using ::grpc::Service::MarkMethodRawCallback;
// using ::grpc::Service::MarkMethodStreamed;
// using ::grpc::Service::RequestAsyncClientStreaming;
// using ::grpc::Service::RequestAsyncServerStreaming;
// using ::grpc::Service::RequestAsyncUnary;
using ::grpc::Status;
// using ::grpc::StatusCode::UNIMPLEMENTED;
using ::grpc::StubOptions;
//

using ::TaoTieChat::SuccessCode;
using ::TaoTieChat::ErrorCode;
using ::TaoTieChat::MessageService;
using ::TaoTieChat::MessageContext;
using ::TaoTieChat::MessageReply;
using ::TaoTieChat::MessageRequest;

using Utils::success_status;
using Utils::error_status;
using Utils::log_grpc_context;

// 服务端实现
class MessageServiceImpl final : public MessageService::Service {
public:
    // SendMessage 发送消息。
    Status SendMessage(ServerContext* context, const MessageContext* request, MessageReply* response) override {
        using Request = MessageContext;
        using Reply   = MessageReply;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        // 执行注册
        shared_ptr<Reply> result = db_sqlcipher.write_message(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("Write message failed."));
        }
        set_success(response->mutable_result(), SuccessCode::SUCCESS);
        response->CopyFrom(*result);
        return Status::OK;
    }
    // ReceiveMessages 接收消息。
    Status ReceiveMessages(ServerContext* context, const MessageRequest* request, ServerWriter<MessageContext>* response) override {
        using Request = MessageRequest;
        using Reply   = MessageContext;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        // response->mutable_request()->CopyFrom(*request);  // 复制请求
        // DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        // shared_ptr<Request> params = make_shared<Request>(*request);
        // // =========================== 执行操作 =========================== //
        // // 执行注册
        // shared_ptr<Reply> result = db_sqlcipher.get_status(params);
        // // 判断结果是否为 nullptr，防止拷贝时崩溃
        // if(!result) {
        //     return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("GetStatus failed."));
        // }
        // response->CopyFrom(*result);
        return Status::OK;
    }
};
