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

struct ContactInfoFriendPayload {
    string     account_id   = string{};
    string     user_id      = string{};
    int32_t    status       = 0;
    bool       starred      = false;
    bool       silent       = false;
    string     notes        = string{};
    // string  added_source = string{};
    // string  added_time   = string{};
    vector<string> tags     = {};
    bool operator==(const ContactInfoFriendPayload& other) const {
        if(account_id != other.account_id) { return false; }
        if(user_id    != other.user_id   ) { return false; }
        if(status     != other.status    ) { return false; }
        if(starred    != other.starred   ) { return false; }
        if(silent     != other.silent    ) { return false; }
        if(notes      != other.notes     ) { return false; }
        if(tags       != other.tags      ) { return false; }
        return true;
    }
};

struct ContactInfoPayload {
    string user_id   = string{ };  // 要修改为的用户 ID，请求的用户 ID 已经在认证信息里面了
    string avatar_id = string{ };
    string nickname  = string{ };
    int    gender    = int   {0};
    string birthday  = string{ };
    string sign      = string{ };
    string region    = string{ };
    string desc      = string{ };
    vector<string> phones       = { };
    vector<string> emails       = { };
    vector<string> social_links = { };
    // 修改自己资料时的目标 ID，没有该值时且没有好友信息时认为无效操作，有该值才修改自身资料
    std::shared_ptr<ContactInfoFriendPayload> friend_info = nullptr;  // 修改的好友资料
    TaoTieChat::ContactInfo to_ContactInfo() const {
        TaoTieChat::ContactInfo contact_info;
        auto* base_info = contact_info.mutable_base_info();
        if(friend_info) {
            auto* friend_info_proto = contact_info.mutable_friend_info();
            friend_info_proto->set_account_id(friend_info->account_id);
            friend_info_proto->set_user_id   (friend_info->user_id   );
            friend_info_proto->set_status    (friend_info->status    );
            friend_info_proto->set_starred   (friend_info->starred   );
            friend_info_proto->set_silent    (friend_info->silent    );
            friend_info_proto->set_notes     (friend_info->notes     );
            for(const auto& tag : friend_info->tags) { *friend_info_proto->add_tags() = tag; }
        }
        else {
            base_info->set_user_id  (user_id  );
            base_info->set_avatar_id(avatar_id);
            base_info->set_nickname (nickname );
            base_info->set_gender   (gender   );
            base_info->set_birthday (birthday );
            base_info->set_sign     (sign     );
            base_info->set_region   (region   );
            base_info->set_desc     (desc     );
            for(const auto& phone       : phones      ) { *base_info->add_phones      () = phone      ; }
            for(const auto& email       : emails      ) { *base_info->add_emails      () = email      ; }
            for(const auto& social_link : social_links) { *base_info->add_social_links() = social_link; }
            contact_info.mutable_self_info()->set_modify_self_info(true);  // 修改个人信息标志
        }
        return contact_info;
    }
    bool from_ContactInfo(const TaoTieChat::ContactInfo& contact_info) {
        if(contact_info.has_base_info()) {
            const auto& base_info = contact_info.base_info();
            // account_id = base_info.account_id();
            user_id    = base_info.user_id  ();
            avatar_id  = base_info.avatar_id();
            nickname   = base_info.nickname ();
            gender     = base_info.gender   ();
            birthday   = base_info.birthday ();
            sign       = base_info.sign     ();
            region     = base_info.region   ();
            desc       = base_info.desc     ();
            phones.clear();
            for(const auto& phone : base_info.phones()) { phones.push_back(phone); }
            emails.clear();
            for(const auto& email : base_info.emails()) { emails.push_back(email); }
            social_links.clear();
            for(const auto& social_link : base_info.social_links()) { social_links.push_back(social_link); }
        }
        if(contact_info.has_friend_info()) {
            const auto& friend_info_proto = contact_info.friend_info();
            std::shared_ptr<ContactInfoFriendPayload> friend_info = std::make_shared<ContactInfoFriendPayload>();
            friend_info->account_id  = friend_info_proto.account_id();
            friend_info->user_id     = friend_info_proto.user_id   ();
            friend_info->status      = friend_info_proto.status    ();
            friend_info->starred     = friend_info_proto.starred   ();
            friend_info->silent      = friend_info_proto.silent    ();
            friend_info->notes       = friend_info_proto.notes     ();
            for(const auto& tag : friend_info_proto.tags()) { friend_info->tags.push_back(tag); }
            this->friend_info = friend_info;
        }
        return true;
    }
    bool operator==(const ContactInfoPayload& other) const {
        if(user_id   != other.user_id  ) { return false; }
        if(avatar_id != other.avatar_id) { return false; }
        if(nickname  != other.nickname ) { return false; }
        if(gender    != other.gender   ) { return false; }
        if(birthday  != other.birthday ) { return false; }
        if(sign      != other.sign     ) { return false; }
        if(region    != other.region   ) { return false; }
        if(desc      != other.desc     ) { return false; }
        if(phones       != other.phones      ) { return false; }
        if(emails       != other.emails      ) { return false; }
        if(social_links != other.social_links) { return false; }
        // 只有一个为空
        if((friend_info && other.friend_info ) == 0 && (friend_info || other.friend_info) != 0) { return false; }
        if(friend_info  && other.friend_info ) {
            if(*friend_info != *other.friend_info) { return false; }
        }
        return true;
    }
};

