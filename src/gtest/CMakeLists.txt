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

set(TEST_TARGETS test_account_service test_db_sqlcipher)
foreach(TEST_ITEM ${TEST_TARGETS})
    add_executable(${TEST_ITEM}
        ${TEST_ITEM}.cc
        ../utils/utils.cpp
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
