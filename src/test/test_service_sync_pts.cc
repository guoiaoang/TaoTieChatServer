#include "test_headers.h"

class ServiceSync : public ::testing::Test {
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
        ASSERT_FALSE(!db_sqlcipher.database_open()) << std::format("Failed to open database with encryption key.") << std::endl;
        ASSERT_FALSE(!db_sqlcipher.database_initialize()) << std::format("Failed to initialize database schema.") << std::endl;
    }

    // 每个 TEST_F 运行后都会执行
    void TearDown() override {
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        db_sqlcipher.database_close();
    }
};

// 部分测试需要有已经注册的用户
std::tuple<grpc::Status, SignUpReply> SignUp(
    const string& user_id          ,
    const string& password         ,
    const string& confirm          ,
    const string& security_question,
    const string& security_answer
) {
    // 创建 AccountServiceImpl 实例
    AccountServiceImpl service;

    // 构造测试请求
    SignUpRequest request;
    if(user_id          .empty()) { request.clear_user_id          (); } else { request.set_user_id          (user_id          ); }
    if(password         .empty()) { request.clear_password         (); } else { request.set_password         (password         ); }
    if(confirm          .empty()) { request.clear_confirm          (); } else { request.set_confirm          (confirm          ); }
    if(security_question.empty()) { request.clear_security_question(); } else { request.set_security_question(security_question); }
    if(security_answer  .empty()) { request.clear_security_answer  (); } else { request.set_security_answer  (security_answer  ); }

    // 构造响应对象
    SignUpReply response;

    // 调用 SignUp 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.SignUp(&context, &request, &response);
    return {status, response};
}

// 1. 获取文件上传 ID
std::tuple<grpc::Status, GetFileIDReply> GetFileID(
    const string& user_id, const string& password,
    const string& file_name, int64_t file_size, const string& file_md5
) {
    FileServiceImpl service;

    GetFileIDRequest request;
    request.mutable_request_user_auth()->set_user_id(user_id);
    request.mutable_request_user_auth()->set_password(password);
    request.mutable_file_metadata()->set_file_name(file_name);
    request.mutable_file_metadata()->set_file_size(file_size);
    request.mutable_file_metadata()->set_file_md5 (file_md5 );

    GetFileIDReply response;
    grpc::ServerContext context;
    grpc::Status status = service.GetFileID(&context, &request, &response);
    return {status, response};
}

