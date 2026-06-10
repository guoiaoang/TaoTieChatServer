#include "headers.h"
#include "utils.h"

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
        Request buffer_header;
        stream->Read(&buffer_header);
        if(!buffer_header.has_header()) {
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_HEADER_REQUIRED, std::format("Missing header in FileUploadRequest"));
        }

        // 处理文件上传头信息
        const FileUploadHeader& header = buffer_header.header();
        response->mutable_request()->mutable_header()->CopyFrom(header);  // 将 header 信息返回给客户端，方便客户端进行后续处理
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

        // 断点续传可支持暂停继续，此时可将文件保存到固定路径，使用时间戳则需要查找记录
        // // 以时间戳创建文件夹进行写入，避免并发写入时文件名冲突
        // // string dir_name = Utils::to_local_string(std::chrono::system_clock::now(), "{:%Y%m%d-%H%M%S}");
        // // path file_path = Utils::upload_dir() / dir_name / file_md5;
        path file_path = Utils::upload_dir() / "tmp" / file_md5;
        int64_t existing_file_size = std::get<1>(Utils::file_size(file_path));
        existing_file_size = existing_file_size < 0 ? 0 : existing_file_size;  // -1 表示错误，0 表示不存在
        LOG_TRACE("file_path: {}, existing_file_size: {}", file_path.string(), existing_file_size);

        // 没有异常时需要告诉客户端可以继续上传
        response->set_file_status(file_status);  // 文件不存在，客户端可以继续上传
        response->set_uploaded_bytes(existing_file_size);  // 在临时目录中查找 // -1 表示错误，0 表示不存在
        success_status<Reply, Request>(stream, response.get(), SuccessCode::SUCCESS, std::format("Ready to receive file chunks for file_id: {}", file_id));
        // stream->Write(*response);

        using namespace Utils::Algorithm;
        // 后面的数据实际只包含文件碎片数据
        FileUploadRequest buffer_chunk;
        BytesMd5          bytes_md5;
        int64_t           bytes_written = existing_file_size;  // 实际写入的位置偏移，初始化为已存在的大小
        //
        Utils::mkdir(file_path.parent_path());  // 创建父目录
        std::fstream fs(file_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!fs) {
            std::ofstream(file_path, std::ios::binary).close();
            fs.open(file_path, std::ios::binary | std::ios::in | std::ios::out);
        }
        if(!fs) {
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_READ_ERROR, std::format("Failed to open file for file_id: {}", file_id));
        }
        // fs.seekp(bytes_written);  // 定位到需要写入的位置

        // 断点续传时只有第一块数据的 offset 可以跳跃，后续的数据必须紧跟着已经写入的数据
        bool is_first_chunk = true;
        while(stream->Read(&buffer_chunk)) {
            // 处理每条消息
            // const FileUploadRequest& req = buffer_chunk;
            if(!buffer_chunk.has_chunk()) {
                break;  // 没有 chunk 字段，结束上传
            }
            int64_t first_chunk_offset = buffer_chunk.chunk().chunk_offset();
            // 第一块数据时校验写入位置是否合法并计算已经存在数据的 md5
            if(is_first_chunk) {
                is_first_chunk = false;
                // 第一块数据的 offset 超出文件末尾，则文件上传不连续，此时告知客户端重新上传
                if(first_chunk_offset > existing_file_size) {
                    fs.close();
                    return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_WRITE_ERROR, std::format("File upload offset error for file_id: {}, please re-upload the file.", file_id));
                }
                // 校验本地已经存在文件的 md5
                if(first_chunk_offset > 0) {
                    // 更新文件状态为未完成上传，允许继续上传
                    auto [success_update_file_status, result_update_file_status] = db_sqlcipher.update_file_status_interface(file_id, FileStatusCode::FILE_STATUS_UPLOADING);
                    if(!success_update_file_status || !result_update_file_status) {
                        fs.close();
                        return error_status<Reply, Request>(stream, response.get(), ErrorCode::DB_EXECUTE, std::format("Failed to update file status to uploaded for file_id: {}", file_id));
                    }
                    // 计算已经存在数据的 md5
                    constexpr int64_t read_chunk_size = 128 * 1024;  // 128KB
                    char buffer[read_chunk_size] = {};
                    int64_t unread_bytes = first_chunk_offset;  // 如果不是在最后追加写入，那么不应该读取到文件末尾
                    int64_t read_total = 0; // 临时计数，避免修改 bytes_written
                    // 获取当前时间，如果间隔时间大于 5 秒则发送一次空数据以便客户端知道服务器还在处理，避免客户端超时
                    auto last_log_time = std::chrono::steady_clock::now();
                    while(fs.read(buffer, std::min(read_chunk_size, unread_bytes)) && unread_bytes > 0) {
                        int64_t bytes_read = fs.gcount();
                        if(bytes_read > 0) {
                            bytes_md5.update(buffer, bytes_read);
                            read_total += bytes_read;
                            unread_bytes  -= bytes_read;
                        }
                        // 每隔 20 秒记录一次日志，避免长时间没有日志输出导致客户端超时
                        auto current_time = std::chrono::steady_clock::now();
                        int64_t heartbeat_interval = 20;  // 20 秒发送一次心跳
                        if(std::chrono::duration_cast<std::chrono::seconds>(current_time - last_log_time).count() >= heartbeat_interval) {
                            last_log_time = current_time;
                            response->set_file_status(FileStatusCode::FILE_STATUS_UPLOADING);
                            success_status<Reply, Request>(stream, response.get(), SuccessCode::SUCCESS, std::format("File hash calculating for file_id: {}", file_id));
                        }
                    }
                }
                fs.seekp(bytes_written);  // 定位到需要写入的位置
                fs.clear();  // 清除 eofbit 和 failbit 状态，继续进行写入操作
            }
            const string& chunk_data = buffer_chunk.chunk().chunk_data();
            int64_t chunk_size = static_cast<int64_t>(chunk_data.size());
            int64_t chunk_offset = buffer_chunk.chunk().chunk_offset();
            // 校验 chunk 偏移，保证断点续传时数据连续
            if(chunk_offset != bytes_written) {
                fs.close();
                LOG_TRACE("chunk offset mismatch: expected {}, got {}", bytes_written, chunk_offset);
                return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_WRITE_ERROR, std::format("Chunk offset mismatch for file_id: {}, expected: {}, got: {}", file_id, bytes_written, chunk_offset));
            }
            fs.seekp(chunk_offset);  // 定位到当前写入位置
            fs.write(chunk_data.data(), chunk_data.size());
            // 写入失败时返回错误状态
            if(!fs) {
                // if(bytes_md5.get_hex_string() != file_md5) { Utils::remove(file_path); }  // 出错时要删除已经写入的文件，避免服务器磁盘被占满
                return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_WRITE_ERROR, std::format("Failed to write file chunk for file_id: {}", file_id));
            }
            bytes_written += chunk_size;
            // LOG_TRACE("Received chunk for file_id: {}, chunk_offset: {}, chunk_size: {}, bytes_written: {}", file_id, chunk_offset, chunk_size, bytes_written);
            // 文件与预期大小不符，可能是客户端上传过程中出现问题，直接返回错误状态
            if(bytes_written > file_size) {
                // if(bytes_md5.get_hex_string() != file_md5) { Utils::remove(file_path); }  // 出错时要删除已经写入的文件，避免服务器磁盘被占满
                Utils::remove(file_path);  // 出错时要删除已经写入的文件
                // 更新文件状态为不存在
                auto [success_update_file_status, result_update_file_status] = db_sqlcipher.update_file_status_interface(file_id, FileStatusCode::FILE_STATUS_NOT_FOUND);
                if(!success_update_file_status || !result_update_file_status) {
                    return error_status<Reply, Request>(stream, response.get(), ErrorCode::DB_EXECUTE, std::format("Failed to update file status to uploaded for file_id: {}", file_id));
                }
                return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_WRITE_ERROR, std::format("Written bytes exceed expected file size for file_id: {}", file_id));
            }
            // 更新 md5
            bytes_md5.update(chunk_data.data(), chunk_data.size());
            // 大小一致时认为接收完毕，退出循环
            if(bytes_written == file_size) {
                fs.flush();  // 确保所有数据已经写入到磁盘，否则会导致文件大小与实际接收到的大小不一致
                break;
            }
        }
        fs.close();  // 调用 close() 也可以触发 flush() 操作

        // 如果文件没有上传完成，那么就没必要进行后续的操作，设置为未上传完成即可
        if(bytes_written < file_size) {
            // 更新文件状态为未完成上传，允许继续上传  // 其实上传时就已经设置了这个状态
            auto [success_update_file_status, result_update_file_status] = db_sqlcipher.update_file_status_interface(file_id, FileStatusCode::FILE_STATUS_UPLOADING);
            if(!success_update_file_status || !result_update_file_status) {
                return error_status<Reply, Request>(stream, response.get(), ErrorCode::DB_EXECUTE, std::format("Failed to update file status to uploading for file_id: {}", file_id));
            }
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_WRITE_ERROR, std::format("File upload incomplete for file_id: {}, written bytes: {}, expected bytes: {}", file_id, bytes_written, file_size));
        }

        // 校验 md5
        string md5_calculated = bytes_md5.get_hex_string();
        if(md5_calculated != file_md5) {
            // 删除上传的文件
            Utils::remove(file_path);  // 出错时要删除已经写入的文件
            // 更新文件状态为不存在
            auto [success_update_file_status, result_update_file_status] = db_sqlcipher.update_file_status_interface(file_id, FileStatusCode::FILE_STATUS_NOT_FOUND);
            if(!success_update_file_status || !result_update_file_status) {
                return error_status<Reply, Request>(stream, response.get(), ErrorCode::DB_EXECUTE, std::format("Failed to update file status to uploaded for file_id: {}", file_id));
            }
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_MD5_MISMATCH, std::format("MD5 mismatch for file_id: {}, calculated md5: {}, expected md5: {}", file_id, md5_calculated, file_md5));
        }

        // 既然是依据 md5 存储的，就可以平级存储，也可以不存储文件路径
        auto storage_path = Utils::storage_dir() / file_path.filename();
        Utils::mkdir(storage_path.parent_path());
        // 将文件移动到存储目录
        if(!Utils::rename(file_path, storage_path)) {
            return error_status<Reply, Request>(stream, response.get(), ErrorCode::FILE_WRITE_ERROR, std::format("Failed to move file to storage for file_id: {}", file_id));
        }
        // // 重命名成功后删除上传目录
        // Utils::remove(file_path.parent_path());

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

    // FileDownload 下载文件。 // 获取用户头像时可能无身份但需要获取成功
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
        // 获取用户头像时可能无身份但需要获取成功
        if(0) {
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
        // 如果客户端发来的文件大小异常，那么就通知客户都重置位置
        int64_t file_offset = request->downloaded_bytes();
        file_offset = file_offset > file_size ? 0 : file_offset;
        // 4. 发送文件元数据给客户端，客户端可以根据这些信息准备接收文件内容
        header_reply->mutable_header()->mutable_file_metadata()->set_file_name(file_name  );
        header_reply->mutable_header()->mutable_file_metadata()->set_file_size(file_size  );
        header_reply->mutable_header()->mutable_file_metadata()->set_file_md5 (file_md5   );
        header_reply->mutable_header()->set_offset(file_offset);
        success_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), SuccessCode::SUCCESS, std::format("Get file info for file_id: {} succeeded.", file_id));

        // 既然是依据 md5 存储的，就可以平级存储，也可以不存储文件路径
        auto file_path = Utils::storage_dir() / file_md5;
        if(!Utils::exists(file_path)) {
            return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::FILE_NOT_FOUND, std::format("File not found for file_id: {}", file_id));
        }
        // 6. 发送文件内容
        std::ifstream ifs(file_path, std::ios::binary);
        if(!ifs) {
            return error_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), ErrorCode::FILE_READ_ERROR, std::format("Failed to open file for file_id: {}", file_id));
        }
        // 跳转发送位置
        ifs.seekg(file_offset);
        // 断点续传时根据客户端已接收的字节数定位到文件中对应的位置开始发送
        int64_t bytes_read_offset = file_offset;
        // 服务端一次发送数据过小会增加系统调用的开销，过大会导致内存占用过高。
        // 基于服务端内存、grpc 默认设置及网络设备支持的数据包默认大小，建议 64KB 到 1MB 之间，128KB 是一个相对合理的值。
        const size_t chunk_size = 128 * 1024;  // 128KB
        char buffer[chunk_size] = {};
        // 空文件或有有效数据都要发送至少一次
        // 读取到文件末尾时由 while 内的判断条件检测退出
        bool is_end_of_file = ifs.peek() == std::ifstream::traits_type::eof(); // 空文件也要进入循环
        while (ifs.read(buffer, chunk_size) || ifs.gcount() > 0 || is_end_of_file) {
            FileDownloadReply chunk_reply;
            chunk_reply.mutable_chunk()->set_chunk_data(buffer, ifs.gcount());
            chunk_reply.mutable_chunk()->set_chunk_offset(bytes_read_offset);
            response->Write(chunk_reply);
            bytes_read_offset += ifs.gcount();
            // 对于空文件，第一次读取就会返回 EOF，
            // 但我们仍然需要发送一个空的 chunk 来让客户端知道这是一个空文件。
            // 读取到文件末尾
            if(ifs.peek() == std::ifstream::traits_type::eof()) {
                break;
            }
        }
        ifs.close();
        return success_status<Reply, FileDownloadHeader>(response, header_reply.get(), header_reply->mutable_header(), SuccessCode::SUCCESS, std::format("File download for file_id: {} succeeded.", file_id));
    }
};

