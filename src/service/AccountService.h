#include "headers.h"
#include "utils.h"
#include "DBSQLCipher.h"
#include "types.h"

using TaoTieChat::AccountService;
using TaoTieChat::UserLoginReply;
using TaoTieChat::UserLoginRequest;
using TaoTieChat::UserRegisterReply;
using TaoTieChat::UserRegisterRequest;

// 服务端实现
class AccountServiceImpl final : public AccountService::Service {
public:
    // UserRegister 返回给定请求的注册信息。
    Status UserRegister(ServerContext* context, const UserRegisterRequest* request, UserRegisterReply* response) override {
        // 复制请求
        response->mutable_request()->CopyFrom(*request); // 获取可变的请求字段并复制内容
        // 用户 ID 不能为空
        if(request->user_id().empty()) {
            response->set_success(false);
            response->set_error_message("User ID is required");
            // return Status::OK;
        }
        // 密码不能为空
        else if(request->password().empty()) {
            response->set_success(false);
            response->set_error_message("Password is required");
            // return Status::OK;
        }
        // 密保问题不能为空
        else if(request->security_question().empty()) {
            response->set_success(false);
            response->set_error_message("Security Question is required");
            // return Status::OK;
        }
        // 密保答案不能为空
        else if(request->security_answer().empty()) {
            response->set_success(false);
            response->set_error_message("Security Answer is required");
            // return Status::OK;
        }
        // 参数合法，执行注册
        else {
            // 初始化相关参数
            DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
            std::shared_ptr<UserRegisterParameters> params = std::make_shared<UserRegisterParameters>();
            std::shared_ptr<UserRegisterResult>     result = nullptr;
            params->user_id           = request->user_id          ();
            params->password          = request->password         ();
            params->security_question = request->security_question();
            params->security_answer   = request->security_answer  ();
            // 执行注册
            result = db_sqlcipher.user_register(params);
            if (!result->success) {
                // 注册失败，返回错误信息
                response->set_success(false);
                response->set_error_message(result->error_message);
                // return Status::OK;
            }
            else {
                // 注册成功，返回注册结果
                response->set_success      (true                 );
                response->set_account_id   (result->account_id   );
                response->set_error_message(result->error_message);
            }
        }
        return Status::OK;
    }

    // UserLogin 验证用户登录信息并返回登录结果。
    Status UserLogin(ServerContext* context, const UserLoginRequest* request, UserLoginReply* response) override {     
        // 复制请求
        response->mutable_request()->CopyFrom(*request); // 获取可变的请求字段并复制内容

        int         error_code = 11;
        std::string error_message; // 参数错误信息
        do {
            // 用户 ID 不能为空
            if(request->user_id().empty()) {
                error_code = 12;
                error_message = "User ID is required";
                break;
            }
            // 密码不能为空
            else if(request->password().empty()) {
                error_code = 13;
                error_message = "Password is required";
                break;
            }

            error_code = 0; // 参数合法
        } while(false); // 仅执行一次的循环，便于使用 break 语句提前退出

        if(error_code != 0) {
            response->set_success(false);
            response->set_error_message(error_message);
            return Status::OK;
        }

        // 表示已经检查参数合法，但尚未执行登录验证，默认设置为登录失败
        error_code = 21;

        // 参数合法，执行登录验证
        // 初始化相关参数
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        std::shared_ptr<UserLoginParameters> params = std::make_shared<UserLoginParameters>();
        std::shared_ptr<UserLoginResult>     result = nullptr;
        params->user_id    = request->user_id   ();
        params->password   = request->password  ();
        // 执行登录验证
        do {
            result = db_sqlcipher.user_login(params);
            if (!result->success) {
                error_code = 22; // 登录验证执行失败
                error_message = result->error_message;
                break; // 登录验证执行失败，提前退出循环
            }

            error_code = 0; // 登录验证执行成功
        } while(false); // 仅执行一次的循环，便于使用 break 语句提前退出

        response->set_success      (error_code == 0   );
        response->set_account_id   (result->account_id);
        response->set_token        (result->token     );
        response->set_error_message(result->error_message);
        return Status::OK;
    }
};
