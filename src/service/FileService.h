#include "headers.h"
#include "utils.h"

using std::string;
using std::map;
using std::set;

using Utils::success_status;
using Utils::error_status;
using Utils::log_grpc_context;

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
        shared_ptr<Reply> result = db_sqlcipher.get_upload_file_id(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("GetFileID failed."));
        }
        response->CopyFrom(*result);
        return Status::OK;
    }

    // virtual ::grpc::Status FileUpload(ServerContext* context, ServerReaderWriter<FileUploadReply, FileUploadRequest>* stream);
    // FileUpload 上传文件。
    Status FileUpload(ServerContext* context, ServerReaderWriter<FileUploadReply, FileUploadRequest>* stream) override {
        using Request = FileUploadRequest;
        using Reply   = FileUploadReply;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !stream) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }

        // 创建响应对象，后续根据处理结果设置响应内容
        shared_ptr<Reply> response = make_shared<Reply>();

        // 第一个字段必须是 header，用于获取文件的 ID
        // 不进行身份验证，谁都可以进行上传
        FileUploadRequest buffer_header;
        stream->Read(&buffer_header);
        if(!buffer_header.has_header()) {
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_HEADER_REQUIRED, std::format("Missing header in FileUploadRequest"));
        }

        // 处理文件上传头信息
        const FileUploadHeader& header = buffer_header.header();
        // response->mutable_request()->mutable_header()->CopyFrom(header);
        string file_id = header.file_id();

        // =========================== 参数校验 =========================== //
        if(file_id.empty()) {
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_ID_REQUIRED, std::format("Missing file_id in FileUploadHeader"));
        }

        // =========================== 执行操作 =========================== //

        // 如果文件已经上传完成，则直接返回成功状态，避免重复上传
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        auto [success_get_file_info, file_metadata, file_status] = db_sqlcipher.get_file_info_interface(file_id);
        if(!success_get_file_info) {
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::DB_QUERY, std::format("Get file info for file_id: {} failed.", file_id));
        }
        string  file_name = file_metadata.file_name();
        int64_t file_size = file_metadata.file_size();
        string  file_md5  = file_metadata.file_md5 ();
        if(file_name.empty() || file_size < 0 || file_md5.empty()) {
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::DB_QUERY, std::format("Get file info for file_id: {} failed.", file_id));
        }
        // 文件已存在，直接返回成功状态
        if(file_status == FileStatusCode::FILE_STATUS_UPLOADED) {
            response->set_file_status(FileStatusCode::FILE_STATUS_UPLOADED);
            return success_status<Reply, Request>(stream, response.get(), SuccessCode::SUCCESS, std::format("File already uploaded for file_id: {}", file_id));
        }

        // 文件处于非上传完成的状态时均可上传  // 包括：文件不存在、文件正在上传、文件上传失败等状态
        // 写文件并校验 md5，成功后更新文件状态为已上传
        // 获取文件内容是否已存在

        // 没有异常时需要告诉客户端可以继续上传
        // response->set_file_status(FileStatusCode::FILE_STATUS_NOT_FOUND);  // 文件不存在，客户端可以继续上传
        response->set_file_status(file_status);  // 文件不存在，客户端可以继续上传
        stream->Write(*response);

        using std::filesystem::path;

        // 以时间戳创建文件夹进行写入，避免并发写入时文件名冲突
        string dir_name = Utils::to_local_string(std::chrono::system_clock::now(), "{:%Y%m%d-%H%M%S}");
        path file_path = Utils::upload_dir() / dir_name / file_md5;
        std::cout << "file_path: " << file_path << std::endl;

        Utils::mkdir(file_path.parent_path());  // 创建父目录
        std::ofstream ofs(file_path, std::ios::binary);
        if(!ofs) {
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_WRITE, std::format("Failed to create file for file_id: {}", file_id));
        }
        using namespace Utils::Algorithm;
        // 后面的数据实际只包含文件碎片数据
        FileUploadRequest buffer_chunk;
        BytesMd5          bytes_md5;
        int64_t           bytes_written = 0;
        while(stream->Read(&buffer_chunk)) {
            // 处理每条消息
            const FileUploadRequest& req = buffer_chunk;
            if(req.has_chunk()) {
                auto&chunk_data = req.chunk().chunk_data();  // 获取文件碎片数据
                ofs.write(chunk_data.data(), chunk_data.size());
                bytes_written += chunk_data.size();
                // 写入失败时要删除已经写入的文件，避免服务器磁盘被占满
                if(!ofs) {
                    if(bytes_md5.get_hex_string() != file_md5) { Utils::remove(file_path); }  // 出错时要删除已经写入的文件，避免服务器磁盘被占满
                    return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_WRITE, std::format("Failed to write file chunk for file_id: {}", file_id));
                }
                // 文件与预期大小不符，可能是客户端上传过程中出现问题，直接返回错误状态
                if(bytes_written > file_size) {
                    if(bytes_md5.get_hex_string() != file_md5) { Utils::remove(file_path); }  // 出错时要删除已经写入的文件，避免服务器磁盘被占满
                    return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_WRITE, std::format("Written bytes exceed expected file size for file_id: {}", file_id));
                }
                // 更新 md5
                bytes_md5.update(chunk_data.data(), chunk_data.size());
                // 大小一致时认为接收完毕，退出循环
                if(bytes_written == file_size) {
                    ofs.flush();  // 确保所有数据已经写入到磁盘，否则会导致文件大小与实际接收到的大小不一致
                    break;
                }
            }
        }
        ofs.close();  // 调用 close() 也可以触发 flush() 操作

        // 校验 md5
        string md5_calculated = bytes_md5.get_hex_string();
        if(md5_calculated != file_md5) {
            // 删除上传的文件
            Utils::remove(file_path);  // 出错时要删除已经写入的文件，避免服务器磁盘被占满
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_MD5_MISMATCH, std::format("MD5 mismatch for file_id: {}, calculated md5: {}, expected md5: {}", file_id, md5_calculated, file_md5));
        }

        // 既然是依据 md5 存储的，就可以平级存储，也可以不存储文件路径
        auto storage_path = Utils::storage_dir() / file_path.filename();
        Utils::mkdir(storage_path.parent_path());
        // 将文件移动到存储目录
        if(!Utils::rename(file_path, storage_path)) {
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_WRITE, std::format("Failed to move file to storage for file_id: {}", file_id));
        }
        // 重命名成功后删除上传目录
        Utils::remove(file_path.parent_path());

        // // 记录文件路径
        // auto [success_record_file_path, result_record_file_path] = db_sqlcipher.record_file_path_interface(file_md5, file_path.string());
        // if(!success_record_file_path || !result_record_file_path) {
        //     // 删除上传的文件
        //     Utils::remove(file_path);
        //     return error_status<Reply, Request>(stream, response.get(), ErrorCode::DB_EXECUTE, std::format("Failed to record file path for file_id: {}", file_id));
        // }

        // 更新文件状态为已上传
        auto [success_update_file_status, result_update_file_status] = db_sqlcipher.update_file_status_interface(file_id, FileStatusCode::FILE_STATUS_UPLOADED);
        if(!success_update_file_status || !result_update_file_status) {
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::DB_EXECUTE, std::format("Failed to update file status to uploaded for file_id: {}", file_id));
        }

        // 告诉客户端上传成功
        response->set_file_status(FileStatusCode::FILE_STATUS_UPLOADED);  // 写数据到客户端的执行在下面的 return 的语句中会被执行
        return success_status<Reply, Request>(stream, response.get(), SuccessCode::SUCCESS, std::format("File uploaded successfully for file_id: {}", file_id));
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
        shared_ptr<FileDownloadReply> header_reply = make_shared<FileDownloadReply>();
        header_reply->mutable_header()->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 参数校验 =========================== //
        string file_id = params->file_id();
        if(file_id.empty()) {
            return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::FILE_ID_REQUIRED, std::format("Missing file_id in FileDownloadRequest"));
        }
        const AuthRequest& request_user_auth = params->request_user_auth();
        // =========================== 执行操作 =========================== //
        // 1. 身份合法验证
        shared_ptr<AuthReply> auth_reply = db_sqlcipher.auth_check(std::make_shared<AuthRequest>(request_user_auth));
        if(!auth_reply || !auth_reply->result().has_success_code()) {
            return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::AUTH_FAILED,
                std::format("Authentication failed: {}", auth_reply ? auth_reply->result().message() : "Unknown error"));
        }
        // 2. 文件读取权限验证
        string account_id = auth_reply->account_id();
        auto [success_check_permission, file_permission] = db_sqlcipher.get_file_permission_interface(account_id, file_id);
        if(!success_check_permission) {
            return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::DB_CHECK, std::format("Failed to check file read permission for account_id: {}, file_id: {}", account_id, file_id));
        }
        if(!(file_permission & Utils::Transform::to_int(PermissionCode::PERMISSION_READ))) {
            return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::PERMISSION_DENIED, std::format("No read permission for account_id: {}, file_id: {}", account_id, file_id));
        }
        // 3. 获取文件元数据
        auto [success_get_file_info, file_metadata, file_status] = db_sqlcipher.get_file_info_interface(file_id);
        if(!success_get_file_info) {
            return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::DB_QUERY, std::format("Get file info for file_id: {} failed.", file_id));
        }
        string  file_name = file_metadata.file_name();
        int64_t file_size = file_metadata.file_size();
        string  file_md5  = file_metadata.file_md5 ();
        if(file_name.empty() || file_size < 0 || file_md5.empty()) {
            return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::DB_QUERY, std::format("Get file info for file_id: {} failed.", file_id));
        }
        // 4. 发送文件元数据给客户端，客户端可以根据这些信息准备接收文件内容
        header_reply->mutable_header()->mutable_file_metadata()->set_file_name(file_name);
        header_reply->mutable_header()->mutable_file_metadata()->set_file_size(file_size);
        header_reply->mutable_header()->mutable_file_metadata()->set_file_md5 (file_md5 );
        success_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), SuccessCode::SUCCESS, std::format("Get file info for file_id: {} succeeded.", file_id));

        // // 5. 获取文件路径
        // auto [success_get_file_path, file_path] = db_sqlcipher.get_file_path_interface(file_md5);
        // if(!success_get_file_path) {
        //     return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::DB_QUERY, std::format("Get file path for file_id: {} failed.", file_id));
        // }

        // 既然是依据 md5 存储的，就可以平级存储，也可以不存储文件路径
        auto file_path = Utils::storage_dir() / file_md5;
        if(!Utils::exists(file_path)) {
            return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::FILE_NOT_FOUND, std::format("File not found for file_id: {}", file_id));
        }
        // 6. 发送文件内容
        std::ifstream ifs(file_path, std::ios::binary);
        if(!ifs) {
            return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::FILE_READER_ERROR, std::format("Failed to open file for file_id: {}", file_id));
        }
        // 服务端一次发送数据过小会增加系统调用的开销，过大会导致内存占用过高。
        // 基于服务端内存、grpc 默认设置及网络设备支持的数据包默认大小，建议 64KB 到 1MB 之间，128KB 是一个相对合理的值。
        const size_t buffer_size = 128 * 1024;  // 128KB
        char buffer[buffer_size] = {};
        int64_t bytes_sent = 0;
        while(ifs) {
            bool is_empty_file = ifs.peek() == std::ifstream::traits_type::eof();
            ifs.read(buffer, buffer_size);
            std::streamsize bytes_read = ifs.gcount();
            if(bytes_read > 0 || is_empty_file) {
                bytes_sent += bytes_read;
                // 防止服务器错误发送数据
                if(bytes_sent > file_size) {
                    return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::FILE_READER_ERROR, std::format("Read more bytes than expected for file_id: {}", file_id));
                }
                FileDownloadReply chunk_reply;
                chunk_reply.mutable_chunk()->set_chunk_data(buffer, bytes_read);
                response->Write(chunk_reply);
            }
            // 读取到文件末尾
            // 空文件也要发送一次数据，因此该判断不能放在循环开始的位置，
            // 否则会导致客户端跳过步骤一直等待数据，最终导致超时
            if(ifs.peek() == std::ifstream::traits_type::eof()) {
                break;
            }
        }
        ifs.close();
        return success_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), SuccessCode::SUCCESS, std::format("File download for file_id: {} succeeded.", file_id));
    }
};
