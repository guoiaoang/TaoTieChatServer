# 必须先找到相关的包
find_package(Protobuf REQUIRED)
find_package(gRPC     REQUIRED)

if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x600)
    # 强制编译器以 UTF-8 编码读取所有源文件
    add_compile_options(/utf-8)
endif()

set(GRPC_CPP             gRPC::grpc++           )
set(GRPC_CPP_REFLECTION  gRPC::grpc++_reflection)
set(PROTOBUF_LIBPROTOBUF protobuf::libprotobuf  )

# if(CMAKE_CROSSCOMPILING)
#     find_program(_PROTOBUF_PROTOC            protoc         )
#     find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
# else()
#     set(_PROTOBUF_PROTOC            $<TARGET_FILE:protobuf::protoc>     )
#     set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
# endif()

function(proto_compile_with_grpc)
    # 参数解析
    set(options "")
    set(oneValueArgs OUT_DIR LIBARY_NAME)
    set(multiValueArgs PROTO_FILES)
    cmake_parse_arguments(PROTO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # 设置参数默认值
    if(NOT PROTO_OUT_DIR)
        set(PROTO_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/protos)
    endif()
    if(NOT PROTO_LIBARY_NAME)
        set(PROTO_LIBARY_NAME "lib_proto_${PROJECT_NAME}")
    endif()
    # 生成文件列表
    set(ALL_GENERATED_FILES "")

    # 获取 gRPC 插件路径 (通常在 gRPC::grpc_cpp_plugin 目标属性中)
    # get_target_property(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
    get_target_property(GRPC_CPP_PLUGIN_EXECUTABLE gRPC::grpc_cpp_plugin LOCATION)

    # 创建输出目录
    file(MAKE_DIRECTORY ${PROTO_OUT_DIR})

    # 调用命令依次生成 .proto 文件对应的实现
    foreach(proto_file ${PROTO_FILES})
        # 获取绝对路径、目录和不带扩展名的文件名
        get_filename_component(proto_abs  ${proto_file} ABSOLUTE)
        get_filename_component(proto_dir  ${proto_abs}  PATH    )
        get_filename_component(proto_name ${proto_file} NAME_WE )

        # 目标文件
        set(generated_files
            "${PROTO_OUT_DIR}/${proto_name}.pb.cc"
            "${PROTO_OUT_DIR}/${proto_name}.pb.h"
            "${PROTO_OUT_DIR}/${proto_name}.grpc.pb.cc"
            "${PROTO_OUT_DIR}/${proto_name}.grpc.pb.h"
        )
        # 调用 protoc 生成 （protobuf 和） gRPC 实现文件
        add_custom_command(
            OUTPUT  ${generated_files}
            COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            ARGS    --cpp_out  ${PROTO_OUT_DIR}
                    --grpc_out ${PROTO_OUT_DIR}
                    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
                    -I ${proto_dir}
                    ${proto_abs}
            DEPENDS ${proto_abs}
            COMMENT "Generating gRPC/C++ sources from ${proto_file}"
            VERBATIM
        )

        # 添加到已生成文件列表
        list(APPEND ALL_GENERATED_FILES ${generated_files})
    endforeach()

    # 封装为库
    add_library(${PROTO_LIBARY_NAME} STATIC ${ALL_GENERATED_FILES})
    # 链接必要的库
    target_link_libraries(${PROTO_LIBARY_NAME} 
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
    target_include_directories(${PROTO_LIBARY_NAME} PUBLIC ${PROTO_OUT_DIR})
endfunction()

# set(PROTO_FILES       "${CMAKE_CURRENT_SOURCE_DIR}/../protos/TaoTieChatServer.proto")
# set(PROTO_HEADERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/protos")

# proto_compile_with_grpc(
#     PROTO_FILES    PROTO_FILES
#     OUT_DIR        ${PROTO_HEADERS_DIR}
#     LIBARY_NAME    LibProtoTaoTieChatServer
# )

# target_link_libraries(ClientStatusChecker PRIVATE LibProtoTaoTieChatServer)
