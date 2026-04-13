#include "headers.h"
#include "utils.h"

using std::string;
using std::map;
using std::set;

using ::grpc::StatusCode;

using ::TaoTieChat::StatusRequest;
using ::TaoTieChat::StatusReply;
using ::TaoTieChat::SuccessCode;
using ::TaoTieChat::ErrorCode;

using Utils::success_status;
using Utils::error_status;
using Utils::log_grpc_context;

// 服务端实现
class StatusServiceImpl final : public StatusService::Service {
public:
    // GetStatus 返回给定请求的状态信息。
    // 暂不校验是否有查询权限
    Status GetStatus(ServerContext* context, const StatusRequest* request, StatusReply* response) override {
        using Request = StatusRequest;
        using Reply   = StatusReply;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        // 执行注册
        shared_ptr<Reply> result = db_sqlcipher.get_status(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("GetStatus failed."));
        }
        // set_success(response, SuccessCode::SUCCESS);
        response->CopyFrom(*result);
        return Status::OK;
    }
};
