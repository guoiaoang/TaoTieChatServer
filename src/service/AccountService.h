#include "headers.h"
#include "utils.h"
#include "DBSQLCipher.h"
#include "types.h"

using ::TaoTieChat::AccountService;
using ::TaoTieChat::SignUpRequest;
using ::TaoTieChat::SignUpReply;
using ::TaoTieChat::SignInRequest;
using ::TaoTieChat::SignInReply;

// 服务端实现
class AccountServiceImpl final : public AccountService::Service {
public:
    // UserRegister 返回给定请求的注册信息。
    Status SignUp(ServerContext* context, const SignUpRequest* request, SignUpReply* response) override {
        using Request = ::TaoTieChat::SignUpRequest;
        using Reply   = ::TaoTieChat::SignUpReply;
        using sc      = ::TaoTieChat::SuccessCode;
        using ec      = ::TaoTieChat::ErrorCode;

        // reply 不是使用智能指针创建的，此处只能使用裸指针，不能使用智能指针，否则会被智能指针再次析构
        auto set_success = [&](Reply* reply, sc code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_success_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
            reply->set_account_id("");
        };
        auto set_error = [&](Reply* reply, ec code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_error_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
            reply->set_account_id("");
        };

        // ================================================================== //

        // 打印客户端 IP 地址
        // “ipv4:127.0.0.1:11669”
        std::string client_peer = context->peer();
        LOG_TRACE("SignUp peer: {}", client_peer.data());
        // 打印元数据
        // "accept-encoding" = "identity,gzip"
        // "service-name" = "TaoTieChat.AccountService"
        const std::multimap<grpc::string_ref, grpc::string_ref>& metadata = context->client_metadata();
        for(const auto& item : metadata) {
            std::string key(item.first.data(), item.first.size());  // 直接打印会导致长度越界
            std::string value(item.second.data(), item.second.size());
            LOG_TRACE("SignUp metadata: {} = {}", key, value);
        }
        // SSL 证书验证
        std::shared_ptr<const grpc::AuthContext> auth_ctx = context->auth_context();
        if (auth_ctx->IsPeerAuthenticated()) {
            std::vector<grpc::string_ref> peers = auth_ctx->GetPeerIdentity();
            for (auto it = peers.begin(); it != peers.end(); ++it) {
            std::string value(it->data(), it->size());
                LOG_TRACE("SignUp peer identity: {}", value);
            }
        }

        if(!context || !request || !response) {
            return Status(::grpc::StatusCode::INTERNAL, "Invalid request or response pointer");
        }

        // ================================================================== //

        // 复制请求
        response->mutable_request()->CopyFrom(*request);  // 获取可变的请求字段并复制内容

        // 用户 ID 不能为空
        if(request->user_id().empty()) {
            set_error(response, ec::SIGN_UP_USER_ID_EMPTY);
            LOG_DEBUG("User ID is empty, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
            return Status::OK;
        }

        // 密码不能为空
        if(request->password().empty()) {
            set_error(response, ec::SIGN_UP_PASSWORD_EMPTY);
            LOG_DEBUG("Password is empty, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
            return Status::OK;
        }

        // 密保问题不能为空
        if(request->security_question().empty()) {
            set_error(response, ec::SIGN_UP_SECURITY_QUESTION_EMPTY);
            LOG_DEBUG("Security question is empty, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
            return Status::OK;
        }

        // 密保答案不能为空
        if(request->security_answer().empty()) {
            set_error(response, ec::SIGN_UP_SECURITY_ANSWER_EMPTY);
            LOG_DEBUG("Security answer is empty, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
            return Status::OK;
        }

        // 参数合法，执行注册
        // 初始化相关参数
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        std::shared_ptr<::TaoTieChat::SignUpRequest> params = std::make_shared<::TaoTieChat::SignUpRequest>(*request);
        // 执行注册
        std::shared_ptr<::TaoTieChat::SignUpReply> result = db_sqlcipher.sign_up(params);
        if(!result) {
            set_error(response, ec::DB_EXECUTE);
            LOG_DEBUG("SignUp failed, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
            return Status::OK;
        }
        // set_success(response, sc::SUCCESS);
        response->CopyFrom(*result);
        return Status::OK;
    }

    // SignIn 验证用户登录信息并返回登录结果。
    Status SignIn(ServerContext* context, const SignInRequest* request, SignInReply* response) override {
        using Request = ::TaoTieChat::SignInRequest;
        using Reply   = ::TaoTieChat::SignInReply;
        using sc      = ::TaoTieChat::SuccessCode;
        using ec      = ::TaoTieChat::ErrorCode;

        // reply 不是使用智能指针创建的，此处只能使用裸指针，不能使用智能指针，否则会被智能指针再次析构
        auto set_success = [&](Reply*reply, sc code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_success_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
            reply->set_account_id("");
            reply->set_token("");
        };
        auto set_error = [&](Reply*reply, ec code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_error_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
            reply->set_account_id("");
            reply->set_token("");
        };

        // ================================================================== //

        if(!context || !request || !response) {
            return Status(::grpc::StatusCode::INTERNAL, "Invalid request or response pointer");
        }

        // ================================================================== //

        // 复制请求
        response->mutable_request()->CopyFrom(*request);  // 获取可变的请求字段并复制内容

        // 用户 ID 不能为空
        if(request->user_id().empty()) {
            set_error(response, ec::SIGN_UP_USER_ID_EMPTY);
            LOG_DEBUG("User ID is empty, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
            return Status::OK;
        }

        // 默认 token 登录
        // 使用 token 登录时也生成一个新的 token，用来添加登录记录
        // 校验逻辑在数据库中

        // // 密码不能为空
        // if(request->password().empty()) {
        //     set_error(response, ec::LOGIN_PASSWORD_EMPTY);
        //     LOG_DEBUG("Password is empty, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
        //     return Status::OK;
        // }

        // 参数合法，执行登录验证
        // 初始化相关参数
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        std::shared_ptr<::TaoTieChat::SignInRequest> params = std::make_shared<::TaoTieChat::SignInRequest>(*request);
        // 执行登录验证
        std::shared_ptr<::TaoTieChat::SignInReply> result = db_sqlcipher.sign_in(params);
        // if(!result) {
        //     set_error(response, ec::DB_EXECUTE);
        //     LOG_DEBUG("SignIn failed, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
        //     return Status::OK;
        // }
        // set_success(response, sc::SUCCESS);
        response->CopyFrom(*result);
        return Status::OK;
    }
};
