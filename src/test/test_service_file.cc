#include "test_headers.h"

class ServiceFile : public ::testing::Test {
protected:
    const path db_path = Utils::app_dir() / (get_current_file_name(__FILE__) + ".db");
    const string encryption_key = "your_secure_encryption_key";

    // 每个 TEST_F 运行前都会执行
    void SetUp() override {
        // 移除数据目录
        path home_dir = Utils::home_dir();
        if(exists(home_dir)) {
            try {
                remove_all(home_dir);
                std::cout << std::format("home directory {} removed successfully.", home_dir.string()) << std::endl;
            } catch (const filesystem_error& e) {
                std::cerr << std::format("Failed to remove home directory {}: {}", home_dir.string(), e.what()) << std::endl;
            }
        } else {
            std::cerr << std::format("Home directory {} does not exist, no need to remove.", home_dir.string()) << std::endl;
        }

        // `ASSERT_FALSE(false);` 会中断测试并标记为失败，而 `EXPECT_FALSE(false);` 会记录失败但继续执行测试。
        std::cout << std::format("database file: {}", db_path.string()) << std::endl;

        // 所有操作都必须成功，否则测试无法继续进行

        // 删除旧的数据库文件，确保每次测试都从干净的状态开始
        if(Utils::exists(db_path)) {
            try {
                // remove 返回 bool 值：成功删除返回 true，文件不存在返回 false
                auto [removed, existed] = Utils::remove(db_path);
                if (existed) {
                    std::cout << std::format("file {} removed successfully.", db_path.string()) << std::endl;
                } else {
                    std::cerr << std::format("file {} does not exist, no need to remove.", db_path.string()) << std::endl;
                }
            } catch (const filesystem_error& e) {
                // 如果文件正在被占用或权限不足，会抛出异常
                std::cerr << std::format("remove {} failed: {}", db_path.string(), e.what()) << std::endl;
            }
            // remove(db_path);
        }
        ASSERT_FALSE(Utils::exists(db_path)) << std::format("Failed to remove old database file: {}", db_path.string());

        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        db_sqlcipher.set_init_params(db_path, encryption_key);
        ASSERT_FALSE(!db_sqlcipher.database_open      ()) << std::format("Failed to open database with encryption key.") << std::endl;
        ASSERT_FALSE(!db_sqlcipher.database_initialize()) << std::format("Failed to initialize database schema."       ) << std::endl;
    }

    // 每个 TEST_F 运行后都会执行
    void TearDown() override {
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        db_sqlcipher.database_close();
    }
};

// 部分测试需要有已经注册的用户
std::tuple<Status, SignUpReply> SignUp(
    const string& user_id          ,
    const string& password         ,
    const string& confirm          ,
    const string& security_question,
    const string& security_answer
) {
    using Service = AccountServiceImpl;
    using Request = SignUpRequest     ;
    using Reply   = SignUpReply       ;
    using Context = ServerContext     ;
    // ======================================================================= //
    // 创建 AccountServiceImpl 实例
    Service service;

    // 构造测试请求
    Request request;
    if(user_id          .empty()) { request.clear_user_id          (); } else { request.set_user_id          (user_id          ); }
    if(password         .empty()) { request.clear_password         (); } else { request.set_password         (password         ); }
    if(confirm          .empty()) { request.clear_confirm          (); } else { request.set_confirm          (confirm          ); }
    if(security_question.empty()) { request.clear_security_question(); } else { request.set_security_question(security_question); }
    if(security_answer  .empty()) { request.clear_security_answer  (); } else { request.set_security_answer  (security_answer  ); }

    // 构造响应对象
    Reply response;

    // 调用 SignUp 方法
    Context context;  // 模拟 gRPC 上下文
    Status  status = service.SignUp(&context, &request, &response);
    return {status, response};
}

