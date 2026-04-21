include(GoogleTest)

set(TEST_ITEM test_helloworld)
add_executable(${TEST_ITEM}
    ${TEST_ITEM}.cc
)
target_link_libraries(${TEST_ITEM}
    PRIVATE
        GTest::gtest_main
)
gtest_discover_tests(${TEST_ITEM})

set(
    TEST_TARGETS
        test_service_status_getstatus        # 获取用户在线状态 # 未实现实际功能
        test_service_account_signup          # 账号注册
        test_service_account_signin          # 账号登录
        # test_service_account_signout       # 账号登出
        # test_service_account_deactivate    # 账号停用
        test_service_search_users            # 搜索用户
        test_service_account_contactrequest  # 好友申请
        test_service_account_contactreply    # 好友申请回复
        # test_service_account_addcontact    # 添加好友
        # test_service_account_removecontact # 删除好友关系
        test_service_message_sendmessage     # 发送消息
        test_service_account_updateprofile   # 更新个人资料
        test_account_service
        test_db_sqlcipher
)
foreach(TEST_ITEM ${TEST_TARGETS})
    add_executable(${TEST_ITEM}
        test_headers.h
        ../utils/utils.cpp
        ${TEST_ITEM}.cc
    )
    target_link_libraries(${TEST_ITEM}
        PRIVATE
            GTest::gtest_main
            fmt::fmt
            spdlog::spdlog
            sqlcipher::sqlcipher
            nlohmann_json::nlohmann_json
            Boost::uuid
            protobuf::libprotoc
            protobuf::libprotobuf
            protobuf::libprotobuf-lite
            gRPC::gpr
            gRPC::grpc
            gRPC::grpc++
            gRPC::grpc++_alts
            lib_proto_taotie_chat
    )
    gtest_discover_tests(${TEST_ITEM})
endforeach()
