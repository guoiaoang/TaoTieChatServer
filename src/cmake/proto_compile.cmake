
function(proto_compile target_name proto_files)
    find_package(Protobuf      CONFIG REQUIRED)
    find_package(gRPC          CONFIG REQUIRED)

    set(protos_headers_dir "${CMAKE_CURRENT_BINARY_DIR}/protos")
    # file(MAKE_DIRECTORY ${protos_headers_dir})
    # make_directory(${protos_headers_dir})

    message(STATUS "Proto files compiled: ${proto_files}")
    message(STATUS "Proto files compiled in directory: ${protos_headers_dir}")

    # 依次处理每一个 proto 文件，生成对应的源文件和头文件
    foreach(proto_file ${proto_files})
        get_filename_component(proto_file_path "${proto_file}" ABSOLUTE)
        get_filename_component(proto_file_dir  "${proto_file}" PATH    )
        get_filename_component(proto_file_name "${proto_file}" NAME_WE )

        set(proto_srcs "${protos_headers_dir}/${proto_file_name}.pb.cc"     )
        set(proto_hdrs "${protos_headers_dir}/${proto_file_name}.pb.h"      )
        set(grpc_srcs  "${protos_headers_dir}/${proto_file_name}.grpc.pb.cc")
        set(grpc_hdrs  "${protos_headers_dir}/${proto_file_name}.grpc.pb.h" )

        add_custom_command(
            OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
            COMMAND $<TARGET_FILE:protobuf::protoc>
            ARGS --grpc_out "${protos_headers_dir}"
                --cpp_out   "${protos_headers_dir}"
                -I          "${proto_file_dir}"
                --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
                "${proto_file}"
            DEPENDS "${proto_file}")

        set(proto_lib_name "proto_lib_${proto_file_name}")

        add_library(${proto_lib_name}
            ${proto_srcs}
            ${proto_hdrs}
            ${grpc_srcs}
            ${grpc_hdrs})

        target_link_libraries(${proto_lib_name}
            absl::check
            absl::flags
            absl::flags_parse
            absl::log
            absl::log_initialize
            ${_REFLECTION}
            ${_GRPC_GRPCPP}
            ${_PROTOBUF_LIBPROTOBUF})

        message(STATUS "proto lib name : ${proto_lib_name}")

        target_link_libraries(${target_name}
        PRIVATE
            ${proto_lib_name}
            # absl::check
            # absl::flags
            # absl::flags_parse
            # absl::log
            # absl::log_initialize
            # ${_REFLECTION}
            # ${_GRPC_GRPCPP}
            # ${_PROTOBUF_LIBPROTOBUF}
        )
    endforeach(proto_file)

    # 将生成的头文件目录添加到 include path 中，这样在代码中就可以直接包含生成的头文件了
    target_include_directories(${target_name} PRIVATE ${protos_headers_dir})
    # include_directories(${protos_headers_dir})
endfunction()

proto_compile(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src/protos/TaoTieChatServer.proto")
