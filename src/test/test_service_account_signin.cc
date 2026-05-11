#include "test_headers.h"

class ServiceAccountSignIn : public ::testing::Test {
protected:
    const std::filesystem::path db_path = std::filesystem::current_path() / (get_current_file_name(__FILE__) + ".db");
    const std::string encryption_key = "your_secure_encryption_key";

    // 每个 TEST_F 运行前都会执行
    void SetUp() override {
        // `ASSERT_FALSE(false);` 会中断测试并标记为失败，而 `EXPECT_FALSE(false);` 会记录失败但继续执行测试。
        std::cout << std::format("database file: {}", db_path.string()) << std::endl;

        // 所有操作都必须成功，否则测试无法继续进行

        // 删除旧的数据库文件，确保每次测试都从干净的状态开始
        if(exists(db_path)) {
            try {
                // remove 返回 bool 值：成功删除返回 true，文件不存在返回 false
                if (remove(db_path)) {
                    std::cout << std::format("file {} removed successfully.", db_path.string()) << std::endl;
                } else {
                    std::cerr << std::format("file {} does not exist, no need to remove.", db_path.string()) << std::endl;
                }
            } catch (const std::filesystem::filesystem_error& e) {
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

// 登录测试
std::tuple<grpc::Status, SignInReply> SignIn(
    const string& account_id,
    const string& user_id   ,
    const string& token     ,
    const string& password
) {
    // 创建 AccountServiceImpl 实例
    AccountServiceImpl service;

    // 构造测试请求
    SignInRequest request;

    if(!account_id.empty() && !user_id.empty()) {
        request.mutable_request_user_auth()->clear_auth_id();
    }
    else if(!account_id.empty()) {
        request.mutable_request_user_auth()->set_account_id(account_id);
        request.mutable_request_user_auth()->clear_user_id();
    }
    else if(!user_id.empty()) {
        request.mutable_request_user_auth()->clear_account_id();
        request.mutable_request_user_auth()->set_user_id(user_id);
    }

    if(!password.empty() && !token.empty()) {
        request.mutable_request_user_auth()->clear_auth_method();
    }
    else if(!password.empty()) {
        request.mutable_request_user_auth()->clear_token();
        request.mutable_request_user_auth()->set_password(password);
    }
    else if(!token.empty()) {
        request.mutable_request_user_auth()->set_token(token);
        request.mutable_request_user_auth()->clear_password();
    }

    // 构造响应对象
    SignInReply response;

    // 调用 SignIn 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.SignIn(&context, &request, &response);
    return {status, response};
}

// 测试 AccountService 的 SignIn 方法

// 用户 ID 及账号 ID 均为空，返回错误码 AUTH_ID_EMPTY
TEST_F(ServiceAccountSignIn, auth_id_empty) {
    auto [status, response] = SignIn("", "", "value_token", "value_password");
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::AUTH_ID_EMPTY);
}

// token 与密码均为空，返回错误码 AUTH_METHOD_EMPTY
TEST_F(ServiceAccountSignIn, auth_method_empty) {
    auto [status, response] = SignIn("", "value_user_id", "", "");
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::AUTH_METHOD_EMPTY);
}

// 用户 ID 不存在、停用或错误，返回错误码 USER_ID_NOT_FOUND
TEST_F(ServiceAccountSignIn, user_id_not_found) {
    auto [status, response] = SignIn("", "userid_not_found", "", "value_password");
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_NOT_FOUND);
}

// 账号 ID 不存在、停用或错误，返回错误码 ACCOUNT_ID_NOT_FOUND
TEST_F(ServiceAccountSignIn, account_id_not_found) {
    auto [status, response] = SignIn("value_account_not_found", "", "value_token", "");
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::ACCOUNT_ID_NOT_FOUND);
}

// ============== 以下部分包含大量重复

// ================== 失败验证

