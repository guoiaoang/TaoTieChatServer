#include "test_headers.h"

class ServiceSearchUser : public ::testing::Test {
protected:
    const std::filesystem::path db_path = std::filesystem::current_path() / (get_current_file_name(__FILE__) + ".db");
    const std::string encryption_key = "your_secure_encryption_key";

    // 每个 TEST_F 运行前都会执行
    void SetUp() override {
        // `ASSERT_FALSE(false);` 会中断测试并标记为失败，而 `EXPECT_FALSE(false);` 会记录失败但继续执行测试。
        std::cout << std::format("database file: {}", db_path.string()) << std::endl;

        // 所有操作都必须成功，否则测试无法继续进行

        // 删除旧的数据库文件，确保每次测试都从干净的状态开始
        if (exists(db_path)) {
            try {
                // remove 返回 bool 值：成功删除返回 true，文件不存在返回 false
                if (remove(db_path)) {
                    std::cout << std::format("file {} removed successfully.", db_path.string()) << std::endl;
                }
                else {
                    std::cerr << std::format("file {} does not exist, no need to remove.", db_path.string()) << std::endl;
                }
            }
            catch (const filesystem_error& e) {
                // 如果文件正在被占用或权限不足，会抛出异常
                std::cerr << std::format("remove {} failed: {}", db_path.string(), e.what()) << std::endl;
            }
            // remove(db_path);
        }
        ASSERT_FALSE(exists(db_path)) << std::format("Failed to remove old database file: {}", db_path.string());

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
    const string& user_id,
    const string& password,
    const string& confirm,
    const string& security_question,
    const string& security_answer
) {
    // 创建 AccountServiceImpl 实例
    AccountServiceImpl service;

    // 构造测试请求
    SignUpRequest request;
    if (user_id.empty()) { request.clear_user_id(); }
    else { request.set_user_id(user_id); }
    if (password.empty()) { request.clear_password(); }
    else { request.set_password(password); }
    if (confirm.empty()) { request.clear_confirm(); }
    else { request.set_confirm(confirm); }
    if (security_question.empty()) { request.clear_security_question(); }
    else { request.set_security_question(security_question); }
    if (security_answer.empty()) { request.clear_security_answer(); }
    else { request.set_security_answer(security_answer); }

    // 构造响应对象
    SignUpReply response;

    // 调用 SignUp 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.SignUp(&context, &request, &response);
    return { status, response };
}

// 搜索测试 // 为了简化，这里仅使用账户密码进行授权验证
std::tuple<grpc::Status, SearchUserReply> SearchUser(
    const string& request_user_id ,  // 搜索请求用户的 ID
    const string& request_password,  // 搜索请求用户的密码
    const string& target_user_id     // 被搜索用户的 ID
) {
    // 创建 SearchServiceImpl 实例
    SearchServiceImpl service;

    // 构造测试请求
    SearchUserRequest request;
    request.set_user_id(target_user_id);  // 搜索目标用户 ID
    request.mutable_request_user_auth()->set_user_id(request_user_id);    // 搜索请求用户 ID
    request.mutable_request_user_auth()->set_password(request_password);  // 搜索请求用户密码

    // 构造响应对象
    SearchUserReply response;

    // 调用 SearchUser 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.SearchUser(&context, &request, &response);
    return { status, response };
}

// 发送好友申请
std::tuple<grpc::Status, ContactRequestReply> ContactRequest(
    const string& request_user_id, const string& request_user_password,
    const string& target_user_id
) {
    // 创建 AccountServiceImpl 实例
    AccountServiceImpl service;

    // 构造测试请求
    ContactRequestRequest request;

    if(request_user_id.empty() && request_user_password.empty()) {
        request.clear_request_user_auth();
    }
    else {
        if(!request_user_id.empty()) {
            request.mutable_request_user_auth()->set_user_id(request_user_id);
        }
        else {
            request.mutable_request_user_auth()->clear_auth_id();
        }

        if(!request_user_password.empty()) {
            request.mutable_request_user_auth()->set_password(request_user_password);
        }
        else {
            request.mutable_request_user_auth()->clear_auth_method();
        }
    }

    if(!target_user_id.empty()) {
        request.set_target_user_id(target_user_id);
    }
    else {
        request.clear_target_user_id();
    }

    // 构造响应对象
    ContactRequestReply response;

    // 调用 ContactRequest 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.ContactRequest(&context, &request, &response);
    return {status, response};
}

// 好友申请的回复
std::tuple<grpc::Status, ContactReplyReply> ContactReply(
    const string& request_user_id, const string& request_user_password,
    const string& target_user_id, const ContactReplyCode& reply
) {
    // 创建 AccountServiceImpl 实例
    AccountServiceImpl service;

    // 构造测试请求
    ContactReplyRequest request;
    if(request_user_id.empty() && request_user_password.empty()) {
        request.clear_request_user_auth();
    }
    else {
        if(!request_user_id.empty()) {
            request.mutable_request_user_auth()->set_user_id(request_user_id);
        }
        else {
            request.mutable_request_user_auth()->clear_auth_id();
        }

        if(!request_user_password.empty()) {
            request.mutable_request_user_auth()->set_password(request_user_password);
        }
        else {
            request.mutable_request_user_auth()->clear_auth_method();
        }
    }
    if(!target_user_id.empty()) {
        request.set_target_user_id(target_user_id);
    }
    else {
        request.clear_target_user_id();
    }
    // 设置好友申请的回复
    request.set_reply(reply);

    // 构造响应对象
    ContactReplyReply response;

    // 调用 ContactReply 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.ContactReply(&context, &request, &response);
    return {status, response};
}

// 目标用户 ID 为空，返回错误码 TARGET_USER_ID_EMPTY
TEST_F(ServiceSearchUser, user_id_empty) {
    auto [status_search, response_search] = SearchUser("value_user_id", "value_password", "");
    EXPECT_TRUE(status_search.ok());
    EXPECT_EQ(response_search.result().error_code(), ErrorCode::TARGET_USER_ID_EMPTY);
}

// 目标用户 ID 不存在，返回空列表和错误码 TARGET_USER_ID_NOT_FOUND
TEST_F(ServiceSearchUser, user_id_not_found) {
    // 注册请求用户的账号，确保搜索时能够通过授权验证
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // 搜索一个不存在的用户 ID，应该返回错误码 TARGET_USER_ID_NOT_FOUND
    auto [status_search, response_search] = SearchUser("value_user_id", "value_password", "nonexistent_userid");
    EXPECT_TRUE(status_search.ok());
    EXPECT_EQ(response_search.result().error_code(), ErrorCode::TARGET_USER_ID_NOT_FOUND);
}

// 无请求用户信息时搜索一个存在用户，也应该返回成功码 SUCCESS 和用户的基本信息
TEST_F(ServiceSearchUser, search_user_without_request_user_info) {
    // 注册一个用户，确保后续可以搜索到该用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // 搜索用户
    auto [status_search, response_search] = SearchUser("", "", "value_user_id");
    EXPECT_TRUE(status_search.ok());
    EXPECT_EQ(response_search.result().success_code(), SuccessCode::SUCCESS);

    // 非注册用户，搜索成功后返回基础信息
    auto contact_infos = response_search.contact_info();
    EXPECT_EQ(contact_infos.base_info().user_id(), "value_user_id");
    EXPECT_FALSE(contact_infos.has_friend_info());  // 因为没有好友关系，所以 friend_info 不应该存在
}

// 已注册用户搜索一个存在的非好友关系的用户 ID，应该返回成功码 SUCCESS 和用户的基本信息
TEST_F(ServiceSearchUser, search_user_without_friend_relationship) {
    // 注册一个用户，确保后续可以搜索到该用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // 搜索用户
    auto [status_search, response_search] = SearchUser("value_user_id", "value_password", "value_user_id");
    EXPECT_TRUE(status_search.ok());
    EXPECT_EQ(response_search.result().success_code(), SuccessCode::SUCCESS);

    // 已注册用户搜索非好友关系搜索成功后返回基础信息
    auto contact_infos = response_search.contact_info();
    // DebugString 会忽略某些类型或值的输出，输出不全时并非是内存的问题
    // std::cout << std::format("response_search : {}", response_search.DebugString()) << std::endl;
    EXPECT_EQ(contact_infos.base_info().user_id(), "value_user_id");
    EXPECT_FALSE(contact_infos.has_friend_info());  // 因为没有好友关系，所以 friend_info 不应该存在
}

// 已注册用户搜索一个存在的好友关系的用户 ID，应该返回成功码 SUCCESS 和用户的基本信息以及好友信息
TEST_F(ServiceSearchUser, search_user_with_friend_info) {
    // 注册一个用户，确保后续可以搜索到该用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // 自己可以申请添加自己为好友
    auto [status_request, response_request] = ContactRequest("value_user_id", "value_password", "value_user_id");
    EXPECT_TRUE(status_request.ok());
    EXPECT_EQ(response_request.result().success_code(), SuccessCode::SUCCESS);

    // 同意添加好友的申请
    auto [status_reply, response_reply] = ContactReply("value_user_id", "value_password", "value_user_id", ContactReplyCode::CONTACT_REPLY_ACCEPT);
    EXPECT_TRUE(status_reply.ok());
    EXPECT_EQ(response_reply.result().success_code(), SuccessCode::SUCCESS);

    // 搜索用户
    auto [status_search, response_search] = SearchUser("value_user_id", "value_password", "value_user_id");
    EXPECT_TRUE(status_search.ok());
    EXPECT_EQ(response_search.result().success_code(), SuccessCode::SUCCESS);

    // 好友关系搜索成功后返回基础信息和好友信息
    // 好友信息不为空即可，好友信息中的 user_id 和 account_id 目前都是冗余的，暂时不进行验证
    auto contact_infos = response_search.contact_info();
    // DebugString 会忽略某些类型或值的输出，输出不全时并非是内存的问题
    // std::cout << std::format("response_search : {}", response_search.DebugString()) << std::endl;
    EXPECT_EQ(contact_infos.base_info().user_id(), "value_user_id");
    EXPECT_TRUE(contact_infos.has_friend_info());  // 因为有好友关系，所以 friend_info 应该存在
    // EXPECT_EQ(contact_infos.friend_info().user_id(), "value_user_id");  // 好友信息中的 user_id 为冗余设计
    EXPECT_EQ(contact_infos.friend_info().status (), 1    );  // 好友信息中的 status 应该为 1，表示已经是好友
    EXPECT_EQ(contact_infos.friend_info().starred(), false);  // 好友信息中的 starred 应该为 false
    EXPECT_EQ(contact_infos.friend_info().silent (), false);  // 好友信息中的 silent 应该为 false
}
