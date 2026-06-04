#include "headers.h"
#include "utils.h"

#include "DBSQLCipherCmds.h"
#include "DBSQLCipher.h"
#include "AsyncCall.h"
#include "ThreadPool.h"

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

namespace Chat::Service::Account::Rpc::Async {

// UserRegister 返回给定请求的注册信息。
class SignUp final : public AsyncCall {
    using Request = SignUpRequest;
    using Reply   = SignUpReply  ;
public:
    SignUp(AccountService::AsyncService* service, grpc::ServerCompletionQueue* completion_queue, ThreadPool* thread_pool)
        : m_service(service), m_completion_queue(completion_queue), m_thread_pool(thread_pool), m_responder(&m_context), m_status(CallStatus::CREATE) {
        m_service->RequestSignUp(&m_context, &m_request, &m_responder, m_completion_queue, m_completion_queue, this);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            delete this;
            return;
        }

        if (m_status == CallStatus::CREATE) {
            m_status = CallStatus::PROCESS;
            // 立即孵化新种子，继续接受新的 SignUp 请求
            new SignUp(m_service, m_completion_queue, m_thread_pool);
            process_status_create();
        }
        else if (m_status == CallStatus::PROCESS) {
            m_status = CallStatus::FINISH;
            process_status_process();
            m_responder.Finish(m_response, grpc::Status::OK, this);
        } else if (m_status == CallStatus::FINISH) {
            process_status_finish();
        }
    }

private:
    void process_status_create() {
        log_grpc_context(&m_context);
        m_response.mutable_request()->CopyFrom(m_request);  // 复制请求
        auto params = std::make_shared<Request>(m_request);

        // 任务线程中执行耗时的操作，执行完成后再唤醒
        m_thread_pool->enqueue([this, params]() {
            auto result = DBSQLCipher::instance().sign_up(params);
            {
                std::lock_guard<std::mutex> lock(m_mutex_db);
                m_result_db = std::move(result);
            }
            m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        });
        // if (m_thread_pool) {
        // }
        // else {
        //     std::lock_guard<std::mutex> lock(m_mutex_db);
        //     m_result_db = DBSQLCipher::instance().sign_up(params);
        //     m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        // }
    }

    void process_status_process() {
        std::shared_ptr<Reply> result;
        // 取出耗时操作的执行结果
        {
            std::lock_guard<std::mutex> lock(m_mutex_db);
            result = m_result_db;
        }

        // 判断结果是否为 nullptr，防止拷贝时崩溃
        if (!result) {
            error_status<Reply>(&m_response, ErrorCode::DB_EXECUTE, std::format("SignUp failed."));
            m_responder.Finish(m_response, grpc::Status(grpc::StatusCode::INTERNAL, "SignUp failed."), this);
            return;
        }

        m_response.CopyFrom(*result);
    }

    void process_status_finish() {
        delete this;
    }

private:
    // 状态机
    enum class CallStatus { CREATE, PROCESS, FINISH };
    CallStatus m_status;

    // 服务/处理
    AccountService::AsyncService* m_service         ;
    grpc::ServerCompletionQueue * m_completion_queue;
    ThreadPool                  * m_thread_pool     ;

    // 网络数据
    grpc::ServerContext m_context ;
    Request       m_request ;
    Reply         m_response;
    grpc::ServerAsyncResponseWriter<Reply> m_responder;

    // 耗时操作 // 线程唤醒
    grpc::Alarm m_alarm   ;
    std::mutex  m_mutex_db;
    std::shared_ptr<Reply> m_result_db;
};

class SignIn final : public AsyncCall {
    using Request = SignInRequest;
    using Reply   = SignInReply  ;
public:
    SignIn(AccountService::AsyncService* service, grpc::ServerCompletionQueue* completion_queue, ThreadPool* thread_pool)
        : m_service(service), m_completion_queue(completion_queue), m_thread_pool(thread_pool), m_responder(&m_context), m_status(CallStatus::CREATE) {
        m_service->RequestSignIn(&m_context, &m_request, &m_responder, m_completion_queue, m_completion_queue, this);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            delete this;
            return;
        }