// 用户 ID 或账号 ID 存在，但密码错误，返回错误码 PASSWORD_ERROR
TEST_F(ServiceAccountSignIn, password_error) {
    // 注册一个用户，确保后续登录测试有一个有效的账号
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // 使用用户名和错误密码登录时要确保账号 ID 和 token 都为空，
    // 否则会优先使用账号 ID 和 token 进行登录，从而导致验证失败

    // 用户 ID + 错误密码 登录
    auto [status_signin_user_id, response_signin_user_id] = SignIn("", "value_user_id", "", "wrong_password");
    EXPECT_TRUE(status_signin_user_id.ok());
    EXPECT_EQ(response_signin_user_id.result().error_code(), ErrorCode::PASSWORD_ERROR);

    // 账号 ID + 错误密码 登录
    auto [status_signin_account_id, response_signin_account_id] = SignIn(account_id_signup, "", "", "wrong_password");
    EXPECT_TRUE(status_signin_account_id.ok());
    EXPECT_EQ(response_signin_account_id.result().error_code(), ErrorCode::PASSWORD_ERROR);
}

// 用户 ID 或账号 ID 存在，但 token 错误，返回错误码 TOKEN_ERROR
TEST_F(ServiceAccountSignIn, token_error) {
    // 注册一个用户，确保后续登录测试有一个有效的账号
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // 用户 ID + 错误 token 登录
    auto [status_signin_user_id, response_signin_user_id] = SignIn("", "value_user_id", "value_wrong_token", "");
    EXPECT_TRUE(status_signin_user_id.ok());
    EXPECT_EQ(response_signin_user_id.result().error_code(), ErrorCode::TOKEN_ERROR);

    // 账号 ID + 错误 token 登录
    auto [status_signin_account_id, response_signin_account_id] = SignIn(account_id_signup, "", "value_wrong_token", "");
    EXPECT_TRUE(status_signin_account_id.ok());
    EXPECT_EQ(response_signin_account_id.result().error_code(), ErrorCode::TOKEN_ERROR);
}

// ================== 成功验证

// 用户 ID + 密码登录
TEST_F(ServiceAccountSignIn, user_id_password) {
    // 注册一个用户，确保后续登录测试有一个有效的账号
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // 使用用户名和密码登录时要确保账号 ID 和 token 都为空，
    // 否则会优先使用账号 ID 和 token 进行登录，从而导致验证失败
    auto [status_signin, response_signin] = SignIn("", "value_user_id", "", "value_password");
    EXPECT_TRUE(status_signin.ok());
    EXPECT_EQ(response_signin.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signin.account_id().empty());  // 登录成功应该返回一个非空的 account_id
    EXPECT_TRUE(!response_signin.token     ().empty());  // 登录成功应该返回一个非空的 token
    std::cout << std::format("Signin account_id: {}", response_signin.account_id()) << std::endl;
    std::cout << std::format("Signin token: {}"     , response_signin.token     ()) << std::endl;
    auto account_id_signin = response_signin.account_id();
    auto token_signin      = response_signin.token     ();

    // 登录返回的 account_id 应该与注册时的一致
    EXPECT_EQ(account_id_signup, account_id_signin);
}

// 账号 ID + 密码登录
// 账号 ID 需要在注册或登录时获得
TEST_F(ServiceAccountSignIn, account_id_password) {
    // 注册一个用户，确保后续登录测试有一个有效的账号
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // 使用账号 ID 和密码登录时要确保 token 为空，
    // 否则会优先使用账号 ID 和 token 进行登录，从而导致验证失败
    auto [status_signin, response_signin] = SignIn(account_id_signup, "", "", "value_password");
    EXPECT_TRUE(status_signin.ok());
    EXPECT_EQ(response_signin.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signin.account_id().empty());  // 登录成功应该返回一个非空的 account_id
    EXPECT_TRUE(!response_signin.token     ().empty());  // 登录成功应该返回一个非空的 token
    std::cout << std::format("Signin account_id: {}", response_signin.account_id()) << std::endl;
    std::cout << std::format("Signin token: {}"     , response_signin.token     ()) << std::endl;
    auto account_id_signin = response_signin.account_id();
    auto token_signin      = response_signin.token     ();

    // 登录返回的 account_id 应该与注册时的一致
    EXPECT_EQ(account_id_signup, account_id_signin);
}