// 1. 获取文件上传 ID
std::tuple<Status, GetFileIDReply> GetFileID(
    const string& user_id  , const string & password ,
    const string& file_name,       int64_t  file_size, const string& file_md5
) {
    using Service = FileServiceImpl ;
    using Request = GetFileIDRequest;
    using Reply   = GetFileIDReply  ;
    using Context = ServerContext   ;
    // ======================================================================= //
    Service service;

    Request request;
    request.mutable_request_user_auth()->set_user_id  (user_id  );
    request.mutable_request_user_auth()->set_password (password );
    request.mutable_file_metadata    ()->set_file_name(file_name);
    request.mutable_file_metadata    ()->set_file_size(file_size);
    request.mutable_file_metadata    ()->set_file_md5 (file_md5 );

    Reply   response;
    Context context ;
    Status  status = service.GetFileID(&context, &request, &response);
    return {status, response};
}

// 2. 双向流式的上传文件建议使用 gRPC 的集成测试框架进行测试，单元测试中无法模拟流式上传的场景
std::tuple<grpc::Status, std::unique_ptr<FileUploadReply>, bool> FileUpload(const string& file_id, const string& file_path) {
    using Builder = ServerBuilder    ;
    using Service = FileServiceImpl  ;
    using Context = ClientContext    ;
    using Request = FileUploadRequest;
    using Reply   = FileUploadReply  ;
    // ======================================================================= //
    // =========== 准备服务器
    // 创建服务器并在内存中启动
    auto builder = std::make_unique<Builder>();
    auto service = std::make_unique<Service>();
    builder->RegisterService(service.get());
    auto server  = builder->BuildAndStart   ();
    auto channel = server ->InProcessChannel(grpc::ChannelArguments());
    auto stub    = FileService::NewStub(channel);
    auto context = std::make_unique<Context>();
    auto request = std::make_unique<Request>();
    auto reply   = std::make_unique<Reply  >();
    // =========== 开始测试代码
    // 由于 FileUpload 是一个双向流式 RPC，无法直接调用服务端方法进行测试，需要通过客户端 Stub 来模拟上传过程
    auto stream  = stub->FileUpload(context.get());
    request->mutable_header()->set_file_id(file_id       );
    request->mutable_header()->set_reason ("value_reason");

    // 写文件元数据，模拟上传文件的第一步
    stream->Write(*request);

    // 读取服务端响应
    stream->Read(reply.get());

    // 获取文件状态，根据状态决定是否继续上传
    auto file_status = reply->file_status();

    // 判断是否是秒传
    bool is_fast_upload = file_status == FileStatusCode::FILE_STATUS_UPLOADED ? true : false;

    if(file_status == FileStatusCode::FILE_STATUS_NOT_FOUND || file_status == FileStatusCode::FILE_STATUS_UPLOADING) {
        std::cout << "File does not exist on server, need to upload." << std::endl;

        // 获取已经上传文件的大小
        int64_t uploaded_bytes = reply->uploaded_bytes();  // 已上传文件的大小，可用于断点续传
        int64_t file_size = std::get<1>(Utils::file_size(file_path));
        // 如果服务器返回的已上传字节数超过了实际文件大小，
        // 说明服务器可能没有正确处理断点续传的逻辑，
        // 此时重置为 0 以进行重新上传。
        if(uploaded_bytes > file_size) {
            uploaded_bytes = 0;
        }

        // 继续上传文件内容，模拟上传文件的后续步骤
        const size_t chunk_size = 128 * 1024; // 每次上传 128KB
        std::vector<char> buffer(chunk_size);
        std::ifstream ifs(file_path, std::ios::binary);
        if(uploaded_bytes >= 0) {
            ifs.seekg(uploaded_bytes);
        }
        // 空文件或有有效数据都要发送至少一次
        // 读取到文件末尾时由 while 内的判断条件检测退出
        bool is_end_of_file = ifs.peek() == std::ifstream::traits_type::eof(); // 空文件也要进入循环
        while (ifs.read(buffer.data(), buffer.size()) || ifs.gcount() > 0 || is_end_of_file) {
            Request chunk_request;
            chunk_request.mutable_chunk()->set_chunk_data  (buffer.data(), ifs.gcount());
            chunk_request.mutable_chunk()->set_chunk_offset(uploaded_bytes             );
            stream->Write(chunk_request);
            uploaded_bytes += ifs.gcount();
            // 对于空文件，第一次读取就会返回 EOF，
            // 但我们仍然需要发送一个空的 chunk 来让服务器知道这是一个空文件。
            // 读取到文件末尾
            if(ifs.peek() == std::ifstream::traits_type::eof()) {
                break;
            }
        }

        // 文件上传完成后，服务端会返回最终的文件状态，客户端需要读取这个状态来确认上传结果
        stream->Read(reply.get());
        file_status = reply->file_status();
    }
    // 成功与异常时都不执行其他操作
    // else if(file_status == FileStatusCode::FILE_STATUS_UPLOADED) {
    //     std::cout << "File already exists on server, fast upload success." << std::endl;
    // }
    // else {
    //     std::cout << "File does not exist on server, need to upload." << std::endl;
    //     // return;
    // }

    // 3. 客户端由于秒传成功，停止发送并关闭
    stream->WritesDone();
    Status status = stream->Finish();

    // =========== 关闭服务器
    server->Shutdown();
    return {status, std::move(reply), is_fast_upload};
}

