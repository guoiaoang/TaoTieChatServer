#include "headers.h"
#include "utils.h"
#include "types.h"

#include "DBSQLCipher.h"
#include "StatusService.h"
#include "AccountService.h"
#include "SearchService.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

std::tuple<grpc::Status, StatusReply> GetStatus(const string& target_user_id,
const string& request_user_id, const string& request_user_token) {
    // 创建 StatusServiceImpl 实例
    StatusServiceImpl service;

    // 构造测试请求
    StatusRequest request;
    request.set_target_user_id(target_user_id);
    if(request_user_id.empty() && request_user_token.empty()) {
        request.clear_request_user_auth();
    }
    else {
        if(request_user_id.empty()) {
            request.mutable_request_user_auth()->clear_user_id();
        }
        else {
            request.mutable_request_user_auth()->set_user_id(request_user_id);
        }

        if(request_user_token.empty()) {
            request.mutable_request_user_auth()->clear_token();
        }
        else {
            request.mutable_request_user_auth()->set_token(request_user_token);
        }
    }

    // 构造响应对象
    StatusReply response;

    // 调用 GetStatus 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.GetStatus(&context, &request, &response);
    return {status, response};
}

// 测试 StatusService 的 GetStatus 方法
// 目标用户 ID 为空，返回服务器运行状态
TEST(StatusServiceGetStatus, target_id_empty) {
    // DBSQLCipherInit::init();
    auto [status, response] = GetStatus("", "user1", "token1");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_USER_ID_EMPTY);
}

// 请求认证信息完全为空，返回错误码 AUTH_INFO_NONE
TEST(StatusServiceGetStatus, request_auth_none) {
    // DBSQLCipherInit::init();
    auto [status, response] = GetStatus("user1", "", "");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::AUTH_INFO_NONE);
}

// 请求用户 ID 为空，返回错误码 USER_ID_EMPTY
TEST(StatusServiceGetStatus, request_user_id_empty) {
    // DBSQLCipherInit::init();
    auto [status, response] = GetStatus("user1", "", "token1");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_EMPTY);
}

// 请求用户认证信息中 Token 为空，返回错误码 TOKEN_EMPTY
TEST(StatusServiceGetStatus, request_user_token_empty) {
    // DBSQLCipherInit::init();
    auto [status, response] = GetStatus("user1", "user1", "");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TOKEN_EMPTY);
}

// 请求用户 ID 错误，返回错误码 USER_ID_NOT_FOUND
TEST(StatusServiceGetStatus, request_user_id_error) {
    // DBSQLCipherInit::init();
    auto [status, response] = GetStatus("user1", "user_", "token1");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_NOT_FOUND);
}

// 请求用户认证信息中 Token 错误，返回错误码 TOKEN_NOT_FOUND
TEST(StatusServiceGetStatus, request_user_token_error) {
    // DBSQLCipherInit::init();
    auto [status, response] = GetStatus("user1", "user1", "token_");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TOKEN_NOT_FOUND);
}

// 目标用户 ID 错误，返回错误码 TARGET_USER_ID_NOT_FOUND
TEST(StatusServiceGetStatus, target_id_error) {
    // DBSQLCipherInit::init();
    auto [status, response] = GetStatus("user_", "user1", "token1");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_USER_ID_NOT_FOUND);
}

// 请求用户认证信息正确，目标用户 ID 正确，返回目标用户状态
TEST(StatusServiceGetStatus, self_status) {
    // DBSQLCipherInit::init();
    auto [status, response] = GetStatus("user1", "user1", "token1");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::STATUS_SUCCESS);
    EXPECT_EQ(response.status(), "online");
}

// 请求用户认证信息正确，目标用户 ID 正确，返回目标用户状态
TEST(StatusServiceGetStatus, other_user_status_user2) {
    // DBSQLCipherInit::init();
    auto [status, response] = GetStatus("user2", "user1", "token1");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::STATUS_SUCCESS);
    EXPECT_EQ(response.status(), "offline");
}

// 请求用户认证信息正确，目标用户 ID 正确，返回目标用户状态
TEST(StatusServiceGetStatus, other_user_status_user3) {
    // DBSQLCipherInit::init();
    auto [status, response] = GetStatus("user3", "user1", "token11");
    // 验证返回状态
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::STATUS_SUCCESS);
    EXPECT_EQ(response.status(), "away");
}
