cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME TaoTieChatServer)

project(${PROJECT_NAME}
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD          20  )
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x600)
    # 强制编译器以 UTF-8 编码读取所有源文件
    add_compile_options(/utf-8)
endif()

set(SOURCE_FILES
    src/main.cpp
)

find_package(Protobuf      CONFIG REQUIRED)
find_package(gRPC          CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(sqlcipher     CONFIG REQUIRED)

add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)
target_link_libraries(${PROJECT_NAME} PRIVATE gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME} PRIVATE sqlcipher::sqlcipher)

# ======================= 生成 gRPC 和 Protobuf 代码 ======================= #

include(${PROJECT_SOURCE_DIR}/src/cmake/proto_compile_with_grpc.cmake)

set(PROTO_FILES       "${CMAKE_CURRENT_SOURCE_DIR}/src/protos/taotie_chat.proto")
set(PROTO_HEADERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/protos")

proto_compile_with_grpc(
    PROTO_FILES    PROTO_FILES
    OUT_DIR        ${PROTO_HEADERS_DIR}
    LIBARY_NAME    lib_proto_taotie_chat
)

include_directories(${PROTO_HEADERS_DIR})

# ========================================================================= #

target_link_libraries(${PROJECT_NAME} PRIVATE lib_proto_taotie_chat)

# subdirs(src/helloworld)

subdirs(src/test)
