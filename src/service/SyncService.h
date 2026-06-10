#include "headers.h"
#include "utils.h"
#include "DBSQLCipher.h"

using Utils::success_status;
using Utils::error_status;
using Utils::log_grpc_context;
using Utils::set_success;
using Utils::set_error;

// 服务端实现
class SyncServiceImpl final : public SyncService::Service {
public:
    // GetLastID 获取当前账号各事件类型的最新 PTS 值，供客户端判断是否有更新需要同步
    Status GetLastID(ServerContext* context, const AuthRequest* request, GetLastIDReply* response) override {
        using Request = AuthRequest;
        using Reply   = GetLastIDReply;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        shared_ptr<Reply> result = db_sqlcipher.get_last_id(params);
        if(!result) {
            return Status(StatusCode::INTERNAL, std::format("GetLastID failed."));
        }
        response->CopyFrom(*result);
        return Status::OK;
    }

    // Sync 双向流式 RPC，客户端发送 SyncRequest，服务器按每个 SyncRequestTag 返回增量 body
    Status Sync(ServerContext* context, ServerReaderWriter<SyncReply, SyncRequest>* stream) override {
        using Request = SyncRequest;
        using Reply   = SyncReply  ;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !stream) {
            return Status(StatusCode::INTERNAL, std::format("Invalid stream pointer"));
        }
        // =========================== 执行操作 =========================== //
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();

        Request request;
        while(stream->Read(&request)) {
            // 发送鉴权结果 header
            SyncReply header_reply;
            auto* header = header_reply.mutable_header();
            header->mutable_request()->CopyFrom(request);

            // 获取身份校验信息
            if(!request.has_request_user_auth()) {
                set_error(header->mutable_result(), ErrorCode::AUTH_INFO_NONE);
                stream->Write(header_reply);
                continue;
            }

            // 身份校验
            auto auth_request = make_shared<AuthRequest>(request.request_user_auth());
            auto auth_reply = db_sqlcipher.auth_check(auth_request);
            if(!auth_reply || !auth_reply->result().has_success_code()) {
                set_error(header->mutable_result(), ErrorCode::AUTH_FAILED);
                stream->Write(header_reply);
                continue;
            }

            // 获取账号 ID
            auto self_account_id = auth_reply->account_id();
            if(self_account_id.empty()) {
                set_error(header->mutable_result(), ErrorCode::AUTH_FAILED);
                stream->Write(header_reply);
                continue;
            }

            set_success(header->mutable_result(), SuccessCode::SUCCESS);
            stream->Write(header_reply);

            // 逐个 SyncRequestTag 返回 body
            for(int i = 0; i < request.sync_tag_size(); ++i) {
                const auto& request_tag = request.sync_tag(i);
                auto pts_type = Utils::Transform::to_string(request_tag.type());
                if(pts_type.empty()) {
                    continue;
                }

                auto [success_get_records, records] = db_sqlcipher.get_pts_records(
                    self_account_id,
                    pts_type,
                    request_tag.id_start(),
                    request_tag.id_end()
                );
                if(!success_get_records) {
                    continue;
                }

                Reply reply;
                auto* body = reply.mutable_body();
                body->mutable_tag()->CopyFrom(request_tag);
                body->mutable_tag()->set_count(static_cast<int64_t>(records.size()));

                // CONTACTS：返回联系人增量
                if(request_tag.type() == SyncType::SYNC_CONTACTS) {
                    std::set<string> unique_account_ids;
                    // 获取事件 ID
                    for(auto& row : records) {
                        auto ite_event_id = row.find("pts_event_id");
                        if(ite_event_id == row.end()) {
                            continue;
                        }
                        auto event_id = get_string(ite_event_id->second);
                        if(event_id.empty()) {
                            continue;
                        }
                        unique_account_ids.insert(event_id);
                    }

//                     auto* infos = body->mutable_contact_infos();
//                     for(auto& target_account_id : unique_account_ids) {
//                         auto [ok_user_info, contact_info] = db_sqlcipher.get_active_account_info_interface(target_account_id);
//                         if(!ok_user_info) {
//                             continue;
//                         }
// 
//                         // 这里按“联系人增量”语义返回：若是好友则附加 friend_info
//                         auto [ok_friend_info, friend_info] = db_sqlcipher.get_friend_info_interface(self_account_id, target_account_id);
//                         if(ok_friend_info && friend_info) {
//                             contact_info.mutable_friend_info()->CopyFrom(*friend_info);
//                         }
// 
//                         infos->add_contact_infos()->CopyFrom(contact_info);
//                     }
                }

                // MESSAGES：返回完整消息明细 + 文件事件明细
                if(request_tag.type() == SyncType::SYNC_MESSAGES) {
//                     auto* messages = body->mutable_messages();
//                     auto* file_events = body->mutable_file_events();
//                     for(auto& row : records) {
//                         auto ite_sub_type = row.find("pts_sub_type");
//                         auto ite_event_id = row.find("pts_event_id");
//                         if(ite_sub_type == row.end() || ite_event_id == row.end()) {
//                             continue;
//                         }
//                         auto sub_type = get_string(ite_sub_type->second);
//                         auto event_id = get_string(ite_event_id->second);
//                         if(sub_type == "MESSAGE_TEXT") {
//                             auto [ok_record, record] = db_sqlcipher.get_message_record_interface(event_id);
//                             if(!ok_record) {
//                                 continue;
//                             }
//                             auto* msg = messages->add_messages();
//                             msg->set_server_message_id  (record.server_message_id  );
//                             msg->set_client_message_id  (record.client_message_id  );
//                             msg->set_sender_account_id  (record.sender_account_id  );
//                             msg->set_receiver_account_id(record.receiver_account_id);
//                             msg->set_short_text         (record.short_text         );
//                             msg->set_timestamp          (record.timestamp          );
//                             msg->set_event_sub_type     (sub_type                  );
//                             continue;
//                         }
// 
//                         if(sub_type == "FILE_UPLOAD") {
//                             auto [ok_file, file_metadata, file_status] = db_sqlcipher.get_file_info_interface(event_id);
//                             if(!ok_file) {
//                                 continue;
//                             }
//                             auto* file_event = file_events->add_file_events();
//                             file_event->mutable_file_metadata()->CopyFrom(file_metadata);
//                             file_event->set_file_id       (event_id   );
//                             file_event->set_file_status   (file_status);
//                             file_event->set_event_sub_type(sub_type   );
//                             continue;
//                         }
//                     }
                }

                // GROUPS 暂无数据库落地与事件明细，保持空列表，仅通过 count 反馈增量数量

                stream->Write(reply);
            }
        }
        return Status::OK;
    }

};

