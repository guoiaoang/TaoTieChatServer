cmake_minimum_required(VERSION 3.10)

# 设置项目名称
set(PROJECT_NAME TaoTieChatServer)

# 项目初始化
project(${PROJECT_NAME}
    LANGUAGES CXX
)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD          20  )
# 要求使用指定的 C++ 标准
set(CMAKE_CXX_STANDARD_REQUIRED True)

# MSVC 编译器特定设置
if(MSVC)
    # add_definitions(-D_WIN32_WINNT=0x600)
    add_compile_definitions(_WIN32_WINNT=0x600)
    # 强制编译器以 UTF-8 编码读取所有源文件
    add_compile_options(/utf-8)
endif()

# 定义源文件列表
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/headers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/AccountService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/SearchService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/StatusService.h
)

add_compile_definitions(ENABLE_SERVER=ON)
# add_compile_definitions(ENABLE_SPDLOG=ON)
# add_compile_definitions(ENABLE_GOOGLE_GRPC=ON)

# 查找依赖的包
find_package(absl          CONFIG REQUIRED)
find_package(fmt           CONFIG REQUIRED)
find_package(Protobuf      CONFIG REQUIRED)
find_package(gRPC          CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(sqlcipher     CONFIG REQUIRED)
find_package(glog          CONFIG REQUIRED)
find_package(spdlog        CONFIG REQUIRED)
find_package(Boost         CONFIG REQUIRED COMPONENTS uuid)

# 添加源码目录，便于 #include 引用
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/data)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/service)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/utils)

# 主可执行程序
add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
)

# 链接依赖库
target_link_libraries(${PROJECT_NAME} PRIVATE absl::any absl::log absl::base absl::bits)  # 需要根据实际使用的 absl 模块调整
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)
# target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt-header-only)
target_link_libraries(${PROJECT_NAME} PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)
target_link_libraries(${PROJECT_NAME} PRIVATE gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME} PRIVATE sqlcipher::sqlcipher)
target_link_libraries(${PROJECT_NAME} PRIVATE glog::glog)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::uuid)
# target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog_header_only)

# ======================= 生成 gRPC 和 Protobuf 代码 ======================= #

# 编译 proto 文件，生成对应的 .pb.h 和 .pb.cc 文件的函数
include(${PROJECT_SOURCE_DIR}/src/cmake/proto_compile_with_grpc.cmake)

# 设置 proto 文件路径和生成的头文件目录
set(PROTO_FILES       "${CMAKE_CURRENT_SOURCE_DIR}/src/protos/taotie_chat.proto")
set(PROTO_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/protos")

# 调用函数编译 proto 文件，并指定输出目录和库名称
proto_compile_with_grpc(
    PROTO_FILES    PROTO_FILES
    OUT_DIR        ${PROTO_HEADERS_DIR}
    LIBARY_NAME    lib_proto_taotie_chat
)

# 将生成的头文件目录添加到 include 路径中，以便在代码中引用生成的 .pb.h 文件
include_directories(${PROTO_HEADERS_DIR})

# ========================================================================= #

# 链接生成的 proto 库
target_link_libraries(${PROJECT_NAME} PRIVATE lib_proto_taotie_chat)

# 添加测试子目录
subdirs(src/test)
