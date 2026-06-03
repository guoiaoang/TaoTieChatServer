#include "headers.h"
#include "utils.h"

#include "DBSQLCipherCmds.h"
#include "DBSQLCipher.h"

using Utils::set_success;
using Utils::set_error;
using Utils::log_grpc_context;
using Utils::error_status;
using Utils::Transform::to_string;

// ========================================================
// 1. 抽象基类：统一的事件分发接口
// ========================================================
// 服务公共基类，每一个 rpc 都需要实现一个子类。
class AsyncCall {
public:
    virtual ~AsyncCall() = default;
    virtual void Proceed(bool ok) = 0;
};

// ============================================================================
// 1️⃣ Unary RPC 模板 (简单一问一答)
// ============================================================================
template <typename Service, typename Request, typename Reply>
class AsyncUnaryCall : public AsyncCall {
public:
    // using RegisterFunc = std::function<void(Service*, grpc::ServerContext*, Request*, grpc::ServerAsyncResponseWriter<Reply>*, grpc::CompletionQueue*, grpc::CompletionQueue*, void*)>;
    using RegisterMethod = void (Service::*)(grpc::ServerContext*, Request*, grpc::ServerAsyncResponseWriter<Reply>*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*);

    AsyncUnaryCall(Service* service, grpc::ServerCompletionQueue* cq, RegisterMethod registerMethod)
        : m_service(service), m_cq(cq), m_registerMethod(registerMethod), m_responder(&m_ctx), m_status(CREATE) {
        (m_service->*m_registerMethod)(&m_ctx, &m_request, &m_responder, m_cq, m_cq, this);
    }

    void Proceed(bool ok) override {
        if (!ok) { delete this; return; }

        if (m_status == CREATE) {
            m_status = FINISH;
            // // 孵化下一个种子，保证网关并发监听
            // auto next = new AsyncUnaryCall<Service, Request, Reply>(m_service, m_cq, m_registerFn);
            // next->SetBusinessHandler(m_handler);

            // 由虚函数来决定克隆谁。如果是子类，会自动调用子类的 CloneNext
            this->CloneNext(m_service, m_cq);

            if (m_handler) {
                // 触发具体的业务处理子类/回调
                m_handler(this, &m_ctx, m_request, &m_response);
            }
        } else if (m_status == FINISH) {
            delete this;
        }
    }

    // 提供给具体的业务实现调用：正常成功返回
    void Success(const Reply& reply) {
        m_response = reply;
        m_responder.Finish(m_response, grpc::Status::OK, this);
    }

    // 提供给具体的业务实现调用：失败返回
    void Error(grpc::StatusCode code, const std::string& msg) {
        m_responder.Finish(m_response, grpc::Status(code, msg), this);
    }

    void SetBusinessHandler(std::function<void(AsyncUnaryCall*, grpc::ServerContext*, const Request&, Reply*)> handler) {
        m_handler = handler;
    }

protected:
    // 提供默认的克隆行为，如果是直接实例化基类就 new 基类；如果是子类继承则在子类中重写
    virtual void CloneNext(Service* service, grpc::ServerCompletionQueue* cq) {
        auto next = new AsyncUnaryCall<Service, Request, Reply>(service, cq, m_registerMethod);
        next->SetBusinessHandler(m_handler);
    }

private:
    enum Status { CREATE, FINISH };
    Status m_status;
    Service* m_service;
    grpc::ServerCompletionQueue* m_cq;
    RegisterMethod m_registerMethod;

    grpc::ServerContext m_ctx;
    Request m_request;
    Reply m_response;
    grpc::ServerAsyncResponseWriter<Reply> m_responder;
    std::function<void(AsyncUnaryCall*, grpc::ServerContext*, const Request&, Reply*)> m_handler;
};

// ============================================================================
// 2️⃣ Server Streaming 模板 (服务端流：一问多答)
// ============================================================================
template <typename Service, typename Request, typename Reply>
class AsyncServerStreamingCall : public AsyncCall {
public:
    using RegisterMethod = void (Service::*)(grpc::ServerContext*, Request*, grpc::ServerAsyncWriter<Reply>*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*);

