#include "headers.h"
#include "utils.h"
#include "DBSQLCipher.h"

using std::string;
using std::map;
using std::set;

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
        using Reply = SyncReply;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if(!context || !stream) {
            return Status(StatusCode::INTERNAL, std::format("Invalid stream pointer"));
        }
        // =========================== 执行操作 =========================== //
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();

        auto type_to_string = [](SyncType type) -> string {
            if(type == SyncType::CONTACTS) { return "CONTACTS"; }
            if(type == SyncType::GROUPS  ) { return "GROUPS";   }
            if(type == SyncType::MESSAGES) { return "MESSAGES"; }
            return string{};
        };

        SyncRequest sync_request;
        while(stream->Read(&sync_request)) {
            // 发送鉴权结果 header
            SyncReply header_reply;
            auto* header = header_reply.mutable_header();
            header->mutable_request()->CopyFrom(sync_request);

            if(!sync_request.has_request_user_auth()) {
                set_error(header->mutable_result(), ErrorCode::AUTH_INFO_NONE);
                stream->Write(header_reply);
                continue;
            }
            auto auth_request = make_shared<AuthRequest>(sync_request.request_user_auth());
            auto auth_reply = db_sqlcipher.auth_check(auth_request);
            if(!auth_reply || !auth_reply->result().has_success_code()) {
                set_error(header->mutable_result(), ErrorCode::AUTH_FAILED);
                stream->Write(header_reply);
                continue;
            }
            auto self_account_id = auth_reply->account_id();
            if(self_account_id.empty()) {
                set_error(header->mutable_result(), ErrorCode::AUTH_FAILED);
                stream->Write(header_reply);
                continue;
            }
            set_success(header->mutable_result(), SuccessCode::SUCCESS);
            stream->Write(header_reply);

            // 逐个 SyncRequestTag 返回 body
            for(int i = 0; i < sync_request.sync_tag_size(); ++i) {
                const auto& request_tag = sync_request.sync_tag(i);
                auto pts_type = type_to_string(request_tag.type());
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

                Reply body_reply;
                auto* body = body_reply.mutable_body();
                body->mutable_tag()->CopyFrom(request_tag);
                body->set_count(static_cast<int64_t>(records.size()));

                // CONTACTS：返回联系人增量
                if(request_tag.type() == SyncType::CONTACTS) {
                    std::set<string> unique_account_ids;
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

                    auto* infos = body->mutable_contact_infos();
                    for(auto& target_account_id : unique_account_ids) {
                        auto [ok_user_info, contact_info] = db_sqlcipher.get_active_account_info_interface(target_account_id);
                        if(!ok_user_info) {
                            continue;
                        }

                        // 这里按“联系人增量”语义返回：若是好友则附加 friend_info
                        auto [ok_friend_info, friend_info] = db_sqlcipher.get_friend_info_interface(self_account_id, target_account_id);
                        if(ok_friend_info && friend_info) {
                            contact_info.mutable_friend_info()->CopyFrom(*friend_info);
                        }

                        infos->add_contact_infos()->CopyFrom(contact_info);
                    }
                }

                // MESSAGES：返回完整消息明细 + 文件事件明细
                if(request_tag.type() == SyncType::MESSAGES) {
                    auto* messages = body->mutable_messages();
                    auto* file_events = body->mutable_file_events();
                    for(auto& row : records) {
                        auto ite_sub_type = row.find("pts_sub_type");
                        auto ite_event_id = row.find("pts_event_id");
                        if(ite_sub_type == row.end() || ite_event_id == row.end()) {
                            continue;
                        }
                        auto sub_type = get_string(ite_sub_type->second);
                        auto event_id = get_string(ite_event_id->second);
                        if(sub_type == "MESSAGE_TEXT") {
                            auto [ok_record, record] = db_sqlcipher.get_message_record_interface(event_id);
                            if(!ok_record) {
                                continue;
                            }
                            auto* msg = messages->add_messages();
                            msg->set_server_message_id(record.server_message_id);
                            msg->set_client_message_id(record.client_message_id);
                            msg->set_sender_account_id(record.sender_account_id);
                            msg->set_receiver_account_id(record.receiver_account_id);
                            msg->set_short_text(record.short_text);
                            msg->set_timestamp(record.timestamp);
                            msg->set_event_sub_type(sub_type);
                            continue;
                        }

                        if(sub_type == "FILE_UPLOAD") {
                            auto [ok_file, file_metadata, file_status] = db_sqlcipher.get_file_info_interface(event_id);
                            if(!ok_file) {
                                continue;
                            }
                            auto* file_event = file_events->add_file_events();
                            file_event->set_file_id(event_id);
                            file_event->mutable_file_metadata()->CopyFrom(file_metadata);
                            file_event->set_file_status(file_status);
                            file_event->set_event_sub_type(sub_type);
                            continue;
                        }
                    }
                }

                // GROUPS 暂无数据库落地与事件明细，保持空列表，仅通过 count 反馈增量数量

                stream->Write(body_reply);
            }
        }
        return Status::OK;
    }
};
