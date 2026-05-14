#include "test_headers.h"

class ServiceMessageSendMessage : public ::testing::Test {
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

// // 添加好友测试
// std::tuple<grpc::Status, AddContactReply> AddContact(
//     const string& request_user_id, const string& request_user_password,
//     const string& target_user_id
// ) {
//     // 创建 AccountServiceImpl 实例
//     AccountServiceImpl service;
//
//     // 构造测试请求
//     AddContactRequest request;
//     if(!target_user_id.empty()) {
//         request.set_target_user_id(target_user_id);
//     }
//     else {
//         request.clear_target_user_id();  // 无效，因为 target_user_id 是 string 类型，不是 optional string 类型。
//     }
//     if(request_user_id.empty() && request_user_password.empty()) {
//         request.clear_request_user_auth();  // 无效，因为 request_user_auth 是 AuthRequest 类型，不是 optional AuthRequest 类型。
//     }
//     else {
//         if(!request_user_id.empty()) {
//             request.mutable_request_user_auth()->set_user_id(request_user_id);
//         }
//         else {
//             request.mutable_request_user_auth()->clear_auth_id();
//         }
//
//         if(!request_user_password.empty()) {
//             request.mutable_request_user_auth()->set_password(request_user_password);
//         }
//         else {
//             request.mutable_request_user_auth()->clear_auth_method();
//         }
//     }
//
//     // 构造响应对象
//     AddContactReply response;
//
//     // 调用 AddContact 方法
//     grpc::ServerContext context;  // 模拟 gRPC 上下文
//     grpc::Status status = service.AddContact(&context, &request, &response);
//     return {status, response};
// }

// 发送消息测试
std::tuple<grpc::Status, MessageReply> SendMessage(
    const string& request_user_id, const string& request_user_password,
    const string& target_user_id , const string& short_message, const string&client_message_id
) {
    // 创建 MessageServiceImpl 实例
    MessageServiceImpl service;

    // 构造测试请求
    MessageContext request;
    // 设置鉴权信息
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
    // 设置时间  // 目前暂未使用到
    Utils::set_timestamp(*request.mutable_timestamp(), std::chrono::system_clock::now());
    // 设置消息 ID
    request.set_client_message_id(client_message_id);
    // 设置目标用户 ID
    if(!target_user_id.empty()) {
        request.set_user_id(target_user_id);
    }
    else {
        request.clear_user_id();  // 无效，因为 target_user_id 是 string 类型，不是 optional string 类型。
    }
    // 设置消息
    request.set_short_text(short_message);

    // 构造响应对象
    MessageReply response;

    // 调用 SendMessage 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.SendMessage(&context, &request, &response);
    return {status, response};
}

// 测试 AccountService 的 SendMessage 方法

// 未提供目标用户 ID，返回错误码 TARGET_ID_REQUESTED
TEST_F(ServiceMessageSendMessage, target_user_id_empty) {
    string client_message_id = Utils::generate_message_id();
    auto [status, response] = SendMessage("value_user_id", "value_password", "", "value_Hello", client_message_id);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_ID_REQUESTED);
}

// 未提供消息内容，返回错误码 CONTEXT_REQUIRED
TEST_F(ServiceMessageSendMessage, message_context_empty) {
    string client_message_id = Utils::generate_message_id();
    auto [status, response] = SendMessage("value_user_id", "value_password", "value_target_user_id", "", client_message_id);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::CONTEXT_REQUIRED);
}

// 未提供客户端消息 ID，返回错误码 CLIENT_MESSAGE_ID_REQUIRED
TEST_F(ServiceMessageSendMessage, client_message_id_missing) {
    auto [status, response] = SendMessage("value_user_id", "value_password", "value_target_user_id", "value_Hello", "");
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::CLIENT_MESSAGE_ID_REQUIRED);
}

// 未提供认证信息，返回错误码 AUTH_INFO_NONE
TEST_F(ServiceMessageSendMessage, auth_info_missing) {
    string client_message_id = Utils::generate_message_id();
    auto [status, response] = SendMessage("", "", "value_target_user_id", "value_Hello", client_message_id);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::AUTH_ID_EMPTY);
}

// 错误的用户 ID(认证信息)，返回错误码 USER_ID_NOT_FOUND
TEST_F(ServiceMessageSendMessage, auth_failed) {
    string client_message_id = Utils::generate_message_id();
    auto [status, response] = SendMessage("value_user_id", "value_wrong_password", "value_target_user_id", "value_Hello", client_message_id);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_NOT_FOUND);
}