// 更新用户资料测试
std::tuple<grpc::Status, UpdateProfileReply> UpdateProfile(
    const string& request_user_id   ,
    const string& request_password  ,
    const string& request_account_id,
    const string& request_token     ,
    const ContactInfoPayload& contact_info_payload
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

    request.mutable_contact_info()->CopyFrom(contact_info_payload.to_ContactInfo());

    // 构造响应对象
    UpdateProfileReply response;

    // 调用 UpdateProfile 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.UpdateProfile(&context, &request, &response);
    return {status, response};
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
    request.mutable_request_user_auth()->set_user_id (request_user_id );    // 搜索请求用户 ID
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

// ============================== 修改自己资料 ============================== //

// 测试 AccountService 的 UpdateProfile 方法

// 未提供认证信息，返回错误码 AUTH_ID_EMPTY (没有 optional 修饰，不会返回 AUTH_INFO_NONE)
TEST_F(ServiceAccountUpdateProfile, auth_info_missing) {
    auto [status, response] = UpdateProfile("", "", "", "", ContactInfoPayload{});
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::AUTH_ID_EMPTY);
}

// 错误的用户 ID，返回错误码 USER_ID_NOT_FOUND
TEST_F(ServiceAccountUpdateProfile, auth_failed) {
    auto [status, response] = UpdateProfile("value_user_id", "value_password", "", "", ContactInfoPayload{});
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_NOT_FOUND);
}

// 没有目标用户 ID，返回错误码 TARGET_USER_ID_EMPTY
TEST_F(ServiceAccountUpdateProfile, target_user_id_empty) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    ContactInfoPayload contact_info_payload = ContactInfoPayload{
        .user_id   = ""               ,
        .avatar_id = "value_avatar_id",
        .nickname  = "value_nickname" ,
        .gender    = 1                ,
        .birthday  = "value_birthday" ,
        .sign      = "value_sign"     ,
        .region    = "value_region"   ,
        .desc      = "value_desc"     ,
        .phones       = { "010-12345", "+86 18812341234" },
        .emails       = { "example@example.com" },
        .social_links = { "QQ:12345678" , "WeChat:example" , "Weibo:example" },
        // .friend_info = std::make_shared<ContactInfoFriendPayload>(ContactInfoFriendPayload{
        //     .user_id = "value_user_id",
        //     .status  = 1,
        //     .starred = false,
        //     .silent  = false,
        //     .notes   = "value_notes",
        //     .tags    = { "value_tag1", "value_tag2" },
        // }),
    };
    auto [status, response] = UpdateProfile("value_user_id", "value_password", "", "", contact_info_payload);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_USER_ID_EMPTY);
}