// 3. 下载文件
std::tuple<grpc::Status, std::unique_ptr<FileDownloadReply>> FileDownload(const string& user_id, const string& password, const string& file_id) {
    using Builder = ServerBuilder      ;
    using Service = FileServiceImpl    ;
    using Context = ClientContext      ;
    using Request = FileDownloadRequest;
    using Reply   = FileDownloadReply  ;
    // ======================================================================= //
    // =========== 准备服务器
    // 创建服务器并在内存中启动
    auto builder = std::make_unique<Builder>();
    auto service = std::make_unique<Service>();
    builder->RegisterService(service.get());
    auto server  = builder->BuildAndStart();
    auto channel = server->InProcessChannel(grpc::ChannelArguments());
    auto stub    = FileService::NewStub(channel);
    auto context = std::make_unique<Context>();
    auto request = std::make_unique<Request>();
    auto reply   = std::make_unique<Reply  >();
    // =========== 开始测试代码
    // 由于 FileDownload 是一个服务器流式 RPC，无法直接调用服务端方法进行测试，需要通过客户端 Stub 来模拟下载过程
    request->mutable_request_user_auth()->set_user_id (user_id );
    request->mutable_request_user_auth()->set_password(password);
    request->set_file_id(file_id);
    request->set_downloaded_bytes(0);  // 从文件开头开始下载

    auto stream  = stub->FileDownload(context.get(), *request);

    // 先读取头获取身份验证结果和文件信息
    bool read_header = stream->Read(reply.get());
    if(!read_header) {
        std::cout << "Failed to receive file download header." << std::endl;
        return {Status(StatusCode::UNKNOWN, "Failed to receive file download header"), std::move(reply)};
    }
    // 获取身份验证结果，判断是否有权限下载文件
    auto header = reply->header();
    if(!header.result().has_success_code()) {
        std::cout << "Authentication failed for file download." << std::endl;
        return {Status(StatusCode::UNAUTHENTICATED, "Authentication failed for file download"), std::move(reply)};
    }
    // 返回成功码即认为有读取权限
    auto file_name = header.file_metadata().file_name();
    auto file_size = header.file_metadata().file_size();
    auto file_md5  = header.file_metadata().file_md5 ();
    // 保存文件到本地
    path download_path = Utils::downloaded_dir() / file_name;
    Utils::mkdir(download_path.parent_path());
    std::ofstream ofs(download_path, std::ios::binary);
    // 文件下载是从头开始下的，且为本地通知客户端进行跳转偏移，客户端不需要自己跳转
    while (stream->Read(reply.get())) {
        auto&chunk_data = reply->chunk().chunk_data();
        ofs.write(chunk_data.data(), chunk_data.size());
    }
    ofs.close();
    Status status = stream->Finish();

    // =========== 关闭服务器
    server->Shutdown();

    // =========== 校验文件大小与 md5
    auto [file_size_success, file_size_actual] = Utils::file_size(download_path);
    if(!file_size_success || file_size_actual != file_size) {
        std::cout << "File size mismatch." << std::endl;
        return {Status(StatusCode::UNKNOWN, "File size mismatch"), std::move(reply)};
    }
    using namespace Utils::Algorithm;
    auto file_md5_calculated = calculate_file_md5(download_path);
    if(file_md5_calculated != file_md5) {
        std::cout << "File md5 mismatch." << std::endl;
        return {Status(StatusCode::UNKNOWN, "File md5 mismatch"), std::move(reply)};
    }

    return {status, std::move(reply)};
}

