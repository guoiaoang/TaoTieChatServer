# 必须先找到相关的包
find_package(Protobuf   REQUIRED)
find_package(gRPC       REQUIRED)

if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x600)
    # 强制编译器以 UTF-8 编码读取所有源文件
    add_compile_options(/utf-8)
endif()

set(GRPC_CPP               gRPC::grpc++           )
set(GRPC_CPP_REFLECTION    gRPC::grpc++_reflection)
set(PROTOBUF_LIBPROTOBUF   protobuf::libprotobuf  )

# if(CMAKE_CROSSCOMPILING)
#     find_program(_PROTOBUF_PROTOC              protoc         )
#     find_program(_GRPC_CPP_PLUGIN_EXECUTABLE   grpc_cpp_plugin)
# else()
#     set(_PROTOBUF_PROTOC              $<TARGET_FILE:protobuf::protoc>     )
#     set(_GRPC_CPP_PLUGIN_EXECUTABLE   $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
# endif()

function(proto_compile_with_grpc)
    # 参数解析
    set(options          "")
    set(oneValueArgs     OUT_DIR       LIBRARY_NAME)
    set(multiValueArgs   PROTO_FILES)
    # 解析传入的参数并添加 PAERSE 前缀 # 生成 PROTO_OUT_DIR、PROTO_LIBRARY_NAME 和 PROTO_FILES 变量
    cmake_parse_arguments(PARSED   "${options}"   "${oneValueArgs}"   "${multiValueArgs}"   ${ARGN})

    # 设置参数默认值
    if(NOT   PARSED_OUT_DIR)
        set(PARSED_OUT_DIR   ${CMAKE_CURRENT_BINARY_DIR}/protos/include)
    endif()
    if(NOT   PARSED_LIBRARY_NAME)
        set(PARSED_LIBRARY_NAME   "LibProto${PROJECT_NAME}")
    endif()
    # 生成文件列表
    set(ALL_GENERATED_FILES   "")

    # 获取 gRPC 插件路径 (通常在 gRPC::grpc_cpp_plugin 目标属性中)
    # get_target_property(GRPC_CPP_PLUGIN_EXECUTABLE   $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
    get_target_property(GRPC_CPP_PLUGIN_EXECUTABLE   gRPC::grpc_cpp_plugin   LOCATION)

    # 创建输出目录
    file(MAKE_DIRECTORY   ${PARSED_OUT_DIR})

    # 调用命令依次生成 .proto 文件对应的实现
    foreach(proto_file   ${PARSED_PROTO_FILES})
        # 获取绝对路径、目录和不带扩展名的文件名
        get_filename_component(proto_abs    "${proto_file}"   ABSOLUTE)
        get_filename_component(proto_dir    "${proto_abs}"    PATH)
        get_filename_component(proto_name   "${proto_file}"   NAME_WE)

        # 目标文件
        set(generated_files
            "${PARSED_OUT_DIR}/${proto_name}.pb.cc"
            "${PARSED_OUT_DIR}/${proto_name}.pb.h"
            "${PARSED_OUT_DIR}/${proto_name}.grpc.pb.cc"
            "${PARSED_OUT_DIR}/${proto_name}.grpc.pb.h"
        )
        # 调用 protoc 生成 （protobuf 和） gRPC 实现文件
        add_custom_command(
            OUTPUT  ${generated_files}
            COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            ARGS    --cpp_out  ${PARSED_OUT_DIR}
                    --grpc_out ${PARSED_OUT_DIR}
                    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
                    -I ${proto_dir}
                    ${proto_abs}
            DEPENDS ${proto_abs}
            COMMENT "Generating gRPC/C++ sources from ${proto_file}"
            VERBATIM
        )

        # 添加到已生成文件列表
        list(APPEND   ALL_GENERATED_FILES   ${generated_files})
    endforeach()

    # 封装为库
    add_library(${PARSED_LIBRARY_NAME}   STATIC   ${ALL_GENERATED_FILES})
    # 链接必要的库
    target_link_libraries(${PARSED_LIBRARY_NAME}
        PUBLIC
            absl::check
            absl::flags
            absl::flags_parse
            absl::log
            absl::log_initialize
            ${GRPC_CPP}
            ${GRPC_CPP_REFLECTION}
            ${PROTOBUF_LIBPROTOBUF}
    )
    # 暴露生成文件的头文件路径
    target_include_directories(${PARSED_LIBRARY_NAME}   PUBLIC   ${PARSED_OUT_DIR})
endfunction()

# set(PROTO_FILES       "${CMAKE_CURRENT_SOURCE_DIR}/../protos/TaoTieChatServer.proto")
# set(PROTO_HEADERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/protos")

# proto_compile_with_grpc(
#     PROTO_FILES    PROTO_FILES
#     OUT_DIR        ${PROTO_HEADERS_DIR}
#     LIBRARY_NAME   LibProtoTaoTieChatServer
# )

# target_link_libraries(ClientStatusChecker PRIVATE LibProtoTaoTieChatServer)