// 2. 双向流式的上传文件建议使用 gRPC 的集成测试框架进行测试，单元测试中无法模拟流式上传的场景
std::tuple<grpc::Status, std::unique_ptr<FileUploadReply>, bool> FileUpload(const string& file_id, const string& file_path) {
    using Request = FileUploadRequest;
    using Reply   = FileUploadReply;
    // =========== 准备服务器
    // 创建服务器并在内存中启动
    auto builder = std::make_unique<ServerBuilder  >();;
    auto service = std::make_unique<FileServiceImpl>();;
    builder->RegisterService(service.get());
    auto server  = builder->BuildAndStart();
    auto channel = server->InProcessChannel(grpc::ChannelArguments());
    auto stub    = FileService::NewStub(channel);
    auto context = std::make_unique<ClientContext>();
    auto request = std::make_unique<Request      >();
    auto reply   = std::make_unique<Reply        >();
    // =========== 开始测试代码
    // 由于 FileUpload 是一个双向流式 RPC，无法直接调用服务端方法进行测试，需要通过客户端 Stub 来模拟上传过程
    auto stream  = stub->FileUpload(context.get());
    request->mutable_header()->set_file_id(file_id);
    request->mutable_header()->set_reason("value_reason");

    // 写文件元数据，模拟上传文件的第一步
    stream->Write(*request);

    // 读取服务端响应
    stream->Read(reply.get());

    // 获取文件状态，根据状态决定是否继续上传
    auto file_status = reply->file_status();

    // 判断是否是秒传
    bool is_fast_upload = file_status == FileStatusCode::FILE_STATUS_UPLOADED ? true : false;

    if(file_status == FileStatusCode::FILE_STATUS_NOT_FOUND) {
        std::cout << "File does not exist on server, need to upload." << std::endl;

        // 继续上传文件内容，模拟上传文件的后续步骤
        const size_t chunk_size = 128 * 1024; // 每次上传 128KB
        std::vector<char> buffer(chunk_size);
        std::ifstream ifs(file_path, std::ios::binary);
        // 空文件特殊处理
        bool is_empty_file = ifs.peek() == std::ifstream::traits_type::eof();
        while (ifs.read(buffer.data(), buffer.size()) || ifs.gcount() > 0 || is_empty_file) {
            FileUploadRequest chunk_request;
            chunk_request.mutable_chunk()->set_chunk_data(buffer.data(), ifs.gcount());
            stream->Write(chunk_request);
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
    using Request = FileDownloadRequest;
    using Reply   = FileDownloadReply;
    // =========== 准备服务器
    // 创建服务器并在内存中启动
    auto builder = std::make_unique<ServerBuilder  >();;
    auto service = std::make_unique<FileServiceImpl>();;
    builder->RegisterService(service.get());
    auto server  = builder->BuildAndStart();
    auto channel = server->InProcessChannel(grpc::ChannelArguments());
    auto stub    = FileService::NewStub(channel);
    auto context = std::make_unique<ClientContext>();
    auto request = std::make_unique<Request      >();
    auto reply   = std::make_unique<Reply        >();
    // =========== 开始测试代码
    // 由于 FileDownload 是一个服务器流式 RPC，无法直接调用服务端方法进行测试，需要通过客户端 Stub 来模拟下载过程
    request->mutable_request_user_auth()->set_user_id (user_id );
    request->mutable_request_user_auth()->set_password(password);
    request->set_file_id(file_id);

    auto stream  = stub->FileDownload(context.get(), *request);

    // 先读取头获取身份验证结果和文件信息
    bool read_header = stream->Read(reply.get());
    if(!read_header) {
        std::cout << "Failed to receive file download header." << std::endl;
        return {Status(grpc::StatusCode::UNKNOWN, "Failed to receive file download header"), std::move(reply)};
    }
    // 获取身份验证结果，判断是否有权限下载文件
    auto header = reply->header();
    if(!header.result().has_success_code()) {
        std::cout << "Authentication failed for file download." << std::endl;
        return {Status(grpc::StatusCode::UNAUTHENTICATED, "Authentication failed for file download"), std::move(reply)};
    }
    // 返回成功码即认为有读取权限
    auto file_name = header.file_metadata().file_name();
    auto file_size = header.file_metadata().file_size();
    auto file_md5  = header.file_metadata().file_md5 ();
    // 保存文件到本地
    path download_path = Utils::downloaded_dir() / file_name;
    Utils::mkdir(download_path.parent_path());
    std::ofstream ofs(download_path, std::ios::binary);
    while (stream->Read(reply.get())) {
        auto&chunk_data = reply->chunk().chunk_data();
        ofs.write(chunk_data.data(), chunk_data.size());
    }
    ofs.close();
    Status status = stream->Finish();
    EXPECT_TRUE(status.ok());

    // =========== 关闭服务器
    server->Shutdown();

    // =========== 校验文件大小与 md5
    auto [file_size_success, file_size_actual] = Utils::file_size(download_path);
    if(!file_size_success || file_size_actual != file_size) {
        std::cout << "File size mismatch." << std::endl;
        return {Status(grpc::StatusCode::UNKNOWN, "File size mismatch"), std::move(reply)};
    }
    using namespace Utils::Algorithm;
    auto file_md5_calculated = calculate_file_md5(download_path);
    if(file_md5_calculated != file_md5) {
        std::cout << "File md5 mismatch." << std::endl;
        return {Status(grpc::StatusCode::UNKNOWN, "File md5 mismatch"), std::move(reply)};
    }

    return {status, std::move(reply)};
}

// 成功上传文件
TEST_F(ServiceSync, UploadAndDownloadNotEmptyFile) {
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

    // 获取当前文件的基础信息
    auto exe_path           = app_path();
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
}

// ========================= PTS 辅助函数 ========================= //

// 获取账号各类型最新 PTS 值
std::tuple<grpc::Status, GetLastIDReply> GetLastID(
    const string& user_id, const string& password
) {
    SyncServiceImpl service;
    AuthRequest request;
    request.set_user_id (user_id );
    request.set_password(password);
    GetLastIDReply response;
    grpc::ServerContext context;
    grpc::Status status = service.GetLastID(&context, &request, &response);
    return {status, response};
}

// 发送好友申请
std::tuple<grpc::Status, ContactRequestReply> ContactRequest(
    const string& user_id, const string& password,
    const string& target_user_id, const string& message = ""
) {
    AccountServiceImpl service;
    ContactRequestRequest request;
    request.mutable_request_user_auth()->set_user_id (user_id );
    request.mutable_request_user_auth()->set_password(password);
    request.set_target_user_id(target_user_id);
    if(!message.empty()) { request.set_message(message); }
    ContactRequestReply response;
    grpc::ServerContext context;
    grpc::Status status = service.ContactRequest(&context, &request, &response);
    return {status, response};
}

// 回复好友申请
std::tuple<grpc::Status, ContactReplyReply> ContactReply(
    const string& user_id, const string& password,
    const string& target_user_id, ContactReplyCode reply_code
) {
    AccountServiceImpl service;
    ContactReplyRequest request;
    request.mutable_request_user_auth()->set_user_id (user_id );
    request.mutable_request_user_auth()->set_password(password);
    request.set_target_user_id(target_user_id);
    request.set_reply(reply_code);
    ContactReplyReply response;
    grpc::ServerContext context;
    grpc::Status status = service.ContactReply(&context, &request, &response);
    return {status, response};
}

// 发送消息
std::tuple<grpc::Status, MessageReply> SendMessage(
    const string& user_id, const string& password,
    const string& target_user_id, const string& text
) {
    MessageServiceImpl service;
    MessageContext request;
    request.mutable_request_user_auth()->set_user_id (user_id );
    request.mutable_request_user_auth()->set_password(password);
    request.set_user_id(target_user_id);
    request.set_short_text(text);
    request.set_client_message_id(Utils::generate_message_id());
    MessageReply response;
    grpc::ServerContext context;
    grpc::Status status = service.SendMessage(&context, &request, &response);
    return {status, response};
}

// 获取指定类型的 pts 值，未找到则返回 -1
int64_t GetPtsValue(const GetLastIDReply& reply, SyncType type) {
    for(int i = 0; i < reply.tags_size(); ++i) {
        if(reply.tags(i).type() == type) {
            return reply.tags(i).id();
        }
    }
    return -1;
}

// 通过 in-process gRPC 调用 Sync，返回服务器流中的所有回复（header + bodies）
std::tuple<grpc::Status, std::vector<SyncReply>> SyncPull(const SyncRequest& request) {
    auto builder = std::make_unique<ServerBuilder>();
    auto service = std::make_unique<SyncServiceImpl>();
    builder->RegisterService(service.get());
    auto server  = builder->BuildAndStart();
    auto channel = server->InProcessChannel(grpc::ChannelArguments());
    auto stub    = SyncService::NewStub(channel);
    auto context = std::make_unique<ClientContext>();

    std::vector<SyncReply> replies;
    auto stream = stub->Sync(context.get());
    stream->Write(request);
    stream->WritesDone();

    SyncReply reply;
    while(stream->Read(&reply)) {
        replies.push_back(reply);
    }
    grpc::Status status = stream->Finish();
    server->Shutdown();
    return {status, replies};
}

// ========================= PTS 测试用例 ========================= //

// 测试：发送好友申请后目标用户的 CONTACTS pts 被更新
TEST_F(ServiceSync, PTS_ContactRequest_UpdatesTargetContactsPts) {
    // 注册两个用户
    auto [s1, r1] = SignUp("user_alice", "pass_alice", "pass_alice", "q1", "a1");
    ASSERT_EQ(r1.result().success_code(), SuccessCode::SUCCESS);

    auto [s2, r2] = SignUp("user_bob", "pass_bob", "pass_bob", "q2", "a2");
    ASSERT_EQ(r2.result().success_code(), SuccessCode::SUCCESS);

    // alice 发送好友申请给 bob 前，先检查 bob 的初始 pts（应为 -1 即无记录）
    auto [sl0, rl0] = GetLastID("user_bob", "pass_bob");
    EXPECT_TRUE(sl0.ok());
    int64_t bob_contacts_pts_before = GetPtsValue(rl0, SyncType::CONTACTS);
    EXPECT_EQ(bob_contacts_pts_before, -1) << "好友申请前 bob 的 CONTACTS pts 应该不存在";

    // alice 发好友申请给 bob
    auto [s3, r3] = ContactRequest("user_alice", "pass_alice", "user_bob", "hi bob");
    ASSERT_TRUE(s3.ok());
    ASSERT_EQ(r3.result().success_code(), SuccessCode::SUCCESS);
    std::cout << std::format("ContactRequest result: {}", r3.result().message()) << std::endl;

    // 申请发送后，bob 的 CONTACTS pts 应该被更新为 1
    auto [sl1, rl1] = GetLastID("user_bob", "pass_bob");
    EXPECT_TRUE(sl1.ok());
    int64_t bob_contacts_pts_after = GetPtsValue(rl1, SyncType::CONTACTS);
    EXPECT_EQ(bob_contacts_pts_after, 1) << "好友申请后 bob 的 CONTACTS pts 应为 1";
    std::cout << std::format("bob's CONTACTS pts after contact request: {}", bob_contacts_pts_after) << std::endl;
}

// 测试：同意好友申请后双方的 CONTACTS pts 均被更新
TEST_F(ServiceSync, PTS_ContactReply_Accept_UpdatesBothContactsPts) {
    // 注册两个用户
    auto [s1, r1] = SignUp("user_alice", "pass_alice", "pass_alice", "q1", "a1");
    ASSERT_EQ(r1.result().success_code(), SuccessCode::SUCCESS);

    auto [s2, r2] = SignUp("user_bob", "pass_bob", "pass_bob", "q2", "a2");
    ASSERT_EQ(r2.result().success_code(), SuccessCode::SUCCESS);

    // alice 发好友申请给 bob
    auto [s3, r3] = ContactRequest("user_alice", "pass_alice", "user_bob", "hi");
    ASSERT_EQ(r3.result().success_code(), SuccessCode::SUCCESS);

    // 记录接受前的 pts
    auto [sl0, rl0] = GetLastID("user_alice", "pass_alice");
    int64_t alice_pts_before = GetPtsValue(rl0, SyncType::CONTACTS);

    // bob 同意 alice 的申请
    auto [s4, r4] = ContactReply("user_bob", "pass_bob", "user_alice", ContactReplyCode::CONTACT_REPLY_ACCEPT);
    ASSERT_TRUE(s4.ok());
    ASSERT_EQ(r4.result().success_code(), SuccessCode::SUCCESS);
    std::cout << std::format("ContactReply result: {}", r4.result().message()) << std::endl;

    // 接受后双方 CONTACTS pts 均应增加
    auto [sl1, rl1] = GetLastID("user_alice", "pass_alice");
    auto [sl2, rl2] = GetLastID("user_bob",   "pass_bob"  );
    EXPECT_TRUE(sl1.ok());
    EXPECT_TRUE(sl2.ok());

    int64_t alice_pts_after = GetPtsValue(rl1, SyncType::CONTACTS);
    int64_t bob_pts_after   = GetPtsValue(rl2, SyncType::CONTACTS);

    // alice 的 pts 应该在之前基础上 +1（同意时增加）
    EXPECT_GT(alice_pts_after, alice_pts_before) << "同意好友申请后 alice 的 CONTACTS pts 应增加";
    // bob 的 pts 也应该增加（之前因为申请已经是 1，同意后变为 2）
    EXPECT_GE(bob_pts_after, 2) << "同意好友申请后 bob 的 CONTACTS pts 应 >= 2";

    std::cout << std::format("alice CONTACTS pts: {}, bob CONTACTS pts: {}", alice_pts_after, bob_pts_after) << std::endl;
}

// 测试：发送消息后接收方的 MESSAGES pts 被更新
TEST_F(ServiceSync, PTS_SendMessage_UpdatesReceiverMessagesPts) {
    // 注册两个用户并建立好友关系
    auto [s1, r1] = SignUp("user_alice", "pass_alice", "pass_alice", "q1", "a1");
    ASSERT_EQ(r1.result().success_code(), SuccessCode::SUCCESS);

    auto [s2, r2] = SignUp("user_bob", "pass_bob", "pass_bob", "q2", "a2");
    ASSERT_EQ(r2.result().success_code(), SuccessCode::SUCCESS);

    // 建立好友关系
    auto [s3, r3] = ContactRequest("user_alice", "pass_alice", "user_bob");
    ASSERT_EQ(r3.result().success_code(), SuccessCode::SUCCESS);
    auto [s4, r4] = ContactReply("user_bob", "pass_bob", "user_alice", ContactReplyCode::CONTACT_REPLY_ACCEPT);
    ASSERT_EQ(r4.result().success_code(), SuccessCode::SUCCESS);

    // 发送消息前检查 bob 的 MESSAGES pts（应为 -1 即无记录）
    auto [sl0, rl0] = GetLastID("user_bob", "pass_bob");
    EXPECT_TRUE(sl0.ok());
    int64_t bob_msg_pts_before = GetPtsValue(rl0, SyncType::MESSAGES);
    EXPECT_EQ(bob_msg_pts_before, -1) << "发送消息前 bob 的 MESSAGES pts 应不存在";

    // alice 向 bob 发送消息
    auto [s5, r5] = SendMessage("user_alice", "pass_alice", "user_bob", "Hello, Bob!");
    ASSERT_TRUE(s5.ok());
    ASSERT_EQ(r5.result().success_code(), SuccessCode::SUCCESS);
    std::cout << std::format("SendMessage server_message_id: {}", r5.server_message_id()) << std::endl;

    // 发送后 bob 的 MESSAGES pts 应更新为 1
    auto [sl1, rl1] = GetLastID("user_bob", "pass_bob");
    EXPECT_TRUE(sl1.ok());
    int64_t bob_msg_pts_after = GetPtsValue(rl1, SyncType::MESSAGES);
    EXPECT_EQ(bob_msg_pts_after, 1) << "发送消息后 bob 的 MESSAGES pts 应为 1";
    std::cout << std::format("bob's MESSAGES pts after SendMessage: {}", bob_msg_pts_after) << std::endl;

    // alice 再向 bob 发送第二条消息
    auto [s6, r6] = SendMessage("user_alice", "pass_alice", "user_bob", "Second message!");
    ASSERT_EQ(r6.result().success_code(), SuccessCode::SUCCESS);

    // bob 的 MESSAGES pts 应变为 2
    auto [sl2, rl2] = GetLastID("user_bob", "pass_bob");
    int64_t bob_msg_pts_after2 = GetPtsValue(rl2, SyncType::MESSAGES);
    EXPECT_EQ(bob_msg_pts_after2, 2) << "发送第二条消息后 bob 的 MESSAGES pts 应为 2";
    std::cout << std::format("bob's MESSAGES pts after second SendMessage: {}", bob_msg_pts_after2) << std::endl;
}

// 测试：GetLastID 对未鉴权请求返回空标签列表
TEST_F(ServiceSync, PTS_GetLastID_AuthFails_ReturnsEmpty) {
    auto [status, reply] = GetLastID("nonexistent_user", "wrong_password");
    EXPECT_TRUE(status.ok());  // gRPC 本身成功，但返回空列表
    EXPECT_EQ(reply.tags_size(), 0) << "鉴权失败时应返回空 tags";
}

// 测试：文件上传流程会更新发起者的 MESSAGES pts（用于客户端刷新上传相关状态）
TEST_F(ServiceSync, PTS_FileUpload_UpdatesMessagesPts) {
    auto [s1, r1] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    ASSERT_EQ(r1.result().success_code(), SuccessCode::SUCCESS);

    using namespace Utils;
    using namespace Utils::Algorithm;
    auto exe_path                            = app_path();
    auto [exe_file_size_success, exe_file_size] = Utils::file_size(exe_path);
    auto exe_md5_calculated                  = calculate_file_md5(exe_path);
    auto exe_file_name                       = exe_path.filename().string();

    // 获取 file_id 并上传
    auto [s2, r2] = GetFileID("value_user_id", "value_password", exe_file_name, exe_file_size, exe_md5_calculated);
    ASSERT_EQ(r2.result().success_code(), SuccessCode::SUCCESS);
    auto file_id = r2.file_id();

    auto [s3, r3, _] = FileUpload(file_id, exe_path.string());
    EXPECT_TRUE(s3.ok());

    // 上传相关流程后该用户的 MESSAGES pts 应已更新（由 GetFileID 触发）
    auto [sl, rl] = GetLastID("value_user_id", "value_password");
    EXPECT_TRUE(sl.ok());
    int64_t messages_pts = GetPtsValue(rl, SyncType::MESSAGES);
    EXPECT_GE(messages_pts, 1) << "文件上传流程应触发 MESSAGES pts 更新";
    std::cout << std::format("MESSAGES pts after file upload: {}", messages_pts) << std::endl;
}

// 测试：Sync 接口按 SyncRequestTag 返回 CONTACTS 增量 body（包含联系人资料）
TEST_F(ServiceSync, SyncBody_ContactRequest_ReturnsContactInfos) {
    auto [s1, r1] = SignUp("user_alice", "pass_alice", "pass_alice", "q1", "a1");
    auto [s2, r2] = SignUp("user_bob",   "pass_bob",   "pass_bob",   "q2", "a2");
    ASSERT_EQ(r1.result().success_code(), SuccessCode::SUCCESS);
    ASSERT_EQ(r2.result().success_code(), SuccessCode::SUCCESS);

    auto [s3, r3] = ContactRequest("user_alice", "pass_alice", "user_bob", "hello");
    ASSERT_TRUE(s3.ok());
    ASSERT_EQ(r3.result().success_code(), SuccessCode::SUCCESS);

    SyncRequest sync_request;
    sync_request.mutable_request_user_auth()->set_user_id("user_bob");
    sync_request.mutable_request_user_auth()->set_password("pass_bob");
    auto* tag = sync_request.add_sync_tag();
    tag->set_type(SyncType::CONTACTS);
    tag->set_id_start(0);
    tag->set_id_end(0);  // 拉取到最新

    auto [status_sync, replies] = SyncPull(sync_request);
    ASSERT_TRUE(status_sync.ok());
    ASSERT_GE(replies.size(), 2);

    // 第一条应为 header
    ASSERT_TRUE(replies[0].has_header());
    ASSERT_TRUE(replies[0].header().result().has_success_code());

    // 至少一条 body，且包含 CONTACTS 增量
    bool found_contacts_body = false;
    bool found_alice = false;
    for(const auto& rep : replies) {
        if(!rep.has_body()) {
            continue;
        }
        const auto& body = rep.body();
        if(body.tag().type() != SyncType::CONTACTS) {
            continue;
        }
        found_contacts_body = true;
        EXPECT_GE(body.count(), 1);
        if(body.has_contact_infos()) {
            for(int i = 0; i < body.contact_infos().contact_infos_size(); ++i) {
                const auto& info = body.contact_infos().contact_infos(i);
                if(info.base_info().user_id() == "user_alice") {
                    found_alice = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_contacts_body);
    EXPECT_TRUE(found_alice);
}

// 测试：Sync 接口按 SyncRequestTag 返回 MESSAGES 增量 body（包含完整消息明细）
TEST_F(ServiceSync, SyncBody_MessageSend_ReturnsMessageEvents) {
    auto [s1, r1] = SignUp("user_alice", "pass_alice", "pass_alice", "q1", "a1");
    auto [s2, r2] = SignUp("user_bob",   "pass_bob",   "pass_bob",   "q2", "a2");
    ASSERT_EQ(r1.result().success_code(), SuccessCode::SUCCESS);
    ASSERT_EQ(r2.result().success_code(), SuccessCode::SUCCESS);

    auto [s3, r3] = ContactRequest("user_alice", "pass_alice", "user_bob");
    ASSERT_EQ(r3.result().success_code(), SuccessCode::SUCCESS);
    auto [s4, r4] = ContactReply("user_bob", "pass_bob", "user_alice", ContactReplyCode::CONTACT_REPLY_ACCEPT);
    ASSERT_EQ(r4.result().success_code(), SuccessCode::SUCCESS);

    auto [s5, r5] = SendMessage("user_alice", "pass_alice", "user_bob", "hello bob");
    ASSERT_TRUE(s5.ok());
    ASSERT_EQ(r5.result().success_code(), SuccessCode::SUCCESS);

    SyncRequest sync_request;
    sync_request.mutable_request_user_auth()->set_user_id("user_bob");
    sync_request.mutable_request_user_auth()->set_password("pass_bob");
    auto* tag = sync_request.add_sync_tag();
    tag->set_type(SyncType::MESSAGES);
    tag->set_id_start(0);
    tag->set_id_end(0);

    auto [status_sync, replies] = SyncPull(sync_request);
    ASSERT_TRUE(status_sync.ok());
    ASSERT_GE(replies.size(), 2);

    bool found_messages_body = false;
    bool found_message_event = false;
    bool found_text = false;
    for(const auto& rep : replies) {
        if(!rep.has_body()) {
            continue;
        }
        const auto& body = rep.body();
        if(body.tag().type() != SyncType::MESSAGES) {
            continue;
        }
        found_messages_body = true;
        EXPECT_GE(body.count(), 1);
        if(body.has_messages()) {
            for(int i = 0; i < body.messages().messages_size(); ++i) {
                const auto& msg = body.messages().messages(i);
                found_message_event = true;
                if(msg.short_text() == "hello bob") {
                    found_text = true;
                }
            }
        }
    }

    EXPECT_TRUE(found_messages_body);
    EXPECT_TRUE(found_message_event);
    EXPECT_TRUE(found_text);
}

// 测试：Sync 接口在 MESSAGES 类型下返回 file_events 增量
TEST_F(ServiceSync, SyncBody_FileUpload_ReturnsFileEvents) {
    auto [s1, r1] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    ASSERT_EQ(r1.result().success_code(), SuccessCode::SUCCESS);

    using namespace Utils;
    using namespace Utils::Algorithm;
    auto exe_path = app_path();
    auto [exe_file_size_success, exe_file_size] = Utils::file_size(exe_path);
    auto exe_md5_calculated = calculate_file_md5(exe_path);
    auto exe_file_name = exe_path.filename().string();

    auto [s2, r2] = GetFileID("value_user_id", "value_password", exe_file_name, exe_file_size, exe_md5_calculated);
    ASSERT_EQ(r2.result().success_code(), SuccessCode::SUCCESS);
    auto file_id = r2.file_id();

    auto [s3, r3, _] = FileUpload(file_id, exe_path.string());
    ASSERT_TRUE(s3.ok());

    SyncRequest sync_request;
    sync_request.mutable_request_user_auth()->set_user_id("value_user_id");
    sync_request.mutable_request_user_auth()->set_password("value_password");
    auto* tag = sync_request.add_sync_tag();
    tag->set_type(SyncType::MESSAGES);
    tag->set_id_start(0);
    tag->set_id_end(0);

    auto [status_sync, replies] = SyncPull(sync_request);
    ASSERT_TRUE(status_sync.ok());
    ASSERT_GE(replies.size(), 2);

    bool found_messages_body = false;
    bool found_file_event = false;
    for(const auto& rep : replies) {
        if(!rep.has_body()) {
            continue;
        }
        const auto& body = rep.body();
        if(body.tag().type() != SyncType::MESSAGES) {
            continue;
        }
        found_messages_body = true;
        if(body.has_file_events()) {
            for(int i = 0; i < body.file_events().file_events_size(); ++i) {
                const auto& evt = body.file_events().file_events(i);
                if(evt.file_id() == file_id) {
                    found_file_event = true;
                }
            }
        }
    }

    EXPECT_TRUE(found_messages_body);
    EXPECT_TRUE(found_file_event);
}