// 目标用户不存在，返回错误码 TARGET_USER_ID_NOT_FOUND
TEST_F(ServiceMessageSendMessage, target_user_id_not_found) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 发送消息
    string client_message_id = Utils::generate_message_id();
    auto [status, response] = SendMessage("value_user_id", "value_password", "value_nonexistent_user_id", "value_Hello", client_message_id);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_USER_ID_NOT_FOUND);
}

// 目标用户存在但不是好友关系，返回非好友状态的错误码 (非 ErrorCode::ASSOCIATED)
TEST_F(ServiceMessageSendMessage, target_user_id_not_contact) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 好友注册
    auto [status_reciver, response_reciver] = SignUp("value_receiver_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_reciver.ok());
    EXPECT_EQ(response_reciver.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_reciver.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_reciver.account_id()) << std::endl;
    auto receiver_account_id = response_reciver.account_id();

    // 发送消息
    string client_message_id = Utils::generate_message_id();
    auto [status, response] = SendMessage("value_user_id", "value_password", "value_receiver_user_id", "value_Hello", client_message_id);
    EXPECT_TRUE(status.ok());
    EXPECT_NE(response.result().error_code(), ErrorCode::ASSOCIATED); // 不是好友关系
}

// 目标用户存在，成功发送消息，返回成功码 SUCCESS
TEST_F(ServiceMessageSendMessage, send_message_success) {
    // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 好友注册
    auto [status_reciver, response_reciver] = SignUp("value_receiver_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_reciver.ok());
    EXPECT_EQ(response_reciver.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_reciver.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_reciver.account_id()) << std::endl;
    auto receiver_account_id = response_reciver.account_id();

    // 申请添加好友
    auto [status_request, response_request] = ContactRequest("value_user_id", "value_password", "value_receiver_user_id");
    EXPECT_TRUE(status_request.ok());
    EXPECT_EQ(response_request.result().success_code(), SuccessCode::SUCCESS);

    // 同意添加好友的申请
    auto [status_reply, response_reply] = ContactReply("value_receiver_user_id", "value_password", "value_user_id", ContactReplyCode::CONTACT_REPLY_ACCEPT);
    EXPECT_TRUE(status_reply.ok());
    EXPECT_EQ(response_reply.result().success_code(), SuccessCode::SUCCESS);

    // 发送消息
    string client_message_id = Utils::generate_message_id();
    auto [status, response] = SendMessage("value_user_id", "value_password", "value_receiver_user_id", "value_Hello", client_message_id);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);  // 成功发送消息
}

// 目标用户已经是好友，重复发送同一条消息，返回错误码 OPERATION_REPEAT(重复的操作)
TEST_F(ServiceMessageSendMessage, send_message_repeat) {
    // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 好友注册
    auto [status_reciver, response_reciver] = SignUp("value_receiver_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_reciver.ok());
    EXPECT_EQ(response_reciver.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_reciver.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_reciver.account_id()) << std::endl;
    auto receiver_account_id = response_reciver.account_id();

    // 申请添加好友
    auto [status_request, response_request] = ContactRequest("value_user_id", "value_password", "value_receiver_user_id");
    EXPECT_TRUE(status_request.ok());
    EXPECT_EQ(response_request.result().success_code(), SuccessCode::SUCCESS);

    // 同意添加好友的申请
    auto [status_reply, response_reply] = ContactReply("value_receiver_user_id", "value_password", "value_user_id", ContactReplyCode::CONTACT_REPLY_ACCEPT);
    EXPECT_TRUE(status_reply.ok());
    EXPECT_EQ(response_reply.result().success_code(), SuccessCode::SUCCESS);

    // 发送消息
    string client_message_id = Utils::generate_message_id();
    auto [status, response] = SendMessage("value_user_id", "value_password", "value_receiver_user_id", "value_Hello", client_message_id);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);  // 成功发送消息

    // 再次发送消息
    auto [status_repeat, response_repeat] = SendMessage("value_user_id", "value_password", "value_receiver_user_id", "value_Hello", client_message_id);
    EXPECT_TRUE(status_repeat.ok());
    EXPECT_EQ(response_repeat.result().error_code(), ErrorCode::DUPLICATE_MESSAGE_ID); // 消息重复发送
}