    AsyncServerStreamingCall(Service* service, grpc::ServerCompletionQueue* cq, RegisterMethod registerMethod)
        : m_service(service), m_cq(cq), m_registerMethod(registerMethod), m_responder(&m_ctx), m_status(CREATE) {
        (m_service->*m_registerMethod)(&m_ctx, &m_request, &m_responder, m_cq, m_cq, this);
    }

    void Proceed(bool ok) override {
        if (!ok) { delete this; return; }

        if (m_status == CREATE) {
            m_status = PROCESS;
            // 孵化下一个流监听种子
            auto next = new AsyncServerStreamingCall<Service, Request, Reply>(m_service, m_cq, m_registerMethod);
            next->SetOnStartHandler(m_onStartHandler);
            next->SetOnWriteDoneHandler(m_onWriteDoneHandler);

            if (m_onStartHandler) m_onStartHandler(this, &m_ctx, m_request);
        }
        else if (m_status == PROCESS) {
            // 前一个包已经写入网络，通知业务子类可以发下一包
            if (m_onWriteDoneHandler) m_onWriteDoneHandler(this);
        }
        else if (m_status == FINISH) {
            delete this;
        }
    }

    // 发送流里的一个包（注意：不能连续/重叠调用，必须在 OnWriteDone 回调里触发下一次）
    void Write(const Reply& response) {
        m_response = response;
        m_responder.Write(m_response, this);
    }

    // 优雅关闭流
    void Finish(grpc::Status status = grpc::Status::OK) {
        m_status = FINISH;
        m_responder.Finish(status, this);
    }

    void SetOnStartHandler(std::function<void(AsyncServerStreamingCall*, grpc::ServerContext*, const Request&)> handler) { m_onStartHandler = handler; }
    void SetOnWriteDoneHandler(std::function<void(AsyncServerStreamingCall*)> handler) { m_onWriteDoneHandler = handler; }

private:
    enum Status { CREATE, PROCESS, FINISH };
    Status m_status;
    Service* m_service;
    grpc::ServerCompletionQueue* m_cq;
    RegisterMethod m_registerMethod;
    grpc::ServerContext m_ctx;
    Request m_request;
    Reply m_response;
    grpc::ServerAsyncWriter<Reply> m_responder;

    std::function<void(AsyncServerStreamingCall*, grpc::ServerContext*, const Request&)> m_onStartHandler;
    std::function<void(AsyncServerStreamingCall*)> m_onWriteDoneHandler;
};

// ============================================================================
// 3️⃣ Client Streaming 模板 (客户端流：多问一答)
// ============================================================================
template <typename Service, typename Request, typename Reply>
class AsyncClientStreamingCall : public AsyncCall {
public:
    using RegisterMethod = void (Service::*)(grpc::ServerContext*, grpc::ServerAsyncReader<Reply, Request>*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*);

    AsyncClientStreamingCall(Service* service, grpc::ServerCompletionQueue* cq, RegisterMethod registerMethod)
        : m_service(service), m_cq(cq), m_registerMethod(registerMethod), m_responder(&m_ctx), m_status(CREATE) {
        (m_service->*m_registerMethod)(&m_ctx, &m_responder, m_cq, m_cq, this);
    }

    void Proceed(bool ok) override {
        // 如果 ok 为 false，在 PROCESS 阶段意味着客户端已经写完了流（触发了半关闭 EOF）
        if (!ok) {
            if (m_status == PROCESS) {
                m_status = FINISH;
                if (m_onClientHalfCloseHandler) m_onClientHalfCloseHandler(this);
            } else {
                delete this;
            }
            return;
        }

        if (m_status == CREATE) {
            m_status = PROCESS;
            auto next = new AsyncClientStreamingCall<Service, Request, Reply>(m_service, m_cq, m_registerMethod);
            next->SetOnReadHandler(m_onReadHandler);
            next->SetOnClientHalfCloseHandler(m_onClientHalfCloseHandler);

            // 开始向流中发起第一次数据读取
            m_responder.Read(&m_request, this);
        }
        else if (m_status == PROCESS) {
            // 成功读到了一个包
            if (m_onReadHandler) m_onReadHandler(this, m_request);
            // 继续读下一包
            m_responder.Read(&m_request, this);
        }
        else if (m_status == FINISH) {
            delete this;
        }
    }

