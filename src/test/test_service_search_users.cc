#include "test_headers.h"

class ServiceSearchUsers : public ::testing::Test {
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
std::tuple<grpc::Status, SearchUsersReply> SearchUsers(
    const string& request_user_id ,  // 搜索请求用户的 ID
    const string& request_password,  // 搜索请求用户的密码
    const string& target_user_id     // 被搜索用户的 ID
) {
    // 创建 SearchServiceImpl 实例
    SearchServiceImpl service;

    // 构造测试请求
    SearchUsersRequest request;
    request.set_user_id(target_user_id);  // 搜索目标用户 ID
    request.mutable_request_user_auth()->set_user_id(request_user_id);    // 搜索请求用户 ID
    request.mutable_request_user_auth()->set_password(request_password);  // 搜索请求用户密码

    // 构造响应对象
    SearchUsersReply response;

    // 调用 SearchUsers 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.SearchUsers(&context, &request, &response);
    return { status, response };
}

// 目标用户 ID 为空，返回错误码 TARGET_USER_ID_EMPTY
TEST_F(ServiceSearchUsers, user_id_empty) {
    auto [status_search, response_search] = SearchUsers("userid", "password", "");
    EXPECT_TRUE(status_search.ok());
    EXPECT_EQ(response_search.result().error_code(), ErrorCode::TARGET_USER_ID_EMPTY);
}

// 目标用户 ID 不存在，返回空列表和错误码 TARGET_USER_ID_NOT_FOUND
TEST_F(ServiceSearchUsers, user_id_not_found) {
    // 注册请求用户的账号，确保搜索时能够通过授权验证
    auto [status_signup, response_signup] = SignUp("userid", "password", "password", "security_question", "security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // 搜索一个不存在的用户 ID，应该返回错误码 TARGET_USER_ID_NOT_FOUND
    auto [status_search, response_search] = SearchUsers("userid", "password", "nonexistent_userid");
    EXPECT_TRUE(status_search.ok());
    EXPECT_EQ(response_search.result().error_code(), ErrorCode::TARGET_USER_ID_NOT_FOUND);
}

// 搜索一个存在的用户 ID，应该返回成功码 SUCCESS 和正确的用户信息
TEST_F(ServiceSearchUsers, search_users) {
    // 注册一个用户，确保后续可以搜索到该用户
    auto [status_signup, response_signup] = SignUp("userid", "password", "password", "security_question", "security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // 搜索用户
    auto [status_search, response_search] = SearchUsers("userid", "password", "userid");
    EXPECT_TRUE(status_search.ok());
    EXPECT_EQ(response_search.result().success_code(), SuccessCode::SUCCESS);

    // 搜索成功应该返回正确的用户信息
    auto contacts = response_search.contacts();
    EXPECT_EQ(contacts.contact_size(), 1);  // 应该找到一个用户
    for (const auto& contact : contacts.contact()) {
        EXPECT_EQ(contact.user_id(), "userid");
    }
}
