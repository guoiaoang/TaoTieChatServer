// #include <iostream>

// int main() {
// 	std::cout << "Hello World!" << std::endl;
// 	return EXIT_SUCCESS;
// }

// 使用 calllback 方案。
// async 方案需要自己处理 CompletionQueue 及维护状态机，过于麻烦。

// std
#include <iostream>
#include <memory>
#include <string>
#include <thread>
// absl
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/absl_log.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/globals.h"
#include "absl/strings/str_format.h"
// grpc
#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
// server
#include "taotie_chat.grpc.pb.h"

ABSL_FLAG(uint16_t, port, 50051, "Server port for the service");

using grpc::Status;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerUnaryReactor;
using grpc::CallbackServerContext;
using grpc::ServerReaderWriter;
// using grpc::ServerAsyncResponseWriter;
// using grpc::ServerCompletionQueue;
// using namespace TaoTieChat;
using TaoTieChat::StatusChecker;
using TaoTieChat::StatusRequest;
using TaoTieChat::StatusReply;

class StatusCheckerServiceImpl final : public StatusChecker::Service {
    Status GetStatus(ServerContext* context, ServerReaderWriter<StatusReply, StatusRequest>* stream) override {
		StatusRequest request;
		while (stream->Read(&request)) {
			ABSL_LOG(INFO) << "Received request with id: " << request.id() << ", token: " << request.token();

			StatusReply reply;
			reply.mutable_request()->CopyFrom(request);
			// 无内容时返回服务器状态
			if(request.id().empty() && request.token().empty()) {
				reply.mutable_success()->set_online_status(TaoTieChat::ONLINE);
				reply.mutable_success()->set_message("Server is running");
				stream->Write(reply);
			}
			// 无效 ID 时返回错误信息
			else if(request.id().empty() || request.token().empty()) {
				reply.mutable_error()->set_error_code(TaoTieChat::INVALID_ARGUMENT);
				reply.mutable_error()->set_message("Invalid request: missing id or token");
				stream->Write(reply);
			}
			// 目前无数据，直接返回未找到
			else {
				reply.mutable_error()->set_error_code(TaoTieChat::NOT_FOUND);
				reply.mutable_error()->set_message("Not found id or token");
				stream->Write(reply);
			}
		}
		return Status::OK;
	}
};

int main(int argc, char** argv) {
	absl::ParseCommandLine(argc, argv);
	absl::InitializeLog();
	absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);
	absl::SetStderrThreshold(absl::LogSeverityAtLeast::kError);

	grpc::EnableDefaultHealthCheckService(true);
	grpc::reflection::InitProtoReflectionServerBuilderPlugin();

	std::string server_address = absl::StrFormat("0.0.0.0:%d", absl::GetFlag(FLAGS_port));
	ABSL_LOG(INFO) << "Server listening on " << server_address;

	StatusCheckerServiceImpl service_status_checker;

	ServerBuilder server_builder;
	server_builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	server_builder.RegisterService(&service_status_checker);
	std::unique_ptr<Server> server = server_builder.BuildAndStart();
	server->Wait();

	return 0;
}