// 不修改用户 ID，成功修改用户资料，返回成功码 SUCCESS
TEST_F(ServiceAccountUpdateProfile, target_user_id_not_changed) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    ContactInfoPayload contact_info_payload = ContactInfoPayload{
        .user_id   = "value_user_id"  ,
        .avatar_id = "value_avatar_id",
        .nickname  = "value_nickname" ,
        .gender    = 1                ,
        .birthday  = "value_birthday" ,
        .sign      = "value_sign"     ,
        .region    = "value_region"   ,
        .desc      = "value_desc"     ,
        .phones       = { "010-12345", "+86 18812341234" },
        .emails       = { "example@example.com" },
        .social_links = { "QQ:12345678" , "WeChat:example" , "Weibo:example" },
        // .friend_info = std::make_shared<ContactInfoFriendPayload>(ContactInfoFriendPayload{
        //     .user_id = "value_user_id",
        //     .status  = 1,
        //     .starred = false,
        //     .silent  = false,
        //     .notes   = "value_notes",
        //     .tags    = { "value_tag1", "value_tag2" },
        // }),
    };
    auto [status, response] = UpdateProfile("value_user_id", "value_password", "", "", contact_info_payload);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);

    // 获取用户资料，验证修改后的用户 ID 和其他资料是否正确
    auto [status_search_user, response_search_user] = SearchUser("value_user_id", "value_password", "value_user_id");
    EXPECT_TRUE(status_search_user.ok());
    EXPECT_EQ(response_search_user.result().success_code(), SuccessCode::SUCCESS);
    ContactInfoPayload actual_contact_info_payload;
    EXPECT_TRUE(actual_contact_info_payload.from_ContactInfo(response_search_user.contact_info()));
    EXPECT_EQ(contact_info_payload, actual_contact_info_payload);
}

// 修改用户 ID，成功修改用户资料，返回成功码 SUCCESS
TEST_F(ServiceAccountUpdateProfile, target_user_id_changed) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    ContactInfoPayload contact_info_payload = ContactInfoPayload{
        .user_id   = "value_user_id_changed"  ,
        .avatar_id = "value_avatar_id",
        .nickname  = "value_nickname" ,
        .gender    = 1                ,
        .birthday  = "value_birthday" ,
        .sign      = "value_sign"     ,
        .region    = "value_region"   ,
        .desc      = "value_desc"     ,
        .phones       = { "010-12345", "+86 18812341234" },
        .emails       = { "example@example.com" },
        .social_links = { "QQ:12345678" , "WeChat:example" , "Weibo:example" },
        // .friend_info = std::make_shared<ContactInfoFriendPayload>(ContactInfoFriendPayload{
        //     .user_id = "value_user_id",
        //     .status  = 1,
        //     .starred = false,
        //     .silent  = false,
        //     .notes   = "value_notes",
        //     .tags    = { "value_tag1", "value_tag2" },
        // }),
    };
    auto [status, response] = UpdateProfile("value_user_id", "value_password", "", "", contact_info_payload);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);

    // 获取用户资料，验证修改后的用户 ID 和其他资料是否正确
    auto [status_search_user, response_search_user] = SearchUser("value_user_id_changed", "value_password", "value_user_id_changed");
    EXPECT_TRUE(status_search_user.ok());
    EXPECT_EQ(response_search_user.result().success_code(), SuccessCode::SUCCESS);
    ContactInfoPayload actual_contact_info_payload;
    EXPECT_TRUE(actual_contact_info_payload.from_ContactInfo(response_search_user.contact_info()));
    EXPECT_EQ(contact_info_payload, actual_contact_info_payload);
}

// 修改用户 ID 后重复执行，返回错误码 USER_ID_NOT_FOUND
TEST_F(ServiceAccountUpdateProfile, target_user_id_changed_again) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    ContactInfoPayload contact_info_payload = ContactInfoPayload{
        .user_id   = "value_user_id_changed"  ,
        .avatar_id = "value_avatar_id",
        .nickname  = "value_nickname" ,
        .gender    = 1                ,
        .birthday  = "value_birthday" ,
        .sign      = "value_sign"     ,
        .region    = "value_region"   ,
        .desc      = "value_desc"     ,
        .phones       = { "010-12345", "+86 18812341234" },
        .emails       = { "example@example.com" },
        .social_links = { "QQ:12345678" , "WeChat:example" , "Weibo:example" },
        // .friend_info = std::make_shared<ContactInfoFriendPayload>(ContactInfoFriendPayload{
        //     .user_id = "value_user_id",
        //     .status  = 1,
        //     .starred = false,
        //     .silent  = false,
        //     .notes   = "value_notes",
        //     .tags    = { "value_tag1", "value_tag2" },
        // }),
    };
    auto [status, response] = UpdateProfile("value_user_id", "value_password", "", "", contact_info_payload);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);

    // 再次以相同的认证信息修改用户资料，应该找不到用户了，因为用户 ID 已经被修改了
    auto [status_update_profile_again, response_update_profile_again] = UpdateProfile("value_user_id", "value_password", "", "", contact_info_payload);
    EXPECT_TRUE(status_update_profile_again.ok());
    EXPECT_EQ(response_update_profile_again.result().error_code(), ErrorCode::USER_ID_NOT_FOUND);

    // 获取用户资料，验证修改后的用户 ID 和其他资料是否正确
    auto [status_search_user, response_search_user] = SearchUser("value_user_id_changed", "value_password", "value_user_id_changed");
    EXPECT_TRUE(status_search_user.ok());
    EXPECT_EQ(response_search_user.result().success_code(), SuccessCode::SUCCESS);
    ContactInfoPayload actual_contact_info_payload;
    EXPECT_TRUE(actual_contact_info_payload.from_ContactInfo(response_search_user.contact_info()));
    EXPECT_EQ(contact_info_payload, actual_contact_info_payload);
}

