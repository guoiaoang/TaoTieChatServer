
foreach(Client ClientGetStatus)

    # 生成可执行文件
    add_executable(${Client} ${Client}.cpp)

    # 链接 gRPC 和 protobuf 库
    target_link_libraries(${Client} PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)
    target_link_libraries(${Client} PRIVATE gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts)

    # 链接 proto 生成的库
    target_link_libraries(${Client} PRIVATE lib_proto_taotie_chat)

endforeach()