        if (m_status == CallStatus::CREATE) {
            m_status = CallStatus::PROCESS;
            // 立即孵化新种子，继续接受新的 SignIn 请求
            new SignIn(m_service, m_completion_queue, m_thread_pool);
            process_status_create();
        }
        else if (m_status == CallStatus::PROCESS) {
            m_status = CallStatus::FINISH;
            process_status_process();
            m_responder.Finish(m_response, grpc::Status::OK, this);
        } else if (m_status == CallStatus::FINISH) {
            process_status_finish();
        }
    }

private:
    void process_status_create() {
        log_grpc_context(&m_context);
        m_response.mutable_request()->CopyFrom(m_request);
        auto params = std::make_shared< Request>(m_request);

        // 任务线程中执行耗时的操作，执行完成后再唤醒
        m_thread_pool->enqueue([this, params]() {
            auto result = DBSQLCipher::instance().sign_in(params);
            {
                std::lock_guard<std::mutex> lock(m_mutex_db);
                m_result_db = std::move(result);
            }
            m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        });
        // if (m_thread_pool) {
        // }
        // else {
        //     std::lock_guard<std::mutex> lock(m_mutex_db);
        //     m_result_db = DBSQLCipher::instance().sign_up(params);
        //     m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        // }
    }

    void process_status_process() {
        std::shared_ptr<Reply> result;
        // 取出耗时操作的执行结果
        {
            std::lock_guard<std::mutex> lock(m_mutex_db);
            result = m_result_db;
        }

        if (!result) {
            error_status<Reply>(&m_response, ErrorCode::DB_EXECUTE, std::format("SignIn failed."));
            m_responder.Finish(m_response, grpc::Status(grpc::StatusCode::INTERNAL, "SignIn failed."), this);
            return;
        }

        m_response.CopyFrom(*result);
    }

    void process_status_finish() {
        delete this;
    }

private:
    // 状态机
    enum class CallStatus { CREATE, PROCESS, FINISH };
    CallStatus m_status;

    // 服务/处理
    AccountService::AsyncService* m_service         ;
    grpc::ServerCompletionQueue * m_completion_queue;
    ThreadPool                  * m_thread_pool     ;  // 复制指针，线程池由 main 函数创建并管理生命周期

    // 网络数据
    grpc::ServerContext m_context ;
    Request       m_request ;
    Reply         m_response;
    grpc::ServerAsyncResponseWriter<Reply> m_responder;

    // 耗时操作 // 线程唤醒
    grpc::Alarm m_alarm   ;
    std::mutex  m_mutex_db;
    std::shared_ptr<Reply> m_result_db;
};

// 添加联系人的请求，应该由联系人同意后方可完成添加
class ContactRequest final : public AsyncCall {
    using Request = ContactRequestRequest;
    using Reply   = ContactRequestReply  ;
public:
    ContactRequest(AccountService::AsyncService* service, grpc::ServerCompletionQueue* completion_queue, ThreadPool* thread_pool)
        : m_service(service), m_completion_queue(completion_queue), m_thread_pool(thread_pool), m_responder(&m_context), m_status(CallStatus::CREATE) {
        m_service->RequestContactRequest(&m_context, &m_request, &m_responder, m_completion_queue, m_completion_queue, this);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            delete this;
            return;
        }

        if (m_status == CallStatus::CREATE) {
            m_status = CallStatus::PROCESS;
            // 立即孵化新种子，继续接受新的 ContactRequest 请求
            new ContactRequest(m_service, m_completion_queue, m_thread_pool);
            process_status_create();
        }
        else if (m_status == CallStatus::PROCESS) {
            m_status = CallStatus::FINISH;
            process_status_process();
            m_responder.Finish(m_response, grpc::Status::OK, this);
        } else if (m_status == CallStatus::FINISH) {
            process_status_finish();
        }
    }