// 目标用户 ID 已经存在，返回错误码 TARGET_USER_ID_EXISTS
TEST_F(ServiceAccountUpdateProfile, target_user_id_exists) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    auto [status_signup_target, response_signup_target] = SignUp("value_target_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup_target.ok());
    EXPECT_EQ(response_signup_target.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup_target.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered target account_id: {}", response_signup_target.account_id()) << std::endl;
    auto target_account_id = response_signup_target.account_id();

    // 修改用户 ID 之前的信息，便于比较
    ContactInfoPayload contact_info_payload = ContactInfoPayload{
        .user_id   = "value_user_id"  ,
        .avatar_id = "value_avatar_id",
        .nickname  = "value_nickname" ,
        .gender    = 1                ,
        .birthday  = "value_birthday" ,
        .sign      = "value_sign"     ,
        .region    = "value_region"   ,
        .desc      = "value_desc"     ,
        .phones       = { "010-12345", "+86 18812341234" },
        .emails       = { "example@example.com" },
        .social_links = { "QQ:12345678" , "WeChat:example" , "Weibo:example" },
        // .friend_info = std::make_shared<ContactInfoFriendPayload>(ContactInfoFriendPayload{
        //     .user_id = "value_user_id",
        //     .status  = 1,
        //     .starred = false,
        //     .silent  = false,
        //     .notes   = "value_notes",
        //     .tags    = { "value_tag1", "value_tag2" },
        // }),
    };
    auto [status, response] = UpdateProfile("value_user_id", "value_password", "", "", contact_info_payload);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);

    // 尝试修改用户 ID 为已经存在的用户 ID
    ContactInfoPayload contact_info_payload_target = ContactInfoPayload{
        .user_id   = "value_target_user_id"  ,
        .avatar_id = "value_avatar_id",
        .nickname  = "value_nickname" ,
        .gender    = 1                ,
        .birthday  = "value_birthday" ,
        .sign      = "value_sign"     ,
        .region    = "value_region"   ,
        .desc      = "value_desc"     ,
        .phones       = { "010-12345", "+86 18812341234" },
        .emails       = { "example@example.com" },
        .social_links = { "QQ:12345678" , "WeChat:example" , "Weibo:example" },
        // .friend_info = std::make_shared<ContactInfoFriendPayload>(ContactInfoFriendPayload{
        //     .user_id = "value_user_id",
        //     .status  = 1,
        //     .starred = false,
        //     .silent  = false,
        //     .notes   = "value_notes",
        //     .tags    = { "value_tag1", "value_tag2" },
        // }),
    };
    auto [status_target, response_target] = UpdateProfile("value_user_id", "value_password", "", "", contact_info_payload_target);
    EXPECT_TRUE(status_target.ok());
    EXPECT_EQ(response_target.result().error_code(), ErrorCode::TARGET_USER_ID_EXISTS);

    // 验证资料应该未修改
    auto [status_search_user, response_search_user] = SearchUser("value_user_id", "value_password", "value_user_id");
    EXPECT_TRUE(status_search_user.ok());
    EXPECT_EQ(response_search_user.result().success_code(), SuccessCode::SUCCESS);
    ContactInfoPayload actual_contact_info_payload;
    EXPECT_TRUE(actual_contact_info_payload.from_ContactInfo(response_search_user.contact_info()));
    EXPECT_EQ(contact_info_payload, actual_contact_info_payload);
}

// ============================== 修改好友资料 ============================== //

