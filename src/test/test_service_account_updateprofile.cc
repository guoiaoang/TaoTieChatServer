#include "test_headers.h"

class ServiceAccountUpdateProfile : public ::testing::Test {
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

struct ContactInfo {
    string user_id  = string{ };
    string nickname = string{ };
    string avatar   = string{ };
    string sign     = string{ };
    int    gender   = int   {0};
    string region   = string{ };
    string birthday = string{ };
    vector<string> phones = { };
    vector<string> emails = { };
    Contact to_Contact() const {
        Contact contact;
        contact.set_user_id  (user_id  );
        contact.set_nickname (nickname );
        contact.set_avatar   (avatar   );
        contact.set_sign     (sign     );
        contact.set_gender   (gender   );
        contact.set_region   (region   );
        contact.set_birthday (birthday );
        for(const auto& phone : phones) { *contact.add_phones() = phone; }
        for(const auto& email : emails) { *contact.add_emails() = email; }
        return contact;
    }
};

// 更新用户资料测试
std::tuple<grpc::Status, UpdateProfileReply> UpdateProfile(
    const string& request_user_id   ,
    const string& request_password  ,
    const string& request_account_id,
    const string& request_token     ,
    const ContactInfo& contact_info
) {
    // 创建 AccountServiceImpl 实例
    AccountServiceImpl service;

    // 构造测试请求
    UpdateProfileRequest request;

    // 1, 第一层
    if(request_user_id.empty() && request_password.empty() && request_account_id.empty() && request_token.empty()) {
        request.clear_request_user_auth();
    }
    else {
        // 2. 第二层
        if(request_account_id.empty() && request_user_id .empty()) {
            request.mutable_request_user_auth()->clear_auth_id    ();
        }
        else {
            // 3. 第三层
            ;    if(!request_account_id.empty()) { request.mutable_request_user_auth()->set_account_id(request_account_id); }
            else if(!request_user_id   .empty()) { request.mutable_request_user_auth()->set_user_id   (request_user_id   ); }
        }

        if(request_token     .empty() && request_password.empty()) {
            request.mutable_request_user_auth()->clear_auth_method();
        }
        else {
            ;    if(!request_token     .empty()) { request.mutable_request_user_auth()->set_token     (request_token     ); }
            else if(!request_password  .empty()) { request.mutable_request_user_auth()->set_password  (request_password  ); }
        }
    }

    request.mutable_contact()->CopyFrom(contact_info.to_Contact());

    // 构造响应对象
    UpdateProfileReply response;

    // 调用 UpdateProfile 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.UpdateProfile(&context, &request, &response);
    return {status, response};
}

// 测试 AccountService 的 UpdateProfile 方法

// 未提供认证信息，返回错误码 AUTH_ID_EMPTY (没有 optional 修饰，不会返回 AUTH_INFO_NONE)
TEST_F(ServiceAccountUpdateProfile, auth_info_missing) {
    auto [status, response] = UpdateProfile("", "", "", "", ContactInfo{});
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::AUTH_ID_EMPTY);
}

// 错误的用户 ID，返回错误码 USER_ID_NOT_FOUND
TEST_F(ServiceAccountUpdateProfile, auth_failed) {
    auto [status, response] = UpdateProfile("user_id", "password", "", "", ContactInfo{});
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_NOT_FOUND);
}

// 没有目标用户 ID，返回错误码 TARGET_USER_ID_EMPTY
TEST_F(ServiceAccountUpdateProfile, target_user_id_empty) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("user_id", "password", "password", "security_question", "security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    ContactInfo contact_info = ContactInfo{
        .user_id  = "" ,
        .nickname = "nickname",
        .avatar   = "avatar"  ,
        .sign     = "sign"    ,
        .gender   = 0,
        .region   = "region"  ,
        .birthday = "birthday",
        .phones = { "010-12345", "+86 18812341234" },
        .emails = { "example@example.com" },
    };
    auto [status, response] = UpdateProfile("user_id", "password", "", "", contact_info);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_USER_ID_EMPTY);
}

// 不修改用户 ID，成功修改用户资料，返回成功码 SUCCESS
TEST_F(ServiceAccountUpdateProfile, target_user_id_not_changed) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("user_id", "password", "password", "security_question", "security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    ContactInfo contact_info = ContactInfo{
        .user_id  = "user_id" ,
        .nickname = "nickname",
        .avatar   = "avatar"  ,
        .sign     = "sign"    ,
        .gender   = 0,
        .region   = "region"  ,
        .birthday = "birthday",
        .phones = { "010-12345", "+86 18812341234" },
        .emails = { "example@example.com" },
    };
    auto [status, response] = UpdateProfile("user_id", "password", "", "", contact_info);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);
}

// 修改用户 ID，成功修改用户资料，返回成功码 SUCCESS
TEST_F(ServiceAccountUpdateProfile, target_user_id_changed) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("user_id", "password", "password", "security_question", "security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    ContactInfo contact_info = ContactInfo{
        .user_id  = "user_id_changed" ,
        .nickname = "nickname",
        .avatar   = "avatar"  ,
        .sign     = "sign"    ,
        .gender   = 0,
        .region   = "region"  ,
        .birthday = "birthday",
        .phones = { "010-12345", "+86 18812341234" },
        .emails = { "example@example.com" },
    };
    auto [status, response] = UpdateProfile("user_id", "password", "", "", contact_info);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);
}

// 修改用户 ID 后重复执行，返回错误码 USER_ID_NOT_FOUND
TEST_F(ServiceAccountUpdateProfile, target_user_id_changed_again) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("user_id", "password", "password", "security_question", "security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    ContactInfo contact_info = ContactInfo{
        .user_id  = "user_id_changed" ,
        .nickname = "nickname",
        .avatar   = "avatar"  ,
        .sign     = "sign"    ,
        .gender   = 0,
        .region   = "region"  ,
        .birthday = "birthday",
        .phones = { "010-12345", "+86 18812341234" },
        .emails = { "example@example.com" },
    };
    auto [status, response] = UpdateProfile("user_id", "password", "", "", contact_info);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);

    // 再次以相同的认证信息修改用户资料，应该找不到用户了，因为用户 ID 已经被修改了
    auto [status_update_profile_again, response_update_profile_again] = UpdateProfile("user_id", "password", "", "", contact_info);
    EXPECT_TRUE(status_update_profile_again.ok());
    EXPECT_EQ(response_update_profile_again.result().error_code(), ErrorCode::USER_ID_NOT_FOUND);
}

// 目标用户 ID 已经存在，返回错误码 USER_ID_EXISTS
TEST_F(ServiceAccountUpdateProfile, target_user_id_exists) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("user_id", "password", "password", "security_question", "security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    auto [status_signup_target, response_signup_target] = SignUp("target_user_id", "password", "password", "security_question", "security_answer");
    EXPECT_TRUE(status_signup_target.ok());
    EXPECT_EQ(response_signup_target.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup_target.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered target account_id: {}", response_signup_target.account_id()) << std::endl;
    auto target_account_id = response_signup_target.account_id();

    ContactInfo contact_info = ContactInfo{
        .user_id  = "target_user_id",
        .nickname = "nickname",
        .avatar   = "avatar"  ,
        .sign     = "sign"    ,
        .gender   = 0,
        .region   = "region"  ,
        .birthday = "birthday",
        .phones = { "010-12345", "+86 18812341234" },
        .emails = { "example@example.com" },
    };
    auto [status, response] = UpdateProfile("user_id", "password", "", "", contact_info);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_EXISTS);
}