    // 客户端发完后，服务端给出的最终应答
    void SuccessAndFinish(const Reply& response) {
        m_response = response;
        m_status = FINISH;
        m_responder.Finish(m_response, grpc::Status::OK, this);
    }

    void SetOnReadHandler(std::function<void(AsyncClientStreamingCall*, const Request&)> handler) { m_onReadHandler = handler; }
    void SetOnClientHalfCloseHandler(std::function<void(AsyncClientStreamingCall*)> handler) { m_onClientHalfCloseHandler = handler; }

private:
    enum Status { CREATE, PROCESS, FINISH };
    Status m_status;
    Service* m_service;
    grpc::ServerCompletionQueue* m_cq;
    RegisterMethod m_registerMethod;
    grpc::ServerContext m_ctx;
    Request m_request;
    Reply m_response;
    grpc::ServerAsyncReader<Reply, Request> m_responder;

    std::function<void(AsyncClientStreamingCall*, const Request&)> m_onReadHandler;
    std::function<void(AsyncClientStreamingCall*)> m_onClientHalfCloseHandler;
};

// ============================================================================
// 4️⃣ Bidirectional Streaming 模板 (工业级：完全规避内存覆盖与时序冲突)
// ============================================================================
template <typename Service, typename Request, typename Reply>
class AsyncBidiStreamingCall : public AsyncCall {
private:
    class WriteTag : public AsyncCall {
    public:
        AsyncBidiStreamingCall* parent;
        void Proceed(bool ok) override { parent->OnWriteDone(ok); }
    };

public:
    using RegisterMethod = void (Service::*)(grpc::ServerContext*, grpc::ServerAsyncReaderWriter<Reply, Request>*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*);

    AsyncBidiStreamingCall(Service* service, grpc::ServerCompletionQueue* cq, RegisterMethod registerMethod)
        : m_service(service), m_cq(cq), m_registerMethod(registerMethod), m_responder(&m_ctx), m_status(CREATE) {
        m_writeTag.parent = this;
        (m_service->*m_registerMethod)(&m_ctx, &m_responder, m_cq, m_cq, this);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            delete this;
            return;
        }

        if (m_status == CREATE) {
            m_status = PROCESS;
            auto next = new AsyncBidiStreamingCall<Service, Request, Reply>(m_service, m_cq, m_registerMethod);
            next->SetOnConnectHandler(m_onConnectHandler);
            next->SetOnMsgHandler(m_onMsgHandler);

            if (m_onConnectHandler) m_onConnectHandler(this, &m_ctx);

            // 启动第一次读取
            m_responder.Read(&m_request, this);
        }
        else if (m_status == PROCESS) {
            // 读到了数据，触发业务。
            // ⚠️ 注意：这里我们移除了原先紧跟其后的 Read() 调用！
            if (m_onMsgHandler) m_onMsgHandler(this, m_request);
        }
    }

    // 提供给业务层的主动驱动接口：只有业务层安全把当前 request 处理完了，才能调这个来读下一包
    void FetchNextMsg() {
        m_responder.Read(&m_request, this);
    }

    void OnWriteDone(bool ok) {
        if (!ok) { delete this; return; }
    }

    void Push(const Reply& response) {
        m_response = response;
        m_responder.Write(m_response, &m_writeTag);
    }

    void SetOnConnectHandler(std::function<void(AsyncBidiStreamingCall*, grpc::ServerContext*)> handler) { m_onConnectHandler = handler; }
    void SetOnMsgHandler(std::function<void(AsyncBidiStreamingCall*, const Request&)> handler) { m_onMsgHandler = handler; }