// 成功上传及下载空文件
TEST_F(ServiceFile, UploadAndDownloadEmptyFile) {
    // 注册一个用户，确保后续操作有一个有效的账号
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    using namespace Utils;
    using namespace Utils::Algorithm;
    using namespace std::filesystem;

    // 创建一个空文件，测试是否支持空文件的上传
    path empty_file_path = Utils::temp_dir() / "test_file.txt";
    Utils::mkdir(empty_file_path.parent_path());
    Utils::remove(empty_file_path);  // 确保文件不存在
    std::ofstream ofs(empty_file_path);
    ofs.close();

    // 获取当前文件的基础信息
    auto exe_path           = empty_file_path; // path(__FILE__); // app_path();
    auto [exe_file_size_success, exe_file_size] = Utils::file_size(exe_path);
    auto exe_md5_calculated = calculate_file_md5(exe_path);
    auto exe_file_name      = exe_path.filename().string();
    std::cout << std::format("Executable file name: {}, size: {}, md5: {}", exe_file_name, exe_file_size, exe_md5_calculated) << std::endl;

    // 获取文件上传 ID，确保服务端正确处理了请求并返回了一个有效的 file_id
    auto [status, response] = GetFileID("value_user_id", "value_password", exe_file_name, exe_file_size, exe_md5_calculated);
    EXPECT_TRUE(status.ok());
    response.result().has_success_code();
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);
    auto file_id = response.file_id();
    EXPECT_TRUE(!file_id.empty());  // 成功应该返回一个非空的 file_id
    std::cout << std::format("Received file_id: {}", file_id) << std::endl;
    auto file_status = response.file_status();
    EXPECT_EQ(file_status, FileStatusCode::FILE_STATUS_NOT_FOUND);  // 文件不存在，应该返回 FILE_STATUS_NOT_FOUND

    // 上传文件
    auto [status_upload, response_upload, is_fast_upload] = FileUpload(file_id, exe_path.string());
    EXPECT_TRUE(status_upload.ok());
    EXPECT_EQ(response_upload->file_status(), FileStatusCode::FILE_STATUS_UPLOADED);  // 上传完成应该返回 FILE_STATUS_UPLOADED
    EXPECT_FALSE(is_fast_upload);  // 由于之前没有上传过这个文件，应该不是秒传

    // 下载文件
    auto [status_download, response_download] = FileDownload("value_user_id", "value_password", file_id);
    EXPECT_TRUE(status_download.ok());
    EXPECT_TRUE(response_download->header().result().has_success_code());
    EXPECT_EQ(response_download->header().result().success_code(), SuccessCode::SUCCESS);
    auto file_name_downloaded = response_download->header().file_metadata().file_name();
    auto file_size_downloaded = response_download->header().file_metadata().file_size();
    auto file_md5_downloaded  = response_download->header().file_metadata().file_md5 ();
    EXPECT_EQ(file_name_downloaded, path(exe_path).filename().string());
    EXPECT_EQ(file_size_downloaded, exe_file_size);
    EXPECT_EQ(file_md5_downloaded , exe_md5_calculated);
    // 校验下载文件的文件大小和 md5
    path download_path = Utils::downloaded_dir() / file_name_downloaded;
    EXPECT_TRUE(Utils::exists(download_path));
    EXPECT_EQ(std::get<1>(Utils::file_size(download_path)), exe_file_size);
    auto file_md5_calculated = Utils::Algorithm::calculate_file_md5(download_path);
    EXPECT_EQ(file_md5_calculated, exe_md5_calculated);
}

