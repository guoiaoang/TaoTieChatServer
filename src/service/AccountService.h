#include "headers.h"
#include "types.h"
#include "utils.h"
#include "DBSQLCipher.h"

using ::grpc::ServerContext;

using std::string;
using std::vector;
using std::shared_ptr;
using std::make_shared;
using ::grpc::StatusCode;
using ::google::protobuf::Timestamp;

using Utils::set_success;
using Utils::set_error;
using Utils::log_grpc_context;
using Utils::to_string;
using Utils::error_status;

// 服务端实现
class AccountServiceImpl final : public AccountService::Service {
public:
    // UserRegister 返回给定请求的注册信息。
    Status SignUp(ServerContext* context, const SignUpRequest* request, SignUpReply* response) override {
        using Request = SignUpRequest;
        using Reply   = SignUpReply;
        // =========================== 初始化操作 =========================== //
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        log_grpc_context(context);
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        // 执行注册
        shared_ptr<Reply> result = db_sqlcipher.sign_up(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("SignUp failed."));
        }
        // set_success(response, SuccessCode::SUCCESS);
        response->CopyFrom(*result);
        return Status::OK;
    }

    // SignIn 验证用户登录信息并返回登录结果。
    Status SignIn(ServerContext* context, const SignInRequest* request, SignInReply* response) override {
        using Request = SignInRequest;
        using Reply   = SignInReply;
        // =========================== 初始化操作 =========================== //
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        log_grpc_context(context);
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<SignInRequest> params = make_shared<::SignInRequest>(*request);
        // =========================== 执行操作 =========================== //
        // 执行登录验证
        shared_ptr<SignInReply> result = db_sqlcipher.sign_in(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("SignIn failed."));
        }
        // set_success(response, SuccessCode::SUCCESS);
        response->CopyFrom(*result);
        return Status::OK;
    }

    // update 获取更新内容
    Status GetUpdate(ServerContext* context, const UpdateResuest* request, UpdateReply* response) override {
        using Request = UpdateResuest;
        using Reply   = UpdateReply;
        // =========================== 初始化操作 =========================== //
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        log_grpc_context(context);
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        // shared_ptr<UpdateReply> params = make_shared<::UpdateReply>(*request);
        // // =========================== 参数校验 =========================== //
        // // 获取信息
        // // 请求
        // auto type        = request->type       ();  // 请求类型
        // auto auth_params = request->auth_params();  // 认证信息
        // auto timestamp   = request->timestamp  ();  // 时间戳
        // // 鉴权
        // auto auto_result = response->mutable_auth_result();
        // // auto_result->set_account_id("");

        // auto func_auth = []() -> bool { return false; };
        // // 可能部分接口不需要鉴权，因此鉴权放在具体的请求类型内
        // if(type == UpdateType::CONTACTS) {
        //     auto get_contacts_update = [&](const SignInRequest&auth, const Timestamp &timestamp) {
        //         // 鉴权失败
        //         if(func_auth()) {
        //             return;
        //         }
        //         // 获取结果
        //     };
        //     get_contacts_update(auth_params, timestamp);
        // }

        // =========================== 执行操作 =========================== //
        // response->mutable_auth()->set_account_id("");
        // response->result();  // 返回结果
        // response->auth();  // 返回内容
        // if(response->has_contacts()) {
        //     response->contacts().contact();
        // }
        // response->timestamp();

        // response->CopyFrom(*result);
        return Status::OK;
    }
};