namespace Chat::Service::File::Rpc::AsyncCall {

// GetFileID 获取文件 ID。
class GetFileID : public AsyncUnaryCall<FileService::AsyncService, GetFileIDRequest, GetFileIDReply> {
public:
    // 构造函数：同时接收服务、完成队列和线程池
    GetFileID(FileService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncUnaryCall<FileService::AsyncService, GetFileIDRequest, GetFileIDReply>(service, cq, &FileService::AsyncService::RequestGetFileID), m_thread_pool(thread_pool) {
        // 不要捕获 this，直接显式捕获局部变量 thread_pool 确保线程安全
        SetBusinessHandler([thread_pool](auto* call, auto* context, const auto& request, auto* response) {
            // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
            thread_pool->enqueue([call, request]() {
                // 执行耗时的 DBSQLCipher 操作
                std::shared_ptr<GetFileIDReply> result = DBSQLCipher::instance().get_upload_file_id(std::make_shared<GetFileIDRequest>(std::move(request)));

                if (!result) {
                    call->Error(grpc::StatusCode::INTERNAL, "GetFileID Database Failed");
                } else {
                    call->Success(*result); // 模板内自动调用 Finish 销毁自己
                }
            });
        });
    }

protected:
    // 重写孵化虚函数，确保高并发时，基类自动 new 出来的下一个监听器依然是 GetFileIDCall 子类，而不是空壳基类
    void CloneNext(FileService::AsyncService* service, grpc::ServerCompletionQueue* cq) override {
        new GetFileID(service, cq, m_thread_pool);
    }

private:
    ThreadPool* m_thread_pool;
};

// FileUpload 上传文件。
class FileUpload : public AsyncBidiStreamingCall<FileService::AsyncService, FileUploadRequest, FileUploadReply> {
public:
    FileUpload(FileService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncBidiStreamingCall<FileService::AsyncService, FileUploadRequest, FileUploadReply>(
            service, cq, &FileService::AsyncService::RequestFileUpload), m_thread_pool(thread_pool)
    {
        SetOnConnectHandler([this](auto* call, auto* context) {
            Utils::log_grpc_context(context);
            m_is_header_processed = false;
            m_bytes_written = 0;
        });

        SetOnMsgHandler([this](auto* call, const FileUploadRequest& request) {
            // 1. 在主线程立刻深度拷贝当前的 request，防止它被 gRPC 引擎后续覆盖清空
            auto request_ptr = std::make_shared<FileUploadRequest>(request);

            // 2. 扔进线程池异步处理磁盘 I/O
            m_thread_pool->enqueue([this, call, request_ptr]() {
                FileUploadReply reply;

                // ---------------- 阶段 A：解析文件头 ----------------
                if (!m_is_header_processed) {
                    if (!request_ptr->has_header()) {
                        BuildAndPushError(call, ErrorCode::FILE_HEADER_REQUIRED, "Missing header");
                        return;
                    }

                    const auto& header = request_ptr->header();
                    reply.mutable_request()->mutable_header()->CopyFrom(header);
                    m_file_id = header.file_id();

                    DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
                    auto [success, file_metadata, file_status] = db_sqlcipher.get_file_info_interface(m_file_id);
                    if (!success) {
                        BuildAndPushError(call, ErrorCode::DB_QUERY, "Query db error");
                        return;
                    }

                    if (file_status == FileStatusCode::FILE_STATUS_UPLOADED) {
                        reply.set_file_status(FileStatusCode::FILE_STATUS_UPLOADED);
                        call->Push(reply);
                        return; // 秒传成功，不需要调 FetchNextMsg 读下一包了
                    }

                    m_file_path = Utils::upload_dir() / "tmp" / file_metadata.file_md5();
                    int64_t existing_size = std::get<1>(Utils::file_size(m_file_path));
                    m_bytes_written = existing_size < 0 ? 0 : existing_size;

                    Utils::mkdir(m_file_path.parent_path());

                    // 根据断点状态打开文件
                    if (m_bytes_written == 0) {
                        m_fs.open(m_file_path, std::ios::binary | std::ios::out | std::ios::trunc);
                    } else {
                        m_fs.open(m_file_path, std::ios::binary | std::ios::in | std::ios::out);
                        m_fs.seekp(m_bytes_written);
                    }

                    if (!m_fs.is_open()) {
                        BuildAndPushError(call, ErrorCode::FILE_WRITE_ERROR, "Cannot open disk file");
                        return;
                    }

                    m_is_header_processed = true;

                    reply.set_file_status(file_status);
                    reply.set_uploaded_bytes(m_bytes_written);
                    call->Push(reply);

                    // 本包（Header 包）在磁盘及业务层面安全处理完毕，驱动读取下一个客户端包
                    call->FetchNextMsg();
                    return;
                }

                // ---------------- 阶段 B：接收数据块 (Chunk) ----------------
                if (!request_ptr->has_chunk()) {
                    call->FetchNextMsg(); // 即使是空包，也放行去读下一包
                    return;
                }

                const auto& chunk = request_ptr->chunk();
                int64_t chunk_offset = chunk.chunk_offset();
                const std::string& chunk_data = chunk.chunk_data();

                if (chunk_offset != m_bytes_written) {
                    BuildAndPushError(call, ErrorCode::FILE_WRITE_ERROR, "Offset mismatch");
                    return;
                }

                if (!chunk_data.empty()) {
                    m_fs.write(chunk_data.data(), chunk_data.size());
                    m_fs.flush(); // 强行刷新缓冲区入盘

                    if (!m_fs) {
                        BuildAndPushError(call, ErrorCode::FILE_WRITE_ERROR, "Write disk failed");
                        return;
                    }
                    m_bytes_written += chunk_data.size();
                }

                reply.set_uploaded_bytes(m_bytes_written);
                reply.set_file_status(FileStatusCode::FILE_STATUS_UPLOADING);
                call->Push(reply);

                // 磁盘写入落盘完全成功，通知底层安全接收下一个分块数据包！
                call->FetchNextMsg();
            });
        });
    }

    ~FileUpload() {
        if (m_fs.is_open()) {
            m_fs.flush();
            m_fs.close();
        }
    }

private:
    void BuildAndPushError(AsyncBidiStreamingCall<FileService::AsyncService, FileUploadRequest, FileUploadReply>* call, ErrorCode code, const std::string& msg) {
        FileUploadReply reply;
        Utils::error_status(&reply, code, msg);
        call->Push(reply);
        if (m_fs.is_open()) m_fs.close();
    }

private:
    ThreadPool* m_thread_pool;
    bool m_is_header_processed = false;
    std::string m_file_id;
    std::filesystem::path m_file_path;
    std::ofstream m_fs;
    int64_t m_bytes_written = 0;
};

// FileDownload 下载文件 (Server Streaming: 一问多答)
class FileDownload : public AsyncServerStreamingCall<FileService::AsyncService, FileDownloadRequest, FileDownloadReply> {
public:
    FileDownload(FileService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncServerStreamingCall<FileService::AsyncService, FileDownloadRequest, FileDownloadReply>(service, cq, &FileService::AsyncService::RequestFileDownload), m_thread_pool(thread_pool)
    {
        // 1. 流启动时触发：初始化文件并发送第一块
        SetOnStartHandler([this](auto* call, auto* context, const auto& request) {
            m_thread_pool->enqueue([this, call, request]() {
                auto dlCtx = std::make_shared<DownloadContext>();

                std::string file_id = request.file_id();
                // 调用数据库获取文件 MD5/真实路径（示例逻辑，根据你的业务匹配）
                DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
                auto [success, file_metadata, file_status] = db_sqlcipher.get_file_info_interface(file_id);

                if (!success) {
                    call->Finish(grpc::Status(grpc::StatusCode::NOT_FOUND, "Get file metadata failed"));
                    return;
                }

                std::filesystem::path file_path = Utils::upload_dir() / "tmp" / file_metadata.file_md5();
                dlCtx->file.open(file_path, std::ios::binary | std::ios::in);
                if (!dlCtx->file.is_open()) {
                    call->Finish(grpc::Status(grpc::StatusCode::NOT_FOUND, "File bytes not found on disk"));
                    return;
                }

                this->m_dlCtx = dlCtx;
                // 发送首个块
                SendNextChunk(call);
            });
        });

        // 2. 前一个块写完网络（网卡闲置）触发：续读发下一块，实现平滑流控
        SetOnWriteDoneHandler([this](auto* call) {
            m_thread_pool->enqueue([this, call]() {
                if (m_dlCtx && m_dlCtx->file.good()) {
                    SendNextChunk(call);
                } else {
                    call->Finish(grpc::Status::OK); // 文件读完，优雅结束
                }
            });
        });
    }

private:
    struct DownloadContext {
        std::ifstream file;
        int64_t offset = 0;
    };

    void SendNextChunk(AsyncServerStreamingCall<FileService::AsyncService, FileDownloadRequest, FileDownloadReply>* call) {
        constexpr size_t CHUNK_SIZE = 64 * 1024; // 64KB
        char buffer[CHUNK_SIZE];

        m_dlCtx->file.read(buffer, CHUNK_SIZE);
        std::streamsize bytes_read = m_dlCtx->file.gcount();

        if (bytes_read > 0) {
            FileDownloadReply reply;
            reply.mutable_chunk()->set_chunk_data(buffer, bytes_read);
            reply.mutable_chunk()->set_chunk_offset(m_dlCtx->offset);

            m_dlCtx->offset += bytes_read;
            call->Write(reply); // 写入网络
        } else {
            call->Finish(grpc::Status::OK); // 宣告流结束
        }
    }

private:
    ThreadPool* m_thread_pool;
    std::shared_ptr<DownloadContext> m_dlCtx;
};

};  // namespace Chat::Service::File::Rpc::AsyncCall
