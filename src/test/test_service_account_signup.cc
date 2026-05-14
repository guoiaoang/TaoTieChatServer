#include "test_headers.h"

class ServiceAccountSignUp : public ::testing::Test {
protected:
    const path db_path = Utils::app_dir() / (get_current_file_name(__FILE__) + ".db");
    const string encryption_key = "your_secure_encryption_key";

    // 每个 TEST_F 运行前都会执行
    void SetUp() override {
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

// 测试 AccountService 的 SignUp 方法

// 用户名为空，返回错误码 USER_ID_EMPTY
TEST_F(ServiceAccountSignUp, username_empty) {
    auto [status, response] = SignUp("", "value_password", "value_password", "value_security_question", "value_security_answer");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_EMPTY);
}

// 密码为空，返回错误码 PASSWORD_EMPTY
TEST_F(ServiceAccountSignUp, password_empty) {
    auto [status, response] = SignUp("value_user_id", "", "value_password", "value_security_question", "value_security_answer");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::PASSWORD_EMPTY);
}

// 确认密码为空，返回错误码 DIFF_PASSWORD_WITH_CONFIRM
TEST_F(ServiceAccountSignUp, confirm_empty) {
    auto [status, response] = SignUp("value_user_id", "value_password", "", "value_security_question", "value_security_answer");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::DIFF_PASSWORD_WITH_CONFIRM);
}

// 确认密码与密码不一致，返回错误码 DIFF_PASSWORD_WITH_CONFIRM
TEST_F(ServiceAccountSignUp, confirm_diff) {
    auto [status, response] = SignUp("value_user_id", "value_password", "password_confim", "value_security_question", "value_security_answer");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::DIFF_PASSWORD_WITH_CONFIRM);
}

// 安全问题为空，返回错误码 SECURITY_QUESTION_EMPTY
TEST_F(ServiceAccountSignUp, security_question_empty) {
    auto [status, response] = SignUp("value_user_id", "value_password", "value_password", "", "value_security_answer");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::SECURITY_QUESTION_EMPTY);
}

// 安全答案为空，返回错误码 SECURITY_ANSWER_EMPTY
TEST_F(ServiceAccountSignUp, security_answer_empty) {
    auto [status, response] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::SECURITY_ANSWER_EMPTY);
}

// 注册成功，返回成功码 SUCCESS
TEST_F(ServiceAccountSignUp, Signup_success) {
    auto [status, response] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response.account_id()) << std::endl;
}

// 用户名已存在，返回错误码 USER_ID_EXISTS
TEST_F(ServiceAccountSignUp, username_exists) {
    // 第一次注册，应该成功
    auto [status_pre, response_pre] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    // 验证返回状态
    EXPECT_TRUE(status_pre.ok());
    EXPECT_EQ(response_pre.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_pre.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_pre.account_id()) << std::endl;

    // 第二次注册，应该因为用户名已存在而失败
    auto [status_exists, response_exists] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    // 验证返回状态
    EXPECT_TRUE(status_exists.ok());
    EXPECT_EQ(response_exists.result().error_code(), ErrorCode::USER_ID_EXISTS);
}
