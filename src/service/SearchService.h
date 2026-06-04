#include "headers.h"
#include "utils.h"

#include "DBSQLCipherCmds.h"
#include "DBSQLCipher.h"

using Utils::error_status;

// 服务端实现
class SearchServiceImpl final : public SearchService::Service {
public:
    // 搜索用户
    Status SearchUser(ServerContext* context, const SearchUserRequest* request, SearchUserReply* response) override {
        using Request = SearchUserRequest;
        using Reply   = SearchUserReply;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        response->mutable_request()->CopyFrom(*request);  // 复制请求
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        // 执行操作
        std::shared_ptr<Reply> result = db_sqlcipher.search_user(params);
        if(!result) {
            return error_status(response, ErrorCode::DB_EXECUTE, std::format("SearchUser failed"));
        }
        // set_success(response, sc::SUCCESS);
        response->CopyFrom(*result);
        return Status::OK;
    }

    // 搜索群组  // TODO:功能未完成
    Status SearchGroup(ServerContext* context, const SearchGroupRequest* request, SearchGroupReply* response) override {
        using Request = SearchGroupRequest;
        using Reply   = SearchGroupReply;
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
        // std::shared_ptr<::TaoTieChat::SearchGroupRequest> params = std::make_shared<::TaoTieChat::SearchGroupRequest>(*request);
        // // 执行登录验证
        // std::shared_ptr<::TaoTieChat::SearchGroupReply> result = db_sqlcipher.search_groups(params);
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

namespace Chat::Service::Search::Rpc::Async{

// 搜索用户
class SearchUser final : public AsyncCall {
    using Request = SearchUserRequest;
    using Reply   = SearchUserReply  ;
public:
    SearchUser(SearchService::AsyncService* service, grpc::ServerCompletionQueue* completion_queue, ThreadPool* thread_pool)
        : m_service(service), m_completion_queue(completion_queue), m_thread_pool(thread_pool), m_responder(&m_context), m_status(CallStatus::CREATE) {
        m_service->RequestSearchUser(&m_context, &m_request, &m_responder, m_completion_queue, m_completion_queue, this);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            delete this;
            return;
        }

        if (m_status == CallStatus::CREATE) {
            m_status = CallStatus::PROCESS;
            // 立即孵化新种子，继续接受新的 SearchUser 请求
            new SearchUser(m_service, m_completion_queue, m_thread_pool);
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
            auto result = DBSQLCipher::instance().search_user(params);
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
    SearchService::AsyncService * m_service         ;
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

}

namespace Chat::Service::Search::Rpc::AsyncCall {

// 搜索用户
class SearchUser : public AsyncUnaryCall<SearchService::AsyncService, SearchUserRequest, SearchUserReply> {
public:
    // 构造函数：同时接收服务、完成队列和线程池
    SearchUser(SearchService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncUnaryCall<SearchService::AsyncService, SearchUserRequest, SearchUserReply>(service, cq, &SearchService::AsyncService::RequestSearchUser), m_thread_pool(thread_pool) {
        // 不要捕获 this，直接显式捕获局部变量 thread_pool 确保线程安全
        SetBusinessHandler([thread_pool](auto* call, auto* context, const auto& request, auto* response) {
            // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
            thread_pool->enqueue([call, request]() {
                // 执行耗时的 DBSQLCipher 操作
                std::shared_ptr<SearchUserReply> result = DBSQLCipher::instance().search_user(std::make_shared<SearchUserRequest>(std::move(request)));

                if (!result) {
                    call->Error(grpc::StatusCode::INTERNAL, "SearchUser Database Failed");
                } else {
                    call->Success(*result); // 模板内自动调用 Finish 销毁自己
                }
            });
        });
    }

protected:
    // 重写孵化虚函数，确保高并发时，基类自动 new 出来的下一个监听器依然是 SearchUserCall 子类，而不是空壳基类
    void CloneNext(SearchService::AsyncService* service, grpc::ServerCompletionQueue* cq) override {
        new SearchUser(service, cq, m_thread_pool);
    }

private:
    ThreadPool* m_thread_pool;
};

}
