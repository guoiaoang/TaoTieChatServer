#include "headers.h"
#include "utils.h"

#include "DBSQLCipherCmds.h"
#include "DBSQLCipher.h"

using std::string;
using std::vector;
using std::shared_ptr;
using std::make_shared;
using google::protobuf::Timestamp;

using Utils::set_success;
using Utils::set_error;
using Utils::log_grpc_context;
using Utils::error_status;
using Utils::Transform::to_string;

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
        response->CopyFrom(*result);
        return Status::OK;
    }

    // 添加联系人的请求
    // TODO: 应该由联系人同意后方可完成添加
    Status ContactRequest(ServerContext* context, const ContactRequestRequest* request, ContactRequestReply* response) override {
        using Request = ContactRequestRequest;
        using Reply   = ContactRequestReply;
        // =========================== 初始化操作 =========================== //
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        log_grpc_context(context);
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        // 添加联系人的请求
        shared_ptr<Reply> result = db_sqlcipher.contact_request(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("ContactRequest failed."));
        }
        response->CopyFrom(*result);
        return Status::OK;
    }

    // 添加联系人的请求的回复
    Status ContactReply(ServerContext* context, const ContactReplyRequest* request, ContactReplyReply* response) override {
        using Request = ContactReplyRequest;
        using Reply   = ContactReplyReply;
        // =========================== 初始化操作 =========================== //
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        log_grpc_context(context);
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        // 添加联系人的请求的回复
        shared_ptr<Reply> result = db_sqlcipher.contact_reply(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("ContactReply failed."));
        }
        response->CopyFrom(*result);
        return Status::OK;
    }

    // 更新用户信息
    Status UpdateProfile(ServerContext* context, const UpdateProfileRequest* request, UpdateProfileReply* response) override {
        using Request = UpdateProfileRequest;
        using Reply   = UpdateProfileReply;
        // =========================== 初始化操作 =========================== //
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        log_grpc_context(context);
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        // 更新用户信息
        shared_ptr<Reply> result = db_sqlcipher.update_profile(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("UpdateProfile failed."));
        }
        response->CopyFrom(*result);
        return Status::OK;
    }

    // 获取安全问题
    Status GetSecurityQuestion(ServerContext* context, const AuthRequest* request, GetSecurityQuestionReply* response) override {
        using Request = AuthRequest;
        using Reply   = GetSecurityQuestionReply;
        // =========================== 初始化操作 =========================== //
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        log_grpc_context(context);
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        // 获取安全问题
        shared_ptr<Reply> result = db_sqlcipher.get_security_question(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("GetSecurityQuestion failed."));
        }
        response->CopyFrom(*result);
        return Status::OK;
    }

    // 更新用户安全信息
    // 修改密码、密保问题、找回账号等
    Status UpdateSecurityInfo(ServerContext* context, const UpdateSecurityInfoRequest* request, UpdateSecurityInfoReply* response) override {
        using Request = UpdateSecurityInfoRequest;
        using Reply   = UpdateSecurityInfoReply;
        // =========================== 初始化操作 =========================== //
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        log_grpc_context(context);
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        // 更新用户安全信息
        shared_ptr<Reply> result = db_sqlcipher.update_security_info(params);
        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if(!result) {
            return error_status<Reply>(response, ErrorCode::DB_EXECUTE, std::format("UpdateSecurityInfo failed."));
        }
        response->CopyFrom(*result);
        return Status::OK;
    }
};