private:
    void process_status_create() {
        log_grpc_context(&m_context);
        m_response.mutable_request()->CopyFrom(m_request);
        auto params = std::make_shared<Request>(m_request);

        // 任务线程中执行耗时的操作，执行完成后再唤醒
        m_thread_pool->enqueue([this, params]() {
            auto result = DBSQLCipher::instance().contact_request(params);
            {
                std::lock_guard<std::mutex> lock(m_mutex_db);
                m_result_db = std::move(result);
            }
            m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        });
        // if (m_thread_pool) {
        // }
        // else {
        //     std::lock_guard<std::mutex> lock(m_mutex_db);
        //     m_result_db = DBSQLCipher::instance().sign_up(params);
        //     m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        // }
    }

    void process_status_process() {
        std::shared_ptr<Reply> result;
        // 取出耗时操作的执行结果
        {
            std::lock_guard<std::mutex> lock(m_mutex_db);
            result = m_result_db;
        }

        if (!result) {
            error_status<Reply>(&m_response, ErrorCode::DB_EXECUTE, std::format("ContactRequest failed."));
            m_responder.Finish(m_response, grpc::Status(grpc::StatusCode::INTERNAL, "ContactRequest failed."), this);
            return;
        }

        m_response.CopyFrom(*result);
    }

    void process_status_finish() {
        delete this;
    }

private:
    // 状态机
    enum class CallStatus { CREATE, PROCESS, FINISH };
    CallStatus m_status;

    // 服务/处理
    AccountService::AsyncService* m_service         ;
    ServerCompletionQueue       * m_completion_queue;
    ThreadPool                  * m_thread_pool     ;  // 复制指针，线程池由 main 函数创建并管理生命周期

    // 网络数据
    ServerContext m_context ;
    Request       m_request ;
    Reply         m_response;
    ServerAsyncResponseWriter<Reply> m_responder;

    // 耗时操作 // 线程唤醒
    Alarm             m_alarm    ;
    mutex             m_mutex_db ;
    shared_ptr<Reply> m_result_db;
};

// 添加联系人的请求的回复
class ContactReply final : public AsyncCall {
    using Request = ContactReplyRequest;
    using Reply   = ContactReplyReply  ;
public:
    ContactReply(AccountService::AsyncService* service, grpc::ServerCompletionQueue* completion_queue, ThreadPool* thread_pool)
        : m_service(service), m_completion_queue(completion_queue), m_thread_pool(thread_pool), m_responder(&m_context), m_status(CallStatus::CREATE) {
        m_service->RequestContactReply(&m_context, &m_request, &m_responder, m_completion_queue, m_completion_queue, this);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            delete this;
            return;
        }

        if (m_status == CallStatus::CREATE) {
            m_status = CallStatus::PROCESS;
            // 立即孵化新种子，继续接受新的 ContactReply 请求
            new ContactReply(m_service, m_completion_queue, m_thread_pool);
            process_status_create();
        }
        else if (m_status == CallStatus::PROCESS) {
            m_status = CallStatus::FINISH;
            process_status_process();
            m_responder.Finish(m_response, grpc::Status::OK, this);
        } else if (m_status == CallStatus::FINISH) {
            process_status_finish();
        }
    }

private:
    void process_status_create() {
        log_grpc_context(&m_context);
        m_response.mutable_request()->CopyFrom(m_request);
        auto params = std::make_shared<Request>(m_request);

        // 任务线程中执行耗时的操作，执行完成后再唤醒
        m_thread_pool->enqueue([this, params]() {
            auto result = DBSQLCipher::instance().contact_reply(params);
            {
                std::lock_guard<std::mutex> lock(m_mutex_db);
                m_result_db = std::move(result);
            }
            m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        });
        // if (m_thread_pool) {
        // }
        // else {
        //     std::lock_guard<std::mutex> lock(m_mutex_db);
        //     m_result_db = DBSQLCipher::instance().sign_up(params);
        //     m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        // }
    }

    void process_status_process() {
        std::shared_ptr<Reply> result;
        // 取出耗时操作的执行结果
        {
            std::lock_guard<std::mutex> lock(m_mutex_db);
            result = m_result_db;
        }

        if (!result) {
            error_status<Reply>(&m_response, ErrorCode::DB_EXECUTE, std::format("ContactReply failed."));
            m_responder.Finish(m_response, grpc::Status(grpc::StatusCode::INTERNAL, "ContactReply failed."), this);
            return;
        }

        m_response.CopyFrom(*result);
    }

    void process_status_finish() {
        delete this;
    }