// 修改好友资料，成功修改好友资料，返回成功码 SUCCESS
TEST_F(ServiceAccountUpdateProfile, friend_info) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 自己可以申请添加自己为好友
    auto [status_request, response_request] = ContactRequest("value_user_id", "value_password", "value_user_id");
    EXPECT_TRUE(status_request.ok());
    EXPECT_EQ(response_request.result().success_code(), SuccessCode::SUCCESS);

    // 同意添加好友的申请
    auto [status_reply, response_reply] = ContactReply("value_user_id", "value_password", "value_user_id", ContactReplyCode::CONTACT_REPLY_ACCEPT);
    EXPECT_TRUE(status_reply.ok());
    EXPECT_EQ(response_reply.result().success_code(), SuccessCode::SUCCESS);

    // 先修改好友的基础资料，便于比较
    ContactInfoPayload contact_info_payload = ContactInfoPayload{
        .user_id   = "value_user_id"  ,
        .avatar_id = "value_avatar_id",
        .nickname  = "value_nickname" ,
        .gender    = 1                ,
        .birthday  = "value_birthday" ,
        .sign      = "value_sign"     ,
        .region    = "value_region"   ,
        .desc      = "value_desc"     ,
        .phones       = { "010-12345", "+86 18812341234" },
        .emails       = { "example@example.com" },
        .social_links = { "QQ:12345678" , "WeChat:example" , "Weibo:example" },
        // .friend_info = std::make_shared<ContactInfoFriendPayload>(ContactInfoFriendPayload{
        //     .user_id = "value_user_id",
        //     .status  = 1,
        //     .starred = false,
        //     .silent  = false,
        //     .notes   = "value_notes",
        //     .tags    = { "value_tag1", "value_tag2" },
        // }),
    };
    auto [status, response] = UpdateProfile("value_user_id", "value_password", "", "", contact_info_payload);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);

    // 修改好友关系中的资料
    ContactInfoPayload contact_info_payload_friend = ContactInfoPayload{
        // .user_id   = "value_user_id"  ,
        // .avatar_id = "value_avatar_id",
        // .nickname  = "value_nickname" ,
        // .gender    = 1                ,
        // .birthday  = "value_birthday" ,
        // .sign      = "value_sign"     ,
        // .region    = "value_region"   ,
        // .desc      = "value_desc"     ,
        // .phones       = { "010-12345", "+86 18812341234" },
        // .emails       = { "example@example.com" },
        // .social_links = { "QQ:12345678" , "WeChat:example" , "Weibo:example" },
        .friend_info = std::make_shared<ContactInfoFriendPayload>(ContactInfoFriendPayload{
            .user_id = "value_user_id",
            .status  = 1,
            .starred = false,
            .silent  = false,
            .notes   = "value_notes",
            .tags    = { "value_tag1", "value_tag2" },
        }),
    };
    auto [status_friend, response_friend] = UpdateProfile("value_user_id", "value_password", "", "", contact_info_payload_friend);
    EXPECT_TRUE(status_friend.ok());
    EXPECT_EQ(response_friend.result().success_code(), SuccessCode::SUCCESS);

    // 最终获取到的复合资料
    ContactInfoPayload contact_info_payload_final = ContactInfoPayload{
        .user_id   = "value_user_id"  ,
        .avatar_id = "value_avatar_id",
        .nickname  = "value_nickname" ,
        .gender    = 1                ,
        .birthday  = "value_birthday" ,
        .sign      = "value_sign"     ,
        .region    = "value_region"   ,
        .desc      = "value_desc"     ,
        .phones       = { "010-12345", "+86 18812341234" },
        .emails       = { "example@example.com" },
        .social_links = { "QQ:12345678" , "WeChat:example" , "Weibo:example" },
        .friend_info = std::make_shared<ContactInfoFriendPayload>(ContactInfoFriendPayload{
            .account_id = response_signup.account_id(),
            .user_id = "value_user_id",
            .status  = 1,
            .starred = false,
            .silent  = false,
            .notes   = "value_notes",
            .tags    = { "value_tag1", "value_tag2" },
        }),
    };

    // 获取用户资料，验证修改后的用户 ID 和其他资料是否正确
    auto [status_search_user, response_search_user] = SearchUser("value_user_id", "value_password", "value_user_id");
    EXPECT_TRUE(status_search_user.ok());
    EXPECT_EQ(response_search_user.result().success_code(), SuccessCode::SUCCESS);
    std::cout << "SearchUser response : " << response_search_user.DebugString() << std::endl;
    ContactInfoPayload actual_contact_info_payload;
    EXPECT_TRUE(actual_contact_info_payload.from_ContactInfo(response_search_user.contact_info()));
    EXPECT_EQ(contact_info_payload_final, actual_contact_info_payload);
}
