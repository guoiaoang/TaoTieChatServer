#include "headers.h"

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
        // DEBUG // 记录请求日志
        // auto&target_id          = request->target_id();
        // auto&request_user_id    = request->request_user_id();
        // auto&request_user_token = request->request_user_token();
        // ABSL_LOG(INFO) << absl::StrFormat(
        //     "Received GetStatus request: target_id=%s, request_user_id=%s, request_user_token=%s",
        //     target_id, request_user_id, request_user_token);

        response->mutable_request()->CopyFrom(*request);
        // 被查询用户 id 为空时返回服务器的状态信息
        if(request->target_id().empty()) {
            response->mutable_success()->set_message("Server is running");
        }
        // 查询用户 id 不为空时要求必须有查询者的用户 ID
        else if(request->request_user_id().empty()) {
            response->mutable_error()->set_message("Request User ID is required");
        }
        // 查询用户 id 不为空时要求必须有查询者的用户 ID
        else if(request->request_user_token().empty()) {
            response->mutable_error()->set_message("Request User Token is required");
        }
        // id 和 token 都不为空时返回用户的状态信息
        else {
            auto user_tokens_it = user_tokens.find(request->request_user_id());
            // 校验用户 ID 的合法性
            if (user_tokens_it == user_tokens.end()) {
                response->mutable_error()->set_message("Invalid Request User ID");
            }
            // 校验 token 的合法性  // 同一用户可以有多个 token
            else if (user_tokens_it->second.find(request->request_user_token()) == user_tokens_it->second.end()) {
                response->mutable_error()->set_message("Invalid Request User Token");
            }
            else {
                // 查询用户 ID 和 token 都合法时返回被查询用户的状态信息
                auto it = user_status.find(request->target_id());
                if (it != user_status.end()) {
                    response->mutable_success()->set_message(absl::StrFormat("User %s is %s", request->target_id(), it->second));
                } else {
                    response->mutable_error()->set_message("Invalid target User ID");
                }
            }
        }
        return Status::OK;
    }
};