private:
    // 状态机
    enum class CallStatus { CREATE, PROCESS, FINISH };
    CallStatus m_status;

    // 服务/处理
    AccountService::AsyncService* m_service         ;
    ServerCompletionQueue       * m_completion_queue;
    ThreadPool                  * m_thread_pool     ;  // 复制指针，线程池由 main 函数创建并管理生命周期

    // 网络数据
    ServerContext m_context ;
    Request       m_request ;
    Reply         m_response;
    ServerAsyncResponseWriter<Reply> m_responder;

    // 耗时操作 // 线程唤醒
    Alarm             m_alarm    ;
    mutex             m_mutex_db ;
    shared_ptr<Reply> m_result_db;
};

// 更新用户信息
class UpdateProfile final : public AsyncCall {
    using Request = UpdateProfileRequest;
    using Reply   = UpdateProfileReply  ;
public:
    UpdateProfile(AccountService::AsyncService* service, grpc::ServerCompletionQueue* completion_queue, ThreadPool* thread_pool)
        : m_service(service), m_completion_queue(completion_queue), m_thread_pool(thread_pool), m_responder(&m_context), m_status(CallStatus::CREATE) {
        m_service->RequestUpdateProfile(&m_context, &m_request, &m_responder, m_completion_queue, m_completion_queue, this);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            delete this;
            return;
        }

        if (m_status == CallStatus::CREATE) {
            m_status = CallStatus::PROCESS;
            // 立即孵化新种子，继续接受新的 UpdateProfile 请求
            new UpdateProfile(m_service, m_completion_queue, m_thread_pool);
            process_status_create();
        }
        else if (m_status == CallStatus::PROCESS) {
            m_status = CallStatus::FINISH;
            process_status_process();
            m_responder.Finish(m_response, grpc::Status::OK, this);
        } else if (m_status == CallStatus::FINISH) {
            process_status_finish();
        }
    }

private:
    void process_status_create() {
        log_grpc_context(&m_context);
        m_response.mutable_request()->CopyFrom(m_request);
        auto params = std::make_shared<Request>(m_request);

        // 任务线程中执行耗时的操作，执行完成后再唤醒
        m_thread_pool->enqueue([this, params]() {
            auto result = DBSQLCipher::instance().update_profile(params);
            {
                std::lock_guard<std::mutex> lock(m_mutex_db);
                m_result_db = std::move(result);
            }
            m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        });
        // if (m_thread_pool) {
        // }
        // else {
        //     std::lock_guard<std::mutex> lock(m_mutex_db);
        //     m_result_db = DBSQLCipher::instance().sign_up(params);
        //     m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        // }
    }

    void process_status_process() {
        std::shared_ptr<Reply> result;
        // 取出耗时操作的执行结果
        {
            std::lock_guard<std::mutex> lock(m_mutex_db);
            result = m_result_db;
        }

        if (!result) {
            error_status<Reply>(&m_response, ErrorCode::DB_EXECUTE, std::format("UpdateProfile failed."));
            m_responder.Finish(m_response, grpc::Status(grpc::StatusCode::INTERNAL, "UpdateProfile failed."), this);
            return;
        }

        m_response.CopyFrom(*result);
    }

    void process_status_finish() {
        delete this;
    }

private:
    // 状态机
    enum class CallStatus { CREATE, PROCESS, FINISH };
    CallStatus m_status;

    // 服务/处理
    AccountService::AsyncService* m_service         ;
    ServerCompletionQueue       * m_completion_queue;
    ThreadPool                  * m_thread_pool     ;  // 复制指针，线程池由 main 函数创建并管理生命周期

    // 网络数据
    ServerContext m_context ;
    Request       m_request ;
    Reply         m_response;
    ServerAsyncResponseWriter<Reply> m_responder;

    // 耗时操作 // 线程唤醒
    Alarm             m_alarm    ;
    mutex             m_mutex_db ;
    shared_ptr<Reply> m_result_db;
};

// 获取安全问题
class GetSecurityQuestion final : public AsyncCall {
    using Request = AuthRequest;
    using Reply   = GetSecurityQuestionReply;
public:
    GetSecurityQuestion(AccountService::AsyncService* service, grpc::ServerCompletionQueue* completion_queue, ThreadPool* thread_pool)
        : m_service(service), m_completion_queue(completion_queue), m_thread_pool(thread_pool), m_responder(&m_context), m_status(CallStatus::CREATE) {
        m_service->RequestGetSecurityQuestion(&m_context, &m_request, &m_responder, m_completion_queue, m_completion_queue, this);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            delete this;
            return;
        }

