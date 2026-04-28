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
using ::grpc::ServerWriter;

// 服务端实现
class FileServiceImpl final : public FileService::Service {
public:
    // GetFileID 获取文件 ID。
    Status GetFileID(ServerContext* context, const GetFileIDRequest* request, GetFileIDReply* response) override {
        using Request = GetFileIDRequest;
        using Reply   = GetFileIDReply;
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
        shared_ptr<Reply> result = db_sqlcipher.get_file_id(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("GetFileID failed."));
        }
        response->CopyFrom(*result);
        return Status::OK;
    }

    // FileUpload 上传文件。
    Status FileUpload(ServerContext* context, ServerReader<FileUploadRequest>* request, FileUploadReply* response) override {
        using Request = FileUploadRequest;
        using Reply   = FileUploadReply;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }

        // 第一个字段必须是 header，包含鉴权和文件元数据
        FileUploadRequest buffer_header;
        request->Read(&buffer_header);
        if(!buffer_header.has_header()) {
            return Status(StatusCode::INTERNAL, std::format("Missing header in FileUploadRequest"));
        }
        // 处理文件上传头信息
        const FileUploadHeader& header = buffer_header.header();
        response->mutable_request()->mutable_header()->CopyFrom(header);

        // =========================== 执行操作 =========================== //

        // 1. 如果文件已经上传完成，则直接返回成功状态，避免重复上传
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<GetFileIDRequest> get_file_id_request = make_shared<GetFileIDRequest>();
        get_file_id_request->mutable_request_user_auth()->CopyFrom(header.request_user_auth());
        get_file_id_request->mutable_file_metadata    ()->CopyFrom(header.file_metadata    ());
        shared_ptr<GetFileIDReply> get_file_id_reply = db_sqlcipher.get_file_id(get_file_id_request);
        if(get_file_id_reply && get_file_id_reply->result().success_code() == SuccessCode::SUCCESS) {
            // 文件已存在，直接返回成功状态
            response->set_file_id(get_file_id_reply->file_id());
            // return success_status<Reply>(response, std::format("File already exists, file_id: {}", get_file_id_reply->file_id()));
            return Status::OK;
        }

        // 2. 上传完 metadata 后获取文件内容是否已存在

        // 先记录文件的 metadata
        auto result_write_file_id = db_sqlcipher.write_file_id(get_file_id_request);
        if(result_write_file_id && result_write_file_id->result().success_code() == SuccessCode::SUCCESS) {
            // 文件内容已存在，直接返回成功状态
            response->set_file_id(result_write_file_id->file_id());
            return Status::OK;
        }

        // 检查 md5 是否已存在，存在则认为文件内容已存在，无需上传文件内容


        // if(get_file_id_reply && get_file_id_reply->result().success_code() == SuccessCode::FILE_METADATA_EXISTS) {
        //     // 文件内容已存在，直接关联 metadata 和文件 ID，无需上传文件内容
        //     response->set_file_id(get_file_id_reply->file_id());
        //     // return success_status<Reply>(response, std::format("File metadata already exists, file_id: {}", get_file_id_reply->file_id()));
        //     return Status::OK;
        // }

        // 后面的数据实际只包含文件碎片数据
        FileUploadRequest buffer_chunk;
        while(request->Read(&buffer_chunk)) {
            // 处理每条消息
            const FileUploadRequest& req = buffer_chunk;
            if(req.has_chunk()) {
                auto&chunk_data = req.chunk().chunk_data();  // 获取文件碎片数据
            }
        }


        // response->mutable_request()->CopyFrom(*request);  // 复制请求
        // if(request->has_header()) {
        //     response->mutable_request()->mutable_header()->CopyFrom(request->header());
        // }
        // DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        // shared_ptr<Request> params = make_shared<Request>(*request);
        // // =========================== 执行操作 =========================== //
        // // 执行注册
        // shared_ptr<Reply> result = db_sqlcipher.file_upload(params);
        // // 判断结果是否为 nullptr，防止拷贝时崩溃
        // if(!result) {
        //     return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("FileUpload failed."));
        // }
        // response->CopyFrom(*result);
        return Status::OK;
    }

    // FileDownload 下载文件。
    Status FileDownload(ServerContext* context, const FileDownloadRequest* request, ServerWriter<FileDownloadReply>* response) override {
        using Request = FileDownloadRequest;
        using Reply   = FileDownloadReply;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }

        FileDownloadReply header_reply;
        header_reply.mutable_header()->mutable_request()->CopyFrom(*request);  // 复制请求
        response->Write(header_reply);  // 先发送包含请求信息的头部消息

        // response->mutable_request()->CopyFrom(*request);  // 复制请求
        // DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        // shared_ptr<Request> params = make_shared<Request>(*request);
        // // =========================== 执行操作 =========================== //
        // // 执行注册
        // shared_ptr<Reply> result = db_sqlcipher.file_download(params);
        // // 判断结果是否为 nullptr，防止拷贝时崩溃
        // if(!result) {
        //     return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("FileDownload failed."));
        // }
        // response->CopyFrom(*result);
        return Status::OK;
    }
};
