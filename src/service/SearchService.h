#include "headers.h"
#include "utils.h"
#include "DBSQLCipher.h"
#include "types.h"

using ::grpc::ServerContext;
using ::TaoTieChat::SearchService;
using ::TaoTieChat::SearchUsersRequest;
using ::TaoTieChat::SearchUsersReply;
using ::TaoTieChat::SearchGroupsRequest;
using ::TaoTieChat::SearchGroupsReply;

// 服务端实现
class SearchServiceImpl final : public SearchService::Service {
public:
    // 搜索用户
    Status SearchUsers(ServerContext* context, const SearchUsersRequest* request, SearchUsersReply* response) override {
        using Request = ::TaoTieChat::SearchUsersRequest;
        using Reply   = ::TaoTieChat::SearchUsersReply;
        using sc      = ::TaoTieChat::SuccessCode;
        using ec      = ::TaoTieChat::ErrorCode;

        // reply 不是使用智能指针创建的，此处只能使用裸指针，不能使用智能指针，否则会被智能指针再次析构
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

        // ================================================================== //

        if(!context || !request || !response) {
            return Status(::grpc::StatusCode::INTERNAL, "Invalid request or response pointer");
        }

        // ================================================================== //

        // 复制请求
        response->mutable_request()->CopyFrom(*request);  // 获取可变的请求字段并复制内容

        // 用户搜索关键词不能为空
        if(request->users_keyword().empty()) {
            set_error(response, ec::SEARCH_USERS_KEYWORD_EMPTY);
            LOG_DEBUG("Users keyword is empty, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
            return Status::OK;
        }

        // 参数合法，执行查找用户操作
        // 初始化相关参数
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        std::shared_ptr<::TaoTieChat::SearchUsersRequest> params = std::make_shared<::TaoTieChat::SearchUsersRequest>(*request);
        // 执行注册
        std::shared_ptr<::TaoTieChat::SearchUsersReply> result = db_sqlcipher.search_users(params);
        if(!result) {
            set_error(response, ec::DB_EXECUTE);
            LOG_DEBUG("SearchUsers failed, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
            return Status::OK;
        }
        // set_success(response, sc::SUCCESS);
        response->CopyFrom(*result);
        return Status::OK;
    }

    // 搜索群组  // TODO:功能未完成
    Status SearchGroups(ServerContext* context, const SearchGroupsRequest* request, SearchGroupsReply* response) override {
        using Request = ::TaoTieChat::SearchGroupsRequest;
        using Reply   = ::TaoTieChat::SearchGroupsReply;
        using sc      = ::TaoTieChat::SuccessCode;
        using ec      = ::TaoTieChat::ErrorCode;

        // reply 不是使用智能指针创建的，此处只能使用裸指针，不能使用智能指针，否则会被智能指针再次析构
        auto set_success = [&](Reply*reply, sc code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_success_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
        };
        auto set_error = [&](Reply*reply, ec code) {
            if(!reply) {
                return;
            }
            reply->mutable_result()->set_error_code(code);
            reply->mutable_result()->set_message(Utils::to_message_string(code));
        };

        // ================================================================== //

        if(!context || !request || !response) {
            return Status(::grpc::StatusCode::INTERNAL, "Invalid request or response pointer");
        }

        // ================================================================== //

        // 复制请求
        response->mutable_request()->CopyFrom(*request);  // 获取可变的请求字段并复制内容

        // 群组搜索词不能为空
        if(request->groups_keyword().empty()) {
            set_error(response, ec::SIGN_UP_USER_ID_EMPTY);
            LOG_DEBUG("Group keyword is empty, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
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
        // DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        // std::shared_ptr<::TaoTieChat::SearchGroupsRequest> params = std::make_shared<::TaoTieChat::SearchGroupsRequest>(*request);
        // // 执行登录验证
        // std::shared_ptr<::TaoTieChat::SearchGroupsReply> result = db_sqlcipher.search_groups(params);
        // if(!result) {
        //     set_error(response, ec::DB_EXECUTE);
        //     LOG_DEBUG("SignIn failed, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
        //     return Status::OK;
        // }
        // set_success(response, sc::SUCCESS);
        // response->CopyFrom(*result);
        return Status::OK;
    }
};