        if (m_status == CallStatus::CREATE) {
            m_status = CallStatus::PROCESS;
            // 立即孵化新种子，继续接受新的 GetSecurityQuestion 请求
            new GetSecurityQuestion(m_service, m_completion_queue, m_thread_pool);
            process_status_create();
        }
        else if (m_status == CallStatus::PROCESS) {
            m_status = CallStatus::FINISH;
            process_status_process();
            m_responder.Finish(m_response, grpc::Status::OK, this);
        } else if (m_status == CallStatus::FINISH) {
            process_status_finish();
        }
    }

private:
    void process_status_create() {
        log_grpc_context(&m_context);
        m_response.mutable_request()->CopyFrom(m_request);
        auto params = std::make_shared<Request>(m_request);

        // 任务线程中执行耗时的操作，执行完成后再唤醒
        m_thread_pool->enqueue([this, params]() {
            auto result = DBSQLCipher::instance().get_security_question(params);
            {
                std::lock_guard<std::mutex> lock(m_mutex_db);
                m_result_db = std::move(result);
            }
            m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        });
        // if (m_thread_pool) {
        // }
        // else {
        //     std::lock_guard<std::mutex> lock(m_mutex_db);
        //     m_result_db = DBSQLCipher::instance().sign_up(params);
        //     m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        // }
    }

    void process_status_process() {
        std::shared_ptr<Reply> result;
        // 取出耗时操作的执行结果
        {
            std::lock_guard<std::mutex> lock(m_mutex_db);
            result = m_result_db;
        }

        if (!result) {
            error_status<Reply>(&m_response, ErrorCode::DB_EXECUTE, std::format("GetSecurityQuestion failed."));
            m_responder.Finish(m_response, grpc::Status(grpc::StatusCode::INTERNAL, "GetSecurityQuestion failed."), this);
            return;
        }

        m_response.CopyFrom(*result);
    }

    void process_status_finish() {
        delete this;
    }

private:
    // 状态机
    enum class CallStatus { CREATE, PROCESS, FINISH };
    CallStatus m_status;

    // 服务/处理
    AccountService::AsyncService* m_service         ;
    ServerCompletionQueue       * m_completion_queue;
    ThreadPool                  * m_thread_pool     ;  // 复制指针，线程池由 main 函数创建并管理生命周期

    // 网络数据
    ServerContext m_context ;
    Request       m_request ;
    Reply         m_response;
    ServerAsyncResponseWriter<Reply> m_responder;

    // 耗时操作 // 线程唤醒
    Alarm             m_alarm    ;
    mutex             m_mutex_db ;
    shared_ptr<Reply> m_result_db;
};

// 更新用户安全信息
// 修改密码、密保问题、找回账号等
class UpdateSecurityInfo final : public AsyncCall {
    using Request = UpdateSecurityInfoRequest;
    using Reply   = UpdateSecurityInfoReply  ;
public:
    UpdateSecurityInfo(AccountService::AsyncService* service, grpc::ServerCompletionQueue* completion_queue, ThreadPool* thread_pool)
        : m_service(service), m_completion_queue(completion_queue), m_thread_pool(thread_pool), m_responder(&m_context), m_status(CallStatus::CREATE) {
        m_service->RequestUpdateSecurityInfo(&m_context, &m_request, &m_responder, m_completion_queue, m_completion_queue, this);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            delete this;
            return;
        }

        if (m_status == CallStatus::CREATE) {
            m_status = CallStatus::PROCESS;
            // 立即孵化新种子，继续接受新的 UpdateSecurityInfo 请求
            new UpdateSecurityInfo(m_service, m_completion_queue, m_thread_pool);
            process_status_create();
        }
        else if (m_status == CallStatus::PROCESS) {
            m_status = CallStatus::FINISH;
            process_status_process();
            m_responder.Finish(m_response, grpc::Status::OK, this);
        } else if (m_status == CallStatus::FINISH) {
            process_status_finish();
        }
    }

