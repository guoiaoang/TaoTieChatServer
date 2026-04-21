#include "headers.h"
#include "utils.h"

using std::string;
using std::map;
using std::set;

using Utils::success_status;
using Utils::error_status;
using Utils::log_grpc_context;

// class Service : public ::grpc::Service {
// public:
//     Service();
//     virtual ~Service();
//     // 客户端流式上传
//     // FileUpload 上传文件。
//     virtual ::grpc::Status FileUpload(::grpc::ServerContext* context, ::grpc::ServerReader< ::TaoTieChat::FileContent>* reader, ::TaoTieChat::FileReply* response);
//     // FileDownload 下载文件。
//     virtual ::grpc::Status FileDownload(::grpc::ServerContext* context, const ::TaoTieChat::FileRequest* request, ::grpc::ServerWriter< ::TaoTieChat::FileContent>* writer);
// };

using ::grpc::Status;
using ::grpc::ServerContext;
using ::grpc::ServerReader;
using ::TaoTieChat::FileContent;
using ::TaoTieChat::FileReply;
using ::TaoTieChat::FileRequest;
using ::grpc::ServerWriter;

// 服务端实现
class FileServiceImpl final : public FileService::Service {
public:
    // FileUpload 上传文件。
    Status FileUpload(ServerContext* context, ServerReader<FileContent>* request, FileReply* response) override {
        using Request = FileContent;
        using Reply   = FileReply;
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
        // set_success(response, SuccessCode::SUCCESS);
        // response->CopyFrom(*result);
        return Status::OK;
    }

    // FileDownload 下载文件。
    Status FileDownload(ServerContext* context, const FileRequest* request, ServerWriter<FileContent>* response) override {
        using Request = FileRequest;
        using Reply   = FileContent;
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
        // set_success(response, SuccessCode::SUCCESS);
        // response->CopyFrom(*result);
        return Status::OK;
    }
};
