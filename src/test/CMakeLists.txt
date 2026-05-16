include(GoogleTest)

# set(
#     TEST_TARGETS
#         test_helloworld
#         # test_hash
# )
# foreach(TEST_ITEM ${TEST_TARGETS})
#     add_executable(${TEST_ITEM}
#         ${TEST_ITEM}.cc
#     )
#     target_link_libraries(${TEST_ITEM}
#         PRIVATE
#             GTest::gtest_main
#     )
#     # if(TEST_ITEM STREQUAL "test_hash")
#     #     target_link_libraries(${TEST_ITEM}
#     #         PRIVATE
#     #             unofficial::hash-library
#     #     )
#     # endif()
#     gtest_discover_tests(${TEST_ITEM})
# endforeach()

set(
    TEST_TARGETS
        test_hash                            # 哈希函数测试
        test_service_status_getstatus        # 获取用户在线状态 # 未实现实际功能
        test_service_account_signup          # 账号注册
        test_service_account_signin          # 账号登录
        # test_service_account_signout       # 账号登出
        # test_service_account_deactivate    # 账号停用
        test_service_search_user             # 搜索用户
        test_service_account_contactrequest  # 好友申请
        test_service_account_contactreply    # 好友申请回复
        # test_service_account_removecontact # 删除好友关系
        test_service_message_sendmessage     # 发送消息
        test_service_account_updateprofile   # 更新个人资料
        test_service_account_getsecurityquestion # 获取安全问题
        test_service_account_updatesecurityinfo  # 更新安全信息（密码/密保问题和答案）
        test_service_file                    # 文件上传下载测试
        test_service_sync_pts                # 消息/事件同步
)
foreach(TEST_ITEM ${TEST_TARGETS})
    add_executable(${TEST_ITEM}
        "${PROJECT_DIR_SOURCE}/utils/utils.cpp"
        "${PROJECT_DIR_TEST}/test_headers.h"
        "${PROJECT_DIR_TEST}/${TEST_ITEM}.cc"
    )
    target_link_libraries(${TEST_ITEM}
        PRIVATE
            GTest::gtest_main
            fmt::fmt
            spdlog::spdlog
            ${SQLCIPHER_TARGET}
            nlohmann_json::nlohmann_json
            Boost::filesystem
            Boost::system
            Boost::uuid
            unofficial::hash-library
            protobuf::libprotoc
            protobuf::libprotobuf
            protobuf::libprotobuf-lite
            gRPC::gpr
            gRPC::grpc
            gRPC::grpc++
            gRPC::grpc++_alts
            ${PROJECT_LIB_PROTO_NAME}
    )
    gtest_discover_tests(${TEST_ITEM})
endforeach()
