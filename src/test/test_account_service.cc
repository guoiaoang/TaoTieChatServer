#include "gtest/gtest.h"

#include "AccountService.h"

TEST(AccountServiceTest, SignUpReturnsInternalWhenRequestIsNull) {
    AccountServiceImpl service;
    grpc::ServerContext context;
    SignUpReply response;

    const auto status = service.SignUp(&context, nullptr, &response);

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_EQ(status.error_message(), "Invalid request or response pointer");
}

TEST(AccountServiceTest, SignInReturnsInternalWhenResponseIsNull) {
    AccountServiceImpl service;
    grpc::ServerContext context;
    SignInRequest request;

    const auto status = service.SignIn(&context, &request, nullptr);

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_EQ(status.error_message(), "Invalid request or response pointer");
}