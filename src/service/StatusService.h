#include "headers.h"
#include "utils.h"

// 用户在线状态缓存
static std::map<std::string, std::string> user_status = {
    {"user1", "online"},
    {"user2", "offline"},
    {"user3", "away"},
};
// 用户 token 缓存
static std::map<std::string, std::set<std::string>> user_tokens = {
    {"user1", {"token1", "token11"}},
    {"user2", {"token2"}},
    {"user3", {"token3"}},
};

// 服务端实现
class StatusServiceImpl final : public StatusService::Service {
public:
    // GetStatus 返回给定请求的状态信息。
    // 暂不校验是否有查询权限
    Status GetStatus(ServerContext* context, const StatusRequest* request, StatusReply* response) override {
        using Request = ::TaoTieChat::StatusRequest;
        using Reply   = ::TaoTieChat::StatusReply;
        using sc      = ::TaoTieChat::SuccessCode;
        using ec      = ::TaoTieChat::ErrorCode;

        // response 不是使用智能指针创建的，此处只能使用裸指针，不能使用智能指针，否则会被智能指针再次析构
        auto set_success = [&](Reply* reply, sc code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_success_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
        };
        auto set_error = [&](Reply* reply, ec code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_error_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
        };

        // DEBUG // 记录请求日志
        // auto&target_id          = request->target_id();
        // auto&request_user_id    = request->request_user_id();
        // auto&request_user_token = request->request_user_token();
        // ABSL_LOG(INFO) << absl::StrFormat(
        //     "Received GetStatus request: target_id=%s, request_user_id=%s, request_user_token=%s",
        //     target_id, request_user_id, request_user_token);

        if(!context || !request || !response) {
            return Status(::grpc::StatusCode::INTERNAL, "Invalid request or reply pointer");
        }

        response->mutable_request()->CopyFrom(*request);

        // 被查询用户 id 为空时返回服务器的状态信息
        if(request->target_id().empty()) {
            set_success(response, sc::STATUS_RUNNING);
            LOG_TRACE("Server is running. request:{}", request->DebugString());
            return Status::OK;
        }

        // 查询用户 id 不为空时要求必须有查询者的用户 ID
        if(request->request_user_id().empty()) {
            set_error(response, ec::STATUS_REQUEST_USER_ID_EMPTY);
            LOG_TRACE("Request user ID is empty. request:{}", request->DebugString());
            return Status::OK;
        }

        // 查询用户 id 不为空时要求必须有查询者的用户 Token
        if(request->request_user_token().empty()) {
            set_error(response, ec::STATUS_REQUEST_USER_TOKEN_EMPTY);
            LOG_TRACE("Request user token is empty. request:{}", request->DebugString());
            return Status::OK;
        }

        // id 和 token 都不为空时返回用户的状态信息
        auto user_tokens_it = user_tokens.find(request->request_user_id());
        // 校验用户 ID 的合法性
        if (user_tokens_it == user_tokens.end()) {
            set_error(response, ec::STATUS_TARGET_ID_NOT_FOUND);
            LOG_TRACE("Target ID not found. request:{}", request->DebugString());
            return Status::OK;
        }

        // 校验 token 的合法性  // 同一用户可以有多个 token
        if (user_tokens_it->second.find(request->request_user_token()) == user_tokens_it->second.end()) {
            set_error(response, ec::STATUS_USER_TOKEN_INVALID);
            LOG_TRACE("User token is invalid. request:{}", request->DebugString());
            return Status::OK;
        }

        // 查询用户 ID 和 token 都合法时返回被查询用户的状态信息
        auto it = user_status.find(request->target_id());
        if (it == user_status.end()) {
            set_error(response, ec::STATUS_TARGET_ID_NOT_FOUND);
            LOG_TRACE("Target ID not found. request:{}", request->DebugString());
            return Status::OK;
        }

        set_success(response, sc::STATUS_SUCCESS);
        response->set_status(it->second);
        LOG_INFO("Target ID {} status is {}. request:{}", request->target_id(), it->second, request->DebugString());
        return Status::OK;
    }
};