// 用户 ID + token 登录
// token 需要在登录时获得
TEST_F(ServiceAccountSignIn, user_id_token) {
    // 注册一个用户，确保后续登录测试有一个有效的账号
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // ============== 第一次登录

    // 使用用户名和密码登录时要确保账号 ID 和 token 都为空，
    // 否则会优先使用账号 ID 和 token 进行登录，从而导致验证失败
    auto [status_signin, response_signin] = SignIn("", "value_user_id", "", "value_password");
    EXPECT_TRUE(status_signin.ok());
    EXPECT_EQ(response_signin.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signin.account_id().empty());  // 登录成功应该返回一个非空的 account_id
    EXPECT_TRUE(!response_signin.token     ().empty());  // 登录成功应该返回一个非空的 token
    std::cout << std::format("Signin account_id: {}", response_signin.account_id()) << std::endl;
    std::cout << std::format("Signin token: {}"     , response_signin.token     ()) << std::endl;
    auto account_id_signin = response_signin.account_id();
    auto token_signin      = response_signin.token     ();

    // 登录返回的 account_id 应该与注册时的一致
    EXPECT_EQ(account_id_signup, account_id_signin);

    // ============== 第二次登录

    // 使用用户名和密码登录时要确保账号 ID 和 token 都为空，
    // 否则会优先使用账号 ID 和 token 进行登录，从而导致验证失败
    auto [status_auth, response_auth] = SignIn("", "value_user_id", token_signin, "");
    EXPECT_TRUE(status_auth.ok());
    EXPECT_EQ(response_auth.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_auth.account_id().empty());  // 登录成功应该返回一个非空的 account_id
    EXPECT_TRUE(!response_auth.token     ().empty());  // 登录成功应该返回一个非空的 token
    std::cout << std::format("Signin account_id: {}", response_auth.account_id()) << std::endl;
    std::cout << std::format("Signin token: {}"     , response_auth.token     ()) << std::endl;
    auto account_id_auth = response_auth.account_id();
    auto token_auth      = response_auth.token     ();

    // 第二次登录返回的 account_id 应该与注册时的一致
    EXPECT_EQ(account_id_signup, account_id_auth);
    // 第二次登录返回的 token 应该与第一次登录的一致
    EXPECT_EQ(token_signin, token_auth);
}

// 账号 ID + token 登录
TEST_F(ServiceAccountSignIn, account_id_token) {
    // 注册一个用户，确保后续登录测试有一个有效的账号
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered account_id: {}", response_signup.account_id()) << std::endl;
    auto account_id_signup = response_signup.account_id();

    // ============== 第一次登录

    // 使用用户名和密码登录时要确保账号 ID 和 token 都为空，
    // 否则会优先使用账号 ID 和 token 进行登录，从而导致验证失败
    auto [status_signin, response_signin] = SignIn("", "value_user_id", "", "value_password");
    EXPECT_TRUE(status_signin.ok());
    EXPECT_EQ(response_signin.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signin.account_id().empty());  // 登录成功应该返回一个非空的 account_id
    EXPECT_TRUE(!response_signin.token     ().empty());  // 登录成功应该返回一个非空的 token
    std::cout << std::format("Signin account_id: {}", response_signin.account_id()) << std::endl;
    std::cout << std::format("Signin token: {}"     , response_signin.token     ()) << std::endl;
    auto account_id_signin = response_signin.account_id();
    auto token_signin      = response_signin.token     ();

    // 登录返回的 account_id 应该与注册时的一致
    EXPECT_EQ(account_id_signup, account_id_signin);

    // ============== 第二次登录

    // 使用用户名和密码登录时要确保账号 ID 和 token 都为空，
    // 否则会优先使用账号 ID 和 token 进行登录，从而导致验证失败
    auto [status_auth, response_auth] = SignIn(account_id_signin, "", token_signin, "");
    EXPECT_TRUE(status_auth.ok());
    EXPECT_EQ(response_auth.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_auth.account_id().empty());  // 登录成功应该返回一个非空的 account_id
    EXPECT_TRUE(!response_auth.token     ().empty());  // 登录成功应该返回一个非空的 token
    std::cout << std::format("Signin account_id: {}", response_auth.account_id()) << std::endl;
    std::cout << std::format("Signin token: {}"     , response_auth.token     ()) << std::endl;
    auto account_id_auth = response_auth.account_id();
    auto token_auth      = response_auth.token     ();

    // 第二次登录返回的 account_id 应该与注册时的一致
    EXPECT_EQ(account_id_signup, account_id_auth);
    // 第二次登录返回的 token 应该与第一次登录的一致
    EXPECT_EQ(token_signin, token_auth);
}