private:
    void process_status_create() {
        log_grpc_context(&m_context);
        m_response.mutable_request()->CopyFrom(m_request);
        auto params = std::make_shared<Request>(m_request);

        // 任务线程中执行耗时的操作，执行完成后再唤醒
        m_thread_pool->enqueue([this, params]() {
            auto result = DBSQLCipher::instance().update_security_info(params);
            {
                std::lock_guard<std::mutex> lock(m_mutex_db);
                m_result_db = std::move(result);
            }
            m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        });
        // if (m_thread_pool) {
        // }
        // else {
        //     std::lock_guard<std::mutex> lock(m_mutex_db);
        //     m_result_db = DBSQLCipher::instance().sign_up(params);
        //     m_alarm.Set(m_completion_queue, gpr_now(GPR_CLOCK_REALTIME), this);
        // }
    }

    void process_status_process() {
        std::shared_ptr<Reply> result;
        // 取出耗时操作的执行结果
        {
            std::lock_guard<std::mutex> lock(m_mutex_db);
            result = m_result_db;
        }

        if (!result) {
            error_status<Reply>(&m_response, ErrorCode::DB_EXECUTE, std::format("UpdateSecurityInfo failed."));
            m_responder.Finish(m_response, grpc::Status(grpc::StatusCode::INTERNAL, "UpdateSecurityInfo failed."), this);
            return;
        }

        m_response.CopyFrom(*result);
    }

    void process_status_finish() {
        delete this;
    }

private:
    // 状态机
    enum class CallStatus { CREATE, PROCESS, FINISH };
    CallStatus m_status;

    // 服务/处理
    AccountService::AsyncService* m_service         ;
    ServerCompletionQueue       * m_completion_queue;
    ThreadPool                  * m_thread_pool     ;  // 复制指针，线程池由 main 函数创建并管理生命周期

    // 网络数据
    ServerContext m_context ;
    Request       m_request ;
    Reply         m_response;
    ServerAsyncResponseWriter<Reply> m_responder;

    // 耗时操作 // 线程唤醒
    Alarm             m_alarm    ;
    mutex             m_mutex_db ;
    shared_ptr<Reply> m_result_db;
};

}