private:
    enum Status { CREATE, PROCESS, FINISH };
    Status m_status;
    Service* m_service;
    grpc::ServerCompletionQueue* m_cq;
    RegisterMethod m_registerMethod;
    grpc::ServerContext m_ctx;
    Request m_request;
    Reply m_response;
    grpc::ServerAsyncReaderWriter<Reply, Request> m_responder;
    WriteTag m_writeTag;

    std::function<void(AsyncBidiStreamingCall*, grpc::ServerContext*)> m_onConnectHandler;
    std::function<void(AsyncBidiStreamingCall*, const Request&)> m_onMsgHandler;
};

// namespace TestAsyncCall {
// void StartServer() {
//     // 基础底层依赖
//     AccountService::AsyncService service;
//     grpc::ServerCompletionQueue* cq = ...;
//     ThreadPool pool(4); // 你的任意参数线程池
//
//     // ========================================================================
//     // 💡 场景 1：模板化 Unary RPC —— 解决你的 SignUp / 数据库隔离问题
//     // ========================================================================
//     auto signUpCall = new AsyncUnaryCall<AccountService::AsyncService, SignUpRequest, SignUpReply>(
//         &service, cq, &AccountService::AsyncService::RequestSignUp
//     );
//
//     signUpCall->SetBusinessHandler([&pool, cq](auto* call, auto* context, const auto& request, auto* response) {
//         // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
//         pool.enqueue([call, request]() {
//             // 执行耗时的 DBSQLCipher 操作
//             auto params = std::make_shared<SignUpRequest>(request);
//             std::shared_ptr<SignUpReply> result = DBSQLCipher::instance().sign_up(params);
//
//             if (!result) {
//                 call->Error(grpc::StatusCode::INTERNAL, "SignUp Database Failed");
//             } else {
//                 call->Success(*result); // 模板内自动调用 Finish 销毁自己
//             }
//         });
//     });
//
//     // ========================================================================
//     // 💡 场景 2：模板化 Server Streaming —— 解决你的 DownloadFile 文件下载流
//     // ========================================================================
//     auto downloadCall = new AsyncServerStreamingCall<AccountService::AsyncService, DownloadRequest, DownloadResponse>(
//         &service, cq, &AccountService::AsyncService::RequestDownloadFile
//     );
//
//     // 托管文件句柄的上下文结构体
//     struct DownloadContext {
//         std::ifstream file;
//         std::int64_t offset = 0;
//         std::int64_t totalSize = 0;
//     };
//     auto dlCtx = std::make_shared<DownloadContext>();
//
//     downloadCall->SetOnStartHandler([dlCtx](auto* call, auto* ctx, const auto& request) {
//         // 初始化打开文件
//         dlCtx->file.open(request.filename(), std::ios::binary | std::ios::in);
//         // 这里可以计算大小，并发送首个块
//         DownloadResponse resp;
//         // resp.set_chunk(...)
//         call->Write(resp);
//     });
//
//     downloadCall->SetOnWriteDoneHandler([dlCtx](auto* call) {
//         // 前一个块发完了，网卡空闲了，在这里续读文件发下一个块
//         if (dlCtx->file.good()) {
//             DownloadResponse resp;
//             // 读取下一块填充 resp
//             call->Write(resp);
//         } else {
//             call->Finish(); // 文件读完了，宣告流结束，模板自动 delete this 释放内存
//         }
//     });
//
//     // ========================================================================
//     // 💡 场景 3：模板化 Bidirectional Streaming —— 解决你的 PTS IM 实时长链接推拉
//     // ========================================================================
//     auto ptsStream = new AsyncBidiStreamingCall<AccountService::AsyncService, StreamMessage, StreamMessage>(
//         &service, cq, &AccountService::AsyncService::RequestConnectStream
//     );
//
//     ptsStream->SetOnConnectHandler([](auto* call, auto* ctx) {
//         std::cout << "IM Client Connected to PTS Stream!" << std::endl;
//     });
//
//     ptsStream->SetOnMsgHandler([](auto* call, const auto& clientMsg) {
//         std::cout << "Received Heartbeat or client action: " << clientMsg.DebugString() << std::endl;
//         // 可以立刻回应
//         StreamMessage ack;
//         ack.set_type("ACK");
//         call->Push(ack);
//     });
// }
//
// }