namespace Chat::Service::Sync::Rpc::AsyncCall {

class GetLastID : public AsyncUnaryCall<SyncService::AsyncService, AuthRequest, GetLastIDReply> {
public:
    // 构造函数：同时接收服务、完成队列和线程池
    GetLastID(SyncService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncUnaryCall<SyncService::AsyncService, AuthRequest, GetLastIDReply>(service, cq, &SyncService::AsyncService::RequestGetLastID), m_thread_pool(thread_pool) {
        // 不要捕获 this，直接显式捕获局部变量 thread_pool 确保线程安全
        SetBusinessHandler([thread_pool](auto* call, auto* context, const auto& request, auto* response) {
            // 1. 无缝扔进你手写的任意参数线程池，绝不卡死 gRPC 核心网络流
            thread_pool->enqueue([call, request]() {
                // 执行耗时的 DBSQLCipher 操作
                std::shared_ptr<GetLastIDReply> result = DBSQLCipher::instance().get_last_id(std::make_shared<AuthRequest>(std::move(request)));

                if (!result) {
                    call->Error(grpc::StatusCode::INTERNAL, "GetLastID Database Failed");
                } else {
                    call->Success(*result); // 模板内自动调用 Finish 销毁自己
                }
            });
        });
    }

protected:
    // 重写孵化虚函数，确保高并发时，基类自动 new 出来的下一个监听器依然是 GetLastID 子类，而不是空壳基类
    void CloneNext(SyncService::AsyncService* service, grpc::ServerCompletionQueue* cq) override {
        new GetLastID(service, cq, m_thread_pool);
    }

private:
    ThreadPool* m_thread_pool;
};

}

namespace Chat::Service::Sync::Rpc::AsyncCall {

// 用于在离开作用域时自动调用 FetchNextMsg 的轻量级 RAII 守卫
class FetchNextMsgGuard {
public:
    explicit FetchNextMsgGuard(AsyncBidiStreamingCall<SyncService::AsyncService, SyncRequest, SyncReply>* call) : m_call(call) {}
    ~FetchNextMsgGuard() { if (m_call) m_call->FetchNextMsg(); }
    FetchNextMsgGuard(const FetchNextMsgGuard&) = delete;
    FetchNextMsgGuard& operator=(const FetchNextMsgGuard&) = delete;
private:
    AsyncBidiStreamingCall<SyncService::AsyncService, SyncRequest, SyncReply>* m_call;
};

class SyncCallInstance : public AsyncBidiStreamingCall<SyncService::AsyncService, SyncRequest, SyncReply> {
public:
    SyncCallInstance(SyncService::AsyncService* service, grpc::ServerCompletionQueue* cq, ThreadPool* thread_pool)
        : AsyncBidiStreamingCall<SyncService::AsyncService, SyncRequest, SyncReply>(service, cq, &SyncService::AsyncService::RequestSync), m_thread_pool(thread_pool)
    {
        SetOnConnectHandler([this](auto* call, auto* context) {
            Utils::log_grpc_context(context);
        });

        SetOnMsgHandler([this](auto* call, const SyncRequest& request) {
            // TODO:客户端异常退出会导致锁竞争而崩溃
            auto request_ptr = std::make_shared<SyncRequest>(request);
            m_thread_pool->enqueue([this, call, request_ptr]() {
                FetchNextMsgGuard guard(call);
                DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();

                // 鉴权
                SyncReply header_reply;
                auto* header = header_reply.mutable_header();
                header->mutable_request()->CopyFrom(*request_ptr);
                // 没有身份信息则返回失败
                if (!request_ptr->has_request_user_auth()) {
                    header->mutable_result()->set_error_code(ErrorCode::AUTH_INFO_NONE);
                    call->Push(header_reply);
                    return;
                }
                // 身份验证
                auto auth_request = std::make_shared<AuthRequest>(request_ptr->request_user_auth());
                auto auth_reply = db_sqlcipher.auth_check(auth_request);
                if (!auth_reply || !auth_reply->result().has_success_code()) {
                    header->mutable_result()->set_error_code(ErrorCode::AUTH_FAILED);
                    call->Push(header_reply);
                    return;
                }
                // 获取账号 ID
                std::string self_account_id = auth_reply->account_id();
                if (self_account_id.empty()) {
                    header->mutable_result()->set_error_code(ErrorCode::AUTH_FAILED);
                    call->Push(header_reply);
                    return;
                }

                // 鉴权通过，下发成功的 Header
                header->mutable_result()->set_success_code(SuccessCode::SUCCESS);
                call->Push(header_reply);

                // =========================================================== //

                // TODO: 事件过滤后再返回给客户端

                // 根据请求查找数据并返回给客户端
                for (int i = 0; i < request_ptr->sync_tag_size(); ++i) {
                    // 读取记录
                    const auto& request_tag = request_ptr->sync_tag(i);
                    auto id_start = request_tag.id_start();
                    auto id_end   = request_tag.id_end  ();
                    id_start = std::max(decltype(id_start)(0), id_start);
                    id_end   = std::max(decltype(id_end  )(0), id_end  );
                    if(id_start >= id_end) { continue; }

                    // 限制一次最多只返回 100 条记录，需要更多记录时需要由客户端继续拉取
                    const int pts_limit = 100;
                    id_end      = std::min(id_start + pts_limit, id_end            );  // 按大的分类读取前 100 条返回
                    // id_start = std::max(id_start            , id_end - pts_limit);  // 按大的分类读取后 100 条返回

                    // 读取记录
                    auto [success_get_records, records] = db_sqlcipher.get_pts_records(self_account_id, Utils::Transform::to_string(request_tag.type()), id_start, id_end);
                    if (!success_get_records) { continue; }

                    // 即使没有消息也进行推送，告知客户端该消息已经被处理

                    SyncReply body_reply;
                    auto* body = body_reply.mutable_body();
                    auto* tag  = body->mutable_tag();
                    tag->CopyFrom(request_tag);
                    tag->set_count(0);  // 需要读取实际写入的数量而非查询到的数量
                    int64_t count = 0;  // 记录实际添加的记录数

                    LOG_TRACE(std::format("pts_type : {}, start id : {}, end id : {}", Utils::Transform::to_string(request_tag.type()), id_start, id_end));

                    // 因为有 start/end/count，所以需要按组返回数据
                    for (auto& row : records) {
                        auto get_string = [](const SqlRow&row, const std::string&key, const std::string default_value = std::string{}) -> string {
                            if(row.contains(key)) { return to_string(row.at(key)); }
                            return default_value;
                        };
                        auto get_first_string = [](const SqlRows&rows, const std::string&key, const std::string default_value = std::string{}) -> string {
                            for(auto&row:rows) {
                                if(row.contains(key)) {
                                    return to_string(row.at(key));
                                }
                            }
                            return default_value;
                        };

                        auto event_id       = get_string(row, "pts_event_id");  // 获取事件 ID  // 必须有效
                        auto event_sub_type = get_string(row, "pts_sub_type");  // 获取子类型   // 可以不存在
                        if (event_id.empty()) { continue; }

                        // 如果用户请求获取账号相关的更新，则从查找到的记录里面提取并填充
                        if (request_tag.type() == SyncType::SYNC_CONTACTS) {
                            // 添加数据 // 有效才允许添加
                            // auto* sync_contact = body->mutable_contacts()->add_contact();
                            ::TaoTieChat::SyncContact  __contact;
                            ::TaoTieChat::SyncContact* sync_contact = &__contact;
                            // 按照子类型填充数据
                            if(event_sub_type == "FRIEND_REQUEST") {
                                sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_REUQEST);
                                // 他人向自己发来的好友申请
                                // 获取请求内容(friend_requests)及好友 ID 和资料()进行填充返回
                                auto contact_request = sync_contact->mutable_request();
                                // 获取申请记录
                                auto[success_request_info, rows_request_info] = db_sqlcipher.get_friend_request_info_by_event_id(event_id);
                                if(!success_request_info) { continue; }
                                // 获取发起者的账号 ID
                                auto request_account_id = get_first_string(rows_request_info, "request_account_id");
                                if(request_account_id.empty()) { continue; }
                                // 校验接收者为自己
                                auto target_account_id = get_first_string(rows_request_info, "target_account_id");
                                if(target_account_id != self_account_id) { continue; }
                                // 填充发送时间
                                auto request_timestamp = get_first_string(rows_request_info, "timestamp");
                                if(!request_timestamp.empty()) { contact_request->set_time(request_timestamp); }
                                // 填充附加消息
                                auto request_message = get_first_string(rows_request_info, "message");
                                if(!request_message.empty()) { contact_request->set_message(request_message); }
                                // 获取发起者的个人资料
                                auto [ok_user_info, request_contact_info] = db_sqlcipher.get_active_account_info_interface(request_account_id);
                                if (ok_user_info) {
                                    contact_request->mutable_contact_request()->CopyFrom(request_contact_info);  // 复制账号资料
                                    // 获取是否已经是好友关系  // 多端登陆时可能已经从其他端同意
                                    auto [ok_friend_info, friend_info] = db_sqlcipher.get_friend_info_interface(self_account_id, request_account_id);
                                    if (ok_friend_info && friend_info) {
                                        // 是好友关系时才复制好友关系的内容
                                        contact_request->mutable_contact_request()->mutable_friend_info()->CopyFrom(*friend_info.get());
                                    }
                                }
                            }
                            else if(event_sub_type == "FRIEND_ACCEPT") {
                                // 自己加他人为好友时，他人的回复
                                sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_REPLY);
                                auto contact_reply = sync_contact->mutable_reply();
                                // 获取申请记录
                                auto[success_request_info, rows_request_info] = db_sqlcipher.get_friend_request_info_by_event_id(event_id);
                                if(!success_request_info) { continue; }
                                // 校验申请发送者为自己
                                auto request_account_id = get_first_string(rows_request_info, "request_account_id");
                                if(request_account_id != self_account_id) { continue; }  // 不是自己的申请
                                // 获取接收者账号
                                auto target_account_id = get_first_string(rows_request_info, "target_account_id");
                                if(request_account_id.empty()) { continue; }
                                // 填充发送时间
                                auto request_timestamp = get_first_string(rows_request_info, "timestamp");
                                if(!request_timestamp.empty()) { contact_reply->set_time(request_timestamp); }
                                // 填充附加消息
                                auto request_message = get_first_string(rows_request_info, "message");
                                if(!request_message.empty()) { contact_reply->set_message(request_message); }
                                // 获取被添加者的个人资料
                                auto [ok_user_info, reply_contact_info] = db_sqlcipher.get_active_account_info_interface(target_account_id);
                                if (ok_user_info) {
                                    contact_reply->mutable_contact_reply()->CopyFrom(reply_contact_info);  // 复制账号资料
                                    // 获取是否已经是好友关系  // 多端登陆时可能已经从其他端同意
                                    auto [ok_friend_info, friend_info] = db_sqlcipher.get_friend_info_interface(self_account_id, request_account_id);
                                    if (ok_friend_info && friend_info) {
                                        contact_reply->mutable_contact_reply()->mutable_friend_info()->CopyFrom(*friend_info.get());
                                    }
                                }
                            }
                            // 成功才填充
                            auto contact = body->mutable_contacts()->add_contact();
                            contact->CopyFrom(__contact);
                            count++;
                        }

                        // MESSAGES (消息与文件详情增量)
                        if (request_tag.type() == SyncType::SYNC_MESSAGES) {
//                         auto* messages    = body->mutable_messages   ();
//                         auto* file_events = body->mutable_file_events();
//
//                         for (auto& row : records) {
//                             auto ite_sub_type = row.find("pts_sub_type");
//                             auto ite_event_id = row.find("pts_event_id");
//                             if (ite_sub_type == row.end() || ite_event_id == row.end()) { continue; }
//
//                             std::string sub_type = get_string(ite_sub_type->second);
//                             std::string event_id = get_string(ite_event_id->second);
//
//                             if (sub_type == "MESSAGE_TEXT") {
//                                 auto [ok_record, record] = db_sqlcipher.get_message_record_interface(event_id);
//                                 if (!ok_record) { continue; }
//
//                                 auto* msg = messages->add_messages();
//                                 msg->set_server_message_id  (record.server_message_id);
//                                 msg->set_client_message_id  (record.client_message_id);
//                                 msg->set_sender_account_id  (record.sender_account_id);
//                                 msg->set_receiver_account_id(record.receiver_account_id);
//                                 msg->set_short_text         (record.short_text);
//                                 msg->set_timestamp          (record.timestamp);
//                                 msg->set_event_sub_type     (sub_type);
//                             }
//                             else if (sub_type == "FILE_UPLOAD") {
//                                 auto [ok_file, file_metadata, file_status] = db_sqlcipher.get_file_info_interface(event_id);
//                                 if (!ok_file) { continue; }
//
//                                 auto* file_event = file_events->add_file_events();
//                                 file_event->mutable_file_metadata()->CopyFrom(file_metadata);
//                                 file_event->set_file_id       (event_id);
//                                 file_event->set_file_status   (file_status);
//                                 file_event->set_event_sub_type(sub_type);
//                             }
//                         }
                        }

                        // ...
                        if (request_tag.type() == SyncType::SYNC_GROUPS) {
                            // ...
                        }
                    }

                    tag->set_count(count);
                    // 将记录发送给客户端  // TODO:客户端意外断开会导致服务端崩溃
                    std::lock_guard<std::mutex> lock(m_write_mutex);
                    call->Push(body_reply);
                }
                // TODO:需要阻塞等待新消息发生并实时同步推送到客户端
            });
        });
    }

private:
    ThreadPool* m_thread_pool;
    std::mutex  m_write_mutex;
};

} // namespace Chat::Service::Sync::Rpc::AsyncCall