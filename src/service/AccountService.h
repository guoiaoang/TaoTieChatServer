#include "headers.h"
#include "utils.h"
#include "DBSQLCipher.h"

// 服务端实现
class AccountServiceImpl final : public AccountService::Service {
public:
    // UserRegister 返回给定请求的注册信息。
    Status UserRegister(ServerContext* context, const UserRegisterRequest* request, UserRegisterReply* response) override {
        // 复制请求
        response->mutable_request()->CopyFrom(*request); // 获取可变的请求字段并复制内容
        // 昵称不能为空
        if(request->nickname().empty()) {
            response->set_success(false);
            response->set_error_message("Nickname is required");
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
            std::shared_ptr<DBSQLCipher::UserRegisterParameters> params = std::make_shared<DBSQLCipher::UserRegisterParameters>();
            std::shared_ptr<DBSQLCipher::UserRegisterResult>     result = nullptr;
            params->nickname          = request->nickname         ();
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
                response->set_user_id      (result->user_id      );
                response->set_error_message(result->error_message);
            }
        }
        return Status::OK;
    }
};
