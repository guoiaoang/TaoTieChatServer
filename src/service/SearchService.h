#include "headers.h"
#include "utils.h"

#include "DBSQLCipherCmds.h"
#include "DBSQLCipher.h"

using std::shared_ptr;
using Utils::error_status;

// 服务端实现
class SearchServiceImpl final : public SearchService::Service {
public:
    // 搜索用户
    Status SearchUsers(ServerContext* context, const SearchUsersRequest* request, SearchUsersReply* response) override {
        using Request = SearchUsersRequest;
        using Reply   = SearchUsersReply;
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
        std::shared_ptr<Reply> result = db_sqlcipher.search_users(params);
        if(!result) {
            return error_status(response, ErrorCode::DB_EXECUTE, std::format("SearchUsers failed"));
        }
        // set_success(response, sc::SUCCESS);
        response->CopyFrom(*result);
        return Status::OK;
    }

    // 搜索群组  // TODO:功能未完成
    Status SearchGroups(ServerContext* context, const SearchGroupsRequest* request, SearchGroupsReply* response) override {
        using Request = SearchGroupsRequest;
        using Reply   = SearchGroupsReply;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 参数校验 =========================== //
        // // 群组搜索词不能为空
        // if(request->groups_keyword().empty()) {
        //     set_error(response, ErrorCode::USER_ID_EMPTY);
        //     LOG_DEBUG("Group keyword is empty, code: {:#010x}, message: {}", (int)response->mutable_result()->error_code(), response->mutable_result()->message());
        //     return Status::OK;
        // }
        // =========================== 执行操作 =========================== //

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