// 成功上传及下载非空文件  // 及秒传
TEST_F(ServiceFile, UploadAndDownloadNotEmptyFile) {
    // 注册一个用户，确保后续操作有一个有效的账号
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    using namespace Utils;
    using namespace Utils::Algorithm;
    using namespace std::filesystem;

    // // 创建一个空文件，测试是否支持空文件的上传
    // path empty_file_path = Utils::temp_dir() / "test_file.txt";
    // Utils::mkdir(empty_file_path.parent_path());
    // std::ofstream ofs(empty_file_path);
    // ofs.close();

    // 获取当前文件的基础信息
    auto exe_path           = app_path(); // empty_file_path; // path(__FILE__); // app_path();
    auto [exe_file_size_success, exe_file_size] = Utils::file_size(exe_path);
    auto exe_md5_calculated = calculate_file_md5(exe_path);
    auto exe_file_name      = exe_path.filename().string();
    std::cout << std::format("Executable file name: {}, size: {}, md5: {}", exe_file_name, exe_file_size, exe_md5_calculated) << std::endl;

    // 获取文件上传 ID，确保服务端正确处理了请求并返回了一个有效的 file_id
    auto [status, response] = GetFileID("value_user_id", "value_password", exe_file_name, exe_file_size, exe_md5_calculated);
    EXPECT_TRUE(status.ok());
    response.result().has_success_code();
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);
    auto file_id = response.file_id();
    EXPECT_TRUE(!file_id.empty());  // 成功应该返回一个非空的 file_id
    std::cout << std::format("Received file_id: {}", file_id) << std::endl;
    auto file_status = response.file_status();
    EXPECT_EQ(file_status, FileStatusCode::FILE_STATUS_NOT_FOUND);  // 文件不存在，应该返回 FILE_STATUS_NOT_FOUND

    // 上传文件
    auto [status_upload, response_upload, not_fast_upload] = FileUpload(file_id, exe_path.string());
    EXPECT_TRUE(status_upload.ok());
    EXPECT_EQ(response_upload->file_status(), FileStatusCode::FILE_STATUS_UPLOADED);  // 上传完成应该返回 FILE_STATUS_UPLOADED
    EXPECT_FALSE(not_fast_upload);  // 由于之前没有上传过这个文件，应该不是秒传

    // 再次上传文件应该为秒传
    auto [status_fast_upload, response_fast_upload, is_fast_upload] = FileUpload(file_id, exe_path.string());
    EXPECT_TRUE(status_fast_upload.ok());
    EXPECT_EQ(response_fast_upload->file_status(), FileStatusCode::FILE_STATUS_UPLOADED);  // 上传完成应该返回 FILE_STATUS_UPLOADED
    EXPECT_TRUE(is_fast_upload);  // 由于之前已经上传过这个文件，应该是秒传

    // 下载文件
    auto [status_download, response_download] = FileDownload("value_user_id", "value_password", file_id);
    EXPECT_TRUE(status_download.ok());
    EXPECT_TRUE(response_download->header().result().has_success_code());
    EXPECT_EQ(response_download->header().result().success_code(), SuccessCode::SUCCESS);
    auto file_name_downloaded = response_download->header().file_metadata().file_name();
    auto file_size_downloaded = response_download->header().file_metadata().file_size();
    auto file_md5_downloaded  = response_download->header().file_metadata().file_md5 ();
    EXPECT_EQ(file_name_downloaded, path(exe_path).filename().string());
    EXPECT_EQ(file_size_downloaded, exe_file_size);
    EXPECT_EQ(file_md5_downloaded , exe_md5_calculated);
    // 校验下载文件的文件大小和 md5
    path download_path = Utils::downloaded_dir() / file_name_downloaded;
    EXPECT_TRUE(Utils::exists(download_path));
    EXPECT_EQ(std::get<1>(Utils::file_size(download_path)), exe_file_size);
    auto file_md5_calculated = Utils::Algorithm::calculate_file_md5(download_path);
    EXPECT_EQ(file_md5_calculated, exe_md5_calculated);
}
