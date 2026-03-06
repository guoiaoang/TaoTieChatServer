add_executable(ClientStatusChecker ClientStatusChecker.cpp)

target_link_libraries(ClientStatusChecker PRIVATE protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)
target_link_libraries(ClientStatusChecker PRIVATE gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts)

target_link_libraries(ClientStatusChecker PRIVATE lib_proto_taotie_chat)