namespace Chat::Service::Account::Rpc::AsyncCall {

class SignUp : public AsyncUnaryCall<AccountService::AsyncService, SignUpRequest, SignUpReply> {
public:
    // 构造函数：同时接收服务、完成队列和线程池
    SignUp(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncUnaryCall<AccountService::AsyncService, SignUpRequest, SignUpReply>(service, cq, &AccountService::AsyncService::RequestSignUp), m_thread_pool(thread_pool) {
        // 不要捕获 this，直接显式捕获局部变量 thread_pool 确保线程安全
        SetBusinessHandler([thread_pool](auto* call, auto* context, const auto& request, auto* response) {
            // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
            thread_pool->enqueue([call, request]() {
                // 执行耗时的 DBSQLCipher 操作
                std::shared_ptr<SignUpReply> result = DBSQLCipher::instance().sign_up(std::make_shared<SignUpRequest>(request));

                if (!result) {
                    call->Error(grpc::StatusCode::INTERNAL, "SignUp Database Failed");
                } else {
                    call->Success(*result); // 模板内自动调用 Finish 销毁自己
                }
            });
        });
    }

protected:
    // 重写孵化虚函数，确保高并发时，基类自动 new 出来的下一个监听器依然是 SignUp 子类，而不是空壳基类
    void CloneNext(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq) override {
        new SignUp(service, cq, m_thread_pool);
    }

private:
    ThreadPool* m_thread_pool;
};

class SignIn : public AsyncUnaryCall<AccountService::AsyncService, SignInRequest, SignInReply> {
public:
    // 构造函数：同时接收服务、完成队列和线程池
    SignIn(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncUnaryCall<AccountService::AsyncService, SignInRequest, SignInReply>(service, cq, &AccountService::AsyncService::RequestSignIn), m_thread_pool(thread_pool) {
        // 不要捕获 this，直接显式捕获局部变量 thread_pool 确保线程安全
        SetBusinessHandler([thread_pool](auto* call, auto* context, const auto& request, auto* response) {
            // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
            thread_pool->enqueue([call, request]() {
                // 执行耗时的 DBSQLCipher 操作
                std::shared_ptr<SignInReply> result = DBSQLCipher::instance().sign_in(std::make_shared<SignInRequest>(std::move(request)));

                if (!result) {
                    call->Error(grpc::StatusCode::INTERNAL, "SignIn Database Failed");
                } else {
                    call->Success(*result); // 模板内自动调用 Finish 销毁自己
                }
            });
        });
    }

protected:
    // 重写孵化虚函数，确保高并发时，基类自动 new 出来的下一个监听器依然是 SignIn 子类，而不是空壳基类
    void CloneNext(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq) override {
        new SignIn(service, cq, m_thread_pool);
    }

private:
    ThreadPool* m_thread_pool;
};

class ContactRequest : public AsyncUnaryCall<AccountService::AsyncService, ContactRequestRequest, ContactRequestReply> {
public:
    // 构造函数：同时接收服务、完成队列和线程池
    ContactRequest(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncUnaryCall<AccountService::AsyncService, ContactRequestRequest, ContactRequestReply>(service, cq, &AccountService::AsyncService::RequestContactRequest), m_thread_pool(thread_pool) {
        // 不要捕获 this，直接显式捕获局部变量 thread_pool 确保线程安全
        SetBusinessHandler([thread_pool](auto* call, auto* context, const auto& request, auto* response) {
            // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
            thread_pool->enqueue([call, request]() {
                // 执行耗时的 DBSQLCipher 操作
                std::shared_ptr<ContactRequestReply> result = DBSQLCipher::instance().contact_request(std::make_shared<ContactRequestRequest>(std::move(request)));

                if (!result) {
                    call->Error(grpc::StatusCode::INTERNAL, "ContactRequest Database Failed");
                } else {
                    call->Success(*result); // 模板内自动调用 Finish 销毁自己
                }
            });
        });
    }

protected:
    // 重写孵化虚函数，确保高并发时，基类自动 new 出来的下一个监听器依然是 ContactRequest 子类，而不是空壳基类
    void CloneNext(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq) override {
        new ContactRequest(service, cq, m_thread_pool);
    }

private:
    ThreadPool* m_thread_pool;
};

class ContactReply : public AsyncUnaryCall<AccountService::AsyncService, ContactReplyRequest, ContactReplyReply> {
public:
    // 构造函数：同时接收服务、完成队列和线程池
    ContactReply(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncUnaryCall<AccountService::AsyncService, ContactReplyRequest, ContactReplyReply>(service, cq, &AccountService::AsyncService::RequestContactReply), m_thread_pool(thread_pool) {
        // 不要捕获 this，直接显式捕获局部变量 thread_pool 确保线程安全
        SetBusinessHandler([thread_pool](auto* call, auto* context, const auto& request, auto* response) {
            // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
            thread_pool->enqueue([call, request]() {
                // 执行耗时的 DBSQLCipher 操作
                std::shared_ptr<ContactReplyReply> result = DBSQLCipher::instance().contact_reply(std::make_shared<ContactReplyRequest>(std::move(request)));

                if (!result) {
                    call->Error(grpc::StatusCode::INTERNAL, "ContactReply Database Failed");
                } else {
                    call->Success(*result); // 模板内自动调用 Finish 销毁自己
                }
            });
        });
    }

protected:
    // 重写孵化虚函数，确保高并发时，基类自动 new 出来的下一个监听器依然是 ContactReply 子类，而不是空壳基类
    void CloneNext(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq) override {
        new ContactReply(service, cq, m_thread_pool);
    }

private:
    ThreadPool* m_thread_pool;
};

class UpdateProfile : public AsyncUnaryCall<AccountService::AsyncService, UpdateProfileRequest, UpdateProfileReply> {
public:
    // 构造函数：同时接收服务、完成队列和线程池
    UpdateProfile(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncUnaryCall<AccountService::AsyncService, UpdateProfileRequest, UpdateProfileReply>(service, cq, &AccountService::AsyncService::RequestUpdateProfile), m_thread_pool(thread_pool) {
        // 不要捕获 this，直接显式捕获局部变量 thread_pool 确保线程安全
        SetBusinessHandler([thread_pool](auto* call, auto* context, const auto& request, auto* response) {
            // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
            thread_pool->enqueue([call, request]() {
                // 执行耗时的 DBSQLCipher 操作
                std::shared_ptr<UpdateProfileReply> result = DBSQLCipher::instance().update_profile(std::make_shared<UpdateProfileRequest>(std::move(request)));

                if (!result) {
                    call->Error(grpc::StatusCode::INTERNAL, "UpdateProfile Database Failed");
                } else {
                    call->Success(*result); // 模板内自动调用 Finish 销毁自己
                }
            });
        });
    }

protected:
    // 重写孵化虚函数，确保高并发时，基类自动 new 出来的下一个监听器依然是 UpdateProfile 子类，而不是空壳基类
    void CloneNext(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq) override {
        new UpdateProfile(service, cq, m_thread_pool);
    }

private:
    ThreadPool* m_thread_pool;
};

class GetSecurityQuestion : public AsyncUnaryCall<AccountService::AsyncService, AuthRequest, GetSecurityQuestionReply> {
public:
    // 构造函数：同时接收服务、完成队列和线程池
    GetSecurityQuestion(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncUnaryCall<AccountService::AsyncService, AuthRequest, GetSecurityQuestionReply>(service, cq, &AccountService::AsyncService::RequestGetSecurityQuestion), m_thread_pool(thread_pool) {
        // 不要捕获 this，直接显式捕获局部变量 thread_pool 确保线程安全
        SetBusinessHandler([thread_pool](auto* call, auto* context, const auto& request, auto* response) {
            // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
            thread_pool->enqueue([call, request]() {
                // 执行耗时的 DBSQLCipher 操作
                std::shared_ptr<GetSecurityQuestionReply> result = DBSQLCipher::instance().get_security_question(std::make_shared<AuthRequest>(std::move(request)));

                if (!result) {
                    call->Error(grpc::StatusCode::INTERNAL, "GetSecurityQuestion Database Failed");
                } else {
                    call->Success(*result); // 模板内自动调用 Finish 销毁自己
                }
            });
        });
    }

protected:
    // 重写孵化虚函数，确保高并发时，基类自动 new 出来的下一个监听器依然是 GetSecurityQuestion 子类，而不是空壳基类
    void CloneNext(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq) override {
        new GetSecurityQuestion(service, cq, m_thread_pool);
    }

private:
    ThreadPool* m_thread_pool;
};

class UpdateSecurityInfo : public AsyncUnaryCall<AccountService::AsyncService, UpdateSecurityInfoRequest, UpdateSecurityInfoReply> {
public:
    // 构造函数：同时接收服务、完成队列和线程池
    UpdateSecurityInfo(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncUnaryCall<AccountService::AsyncService, UpdateSecurityInfoRequest, UpdateSecurityInfoReply>(service, cq, &AccountService::AsyncService::RequestUpdateSecurityInfo), m_thread_pool(thread_pool) {
        // 不要捕获 this，直接显式捕获局部变量 thread_pool 确保线程安全
        SetBusinessHandler([thread_pool](auto* call, auto* context, const auto& request, auto* response) {
            // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
            thread_pool->enqueue([call, request]() {
                // 执行耗时的 DBSQLCipher 操作
                std::shared_ptr<UpdateSecurityInfoReply> result = DBSQLCipher::instance().update_security_info(std::make_shared<UpdateSecurityInfoRequest>(std::move(request)));

                if (!result) {
                    call->Error(grpc::StatusCode::INTERNAL, "UpdateSecurityInfo Database Failed");
                } else {
                    call->Success(*result); // 模板内自动调用 Finish 销毁自己
                }
            });
        });
    }

protected:
    // 重写孵化虚函数，确保高并发时，基类自动 new 出来的下一个监听器依然是 UpdateSecurityInfo 子类，而不是空壳基类
    void CloneNext(AccountService::AsyncService* service, grpc::ServerCompletionQueue* cq) override {
        new UpdateSecurityInfo(service, cq, m_thread_pool);
    }

private:
    ThreadPool* m_thread_pool;
};

}
