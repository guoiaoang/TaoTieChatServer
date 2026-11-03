#include "headers.h"
#include "utils.h"
#include "DBSQLCipher.h"

using Utils::success_status;
using Utils::error_status;
using Utils::log_grpc_context;
using Utils::set_success;
using Utils::set_error;

// 线程安全的 Stream 写封装
class SyncStreamWriter {
public:
    explicit SyncStreamWriter(ServerReaderWriter<SyncReply, SyncRequest>* stream)
        : stream_(stream) {}

    bool Write(const SyncReply& reply) {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_->Write(reply);
    }

private:
    ServerReaderWriter<SyncReply, SyncRequest>* stream_;
    std::mutex mutex_;
};

// 支持多设备同时在线的 Session 管理器
class SessionManager {
public:
    static SessionManager& instance() {
        static SessionManager mgr;
        return mgr;
    }

    /**
     * @brief 注册 Session
     * @param account_id 账号 ID
     * @param device_id 设备唯一标识（如 "iPhone_15_PRO", "PC_WIN11"，若无可以传空或自动生成）
     * @param writer 线程安全的流写入器
     */
    void register_session(const std::string& account_id, const std::string& device_id, std::shared_ptr<SyncStreamWriter> writer) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 如果该账号还没有任何设备在线，会自动创建对应的 map
        sessions_[account_id][device_id] = writer;

        LOG_INFO(std::format("User {} logged in from device {}. Total active devices: {}",
                 account_id, device_id, sessions_[account_id].size()));
    }

    /**
     * @brief 注销指定设备的 Session
     * @param account_id 账号 ID
     * @param device_id 设备唯一标识
     */
    void unregister_session(const std::string& account_id, const std::string& device_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto user_it = sessions_.find(account_id);
        if (user_it != sessions_.end()) {
            user_it->second.erase(device_id);
            LOG_INFO(std::format("User {} logged out device {}.", account_id, device_id));

            // 如果该用户已经没有在线设备，清理该用户的 Key，释放内存
            if (user_it->second.empty()) {
                sessions_.erase(user_it);
                LOG_INFO(std::format("User {} has no active devices left, session cleared.", account_id));
            }
        }
    }

    /**
     * @brief 主动推送消息给该账号下的所有在线设备
     * @param account_id 接收方账号 ID
     * @param push_reply 推送的 Protobuf 结构体
     * @return int 成功推送到的设备数量
     */
    int push_to_user(const std::string& account_id, const SyncReply& push_reply) {
        std::vector<std::shared_ptr<SyncStreamWriter>> target_writers;

        // 1. 在锁保护下快速复制出当前所有在线设备的 writer 句柄，避免加锁执行网络 IO 导致阻塞
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto user_it = sessions_.find(account_id);
            if (user_it != sessions_.end()) {
                target_writers.reserve(user_it->second.size());
                for (const auto& [dev_id, writer] : user_it->second) {
                    target_writers.push_back(writer);
                }
            }
        }

        // 2. 锁外并行或依次推送给各个设备
        int success_count = 0;
        for (const auto& writer : target_writers) {
            if (writer && writer->Write(push_reply)) {
                success_count++;
            }
        }

        return success_count; // 返回成功送达的设备数（如果为 0 则代表全端离线）
    }

private:
    SessionManager() = default;
    ~SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    // 数据结构：account_id -> { device_id -> SyncStreamWriter }
    std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<SyncStreamWriter>>> sessions_;
    std::mutex mutex_;
};

// SyncReply pts_line(shared_ptr<Data::PtsInfo> pts_info) {
//     do {
//         if (!pts_info) { break; }
//         if ( pts_info->id              <  0) { break; }
//         if ( pts_info->account_id  .empty()) { break; }
//         if ( pts_info->pts_type    .empty()) { break; }
//         if ( pts_info->pts_sub_type.empty()) { break; }
//         if ( pts_info->pts_value       <= 0) { break; }
//         if ( pts_info->pts_event_id.empty()) { break; }
//         if ( pts_info->update_time .empty()) { break; }
// 
//         auto pts_type = Utils::Transform::to_sync_type(pts_info->pts_type);
//         if ( pts_type == SyncType::SYNC_UNSET) { break; }
//         // const auto&pts_sub_type = pts_info->pts_sub_type;
//         // const auto&account_id   = pts_info->account_id;
// 
//         DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
//         // db_sqlcipher.get_pts_record(pts_info->pts_event_id);
// 
//         SyncReply body_reply;
//         auto* body = body_reply.mutable_body();
//         auto* tag  = body->mutable_tag();
//         // tag->CopyFrom(request_tag);
//         tag->set_count(0);  // 需要读取实际写入的数量而非查询到的数量
//         int64_t count = 0;  // 记录实际添加的记录数
// 
//         // 如果用户请求获取账号相关的更新，则从查找到的记录里面提取并填充
//         if (pts_type == SyncType::SYNC_CONTACTS) {
//             // 添加数据 // 有效才允许添加
//             // auto* sync_contact = body->mutable_contacts()->add_contact();
//             ::TaoTieChat::SyncContact  __contact;
//             ::TaoTieChat::SyncContact* sync_contact = &__contact;
//             // 按照子类型填充数据
//             if (pts_info->pts_sub_type == "FRIEND_REQUEST") {
//                 sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_REUQEST);
//                 // 他人向自己发来的好友申请
//                 // 获取请求内容(friend_requests)及好友 ID 和资料()进行填充返回
//                 auto contact_request = sync_contact->mutable_request();
//                 // 获取申请记录
//                 auto[success_request_info, rows_request_info] = db_sqlcipher.get_friend_request_info_by_event_id(pts_info->pts_event_id);
//                 if (!success_request_info) { continue; }
//                 contact_request->set_message_id(pts_info->pts_event_id);
//                 // 获取发起者的账号 ID
//                 auto [rrai, request_account_id] = get_first_string(rows_request_info, "request_account_id");
//                 if (request_account_id.empty()) { continue; }
//                 // 校验接收者为自己
//                 auto [rtai, target_account_id] = get_first_string(rows_request_info, "target_account_id");
//                 if (target_account_id != pts_info->account_id) { continue; }
//                 // 填充发送时间
//                 auto [rrt, request_timestamp] = get_first_string(rows_request_info, "timestamp");
//                 if (!request_timestamp.empty()) {
//                     const auto&tp = Utils::to_time_point(request_timestamp);
//                     const auto&ts = Utils::to_proto_timestamp(tp);
//                     contact_request->mutable_timestamp()->CopyFrom(ts);
//                 }
//                 // 填充附加消息
//                 auto [rrmt, request_message_text] = get_first_string(rows_request_info, "message");
//                 if (!request_message_text.empty()) { contact_request->set_message_text(request_message_text); }
//                 // 获取发起者的个人资料
//                 auto [ok_user_info, request_contact_info] = db_sqlcipher.get_active_account_info_interface(request_account_id);
//                 if (ok_user_info) {
//                     contact_request->mutable_contact_request()->CopyFrom(request_contact_info);  // 复制账号资料
//                     // 获取是否已经是好友关系  // 多端登陆时可能已经从其他端同意
//                     auto [ok_friend_info, friend_info] = db_sqlcipher.get_friend_info_interface(pts_info->account_id, request_account_id);
//                     if (ok_friend_info && friend_info) {
//                         // 是好友关系时才复制好友关系的内容
//                         contact_request->mutable_contact_request()->mutable_friend_info()->CopyFrom(*friend_info.get());
//                     }
//                 }
//             }
//             else if (pts_info->pts_sub_type == "FRIEND_ACCEPT") {
//                 // 同意他人好友申请时更新好友列表即可，不需要广播允许状态
//                 // 表 friends 目前没有 event_id 字段
//                 // 简化处理，只推送新添加了好友即可
//                 sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_REPLY);
//                 // 从好友表中找对方，进行状态推送
//                 // 基础信息
//                 auto [success_account, target_account_info] = db_sqlcipher.get_active_account_info_interface(pts_info->pts_event_id);
//                 if (!success_account) { continue; }
//                 // 好友信息
//                 auto [success_ship, friend_info] = db_sqlcipher.get_friend_info_interface(pts_info->account_id, pts_info->pts_event_id);
//                 if (!success_ship) { continue; }
//                 // 复制回传
//                 auto contact_info = sync_contact->mutable_ship()->mutable_contact_ship();
//                 LOG_TRACE(std::format("target_account_info : \n{}", target_account_info.DebugString()));
//                 LOG_TRACE(std::format("friend_info : \n{}", friend_info->DebugString()));
//                 contact_info->mutable_base_info()->CopyFrom(target_account_info.base_info());  // 基础信息
//                 contact_info->mutable_friend_info()->CopyFrom(*friend_info.get());  // 好友信息
//             }
//             // 好友资料变更  // 广播有利于多客户端通知/同步
//             else if (pts_info->pts_sub_type == "CONTACT_INFO_CHANGE") {
//                 sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_INFO);
//                 // 从好友表中找对方，进行状态推送
//                 // 基础信息
//                 auto [success_account, target_account_info] = db_sqlcipher.get_active_account_info_interface(pts_info->pts_event_id);
//                 if (!success_account) { continue; }
//                 // 好友信息
//                 auto [success_ship, friend_info] = db_sqlcipher.get_friend_info_interface(pts_info->account_id, pts_info->pts_event_id);
//                 // if (!success_ship) { continue; }
//                 // TODO:自己信息更新时也会触发该操作，当自己不是自己好友时需要从自己资料中查询
//                 if ( success_ship && friend_info && !friend_info->account_id().empty()) {
//                     // 复制回传
//                     auto contact_info = sync_contact->mutable_update()->mutable_contact_update();
//                     LOG_TRACE(std::format("target_account_info : \n{}", target_account_info.DebugString()));
//                     LOG_TRACE(std::format("friend_info : \n{}", friend_info->DebugString()));
//                     // 基础信息
//                     contact_info->mutable_base_info()->CopyFrom(target_account_info.base_info());
//                     // 好友信息
//                     contact_info->mutable_friend_info()->CopyFrom(*friend_info.get());
//                 }
//                 else {
//                     // 不是好友时不需要查询好友信息
//                     auto [success_self, self_info] = db_sqlcipher.get_active_account_info_interface(pts_info->account_id);
//                     if ( success_self && self_info.has_base_info() && !self_info.base_info().account_id().empty()) {
//                         // 复制回传
//                         auto contact_info = sync_contact->mutable_update()->mutable_contact_update();
//                         LOG_TRACE(std::format("target_account_info : \n{}", target_account_info.DebugString()));
//                         LOG_TRACE(std::format("self_info : \n{}", self_info.DebugString()));
//                         // 基础信息
//                         contact_info->mutable_base_info()->CopyFrom(self_info.base_info());
//                         // 好友信息  // 实际这里查询的是自己的信息，该消息为空
//                         // contact_info->mutable_friend_info()->CopyFrom(self_info.friend_info());
//                     }
//                 }
//             }
//             // 成功才填充
//             auto contact = body->mutable_contacts()->add_contact();
//             contact->CopyFrom(__contact);
//             count++;
//             continue;
//         }
// 
//         // MESSAGES (消息与文件详情增量)
//         else if (pts_type == SyncType::SYNC_MESSAGES) {
//             // 文本消息
//             if (pts_sub_type == "MESSAGE_TEXT") {
//                 auto [success_sync_message_text, sync_message_text] = db_sqlcipher.get_message_record_interface(event_id);
//                 // LOG_DEBUG("sync_message_text : {}", sync_message_text.DebugString());
//                 if (!success_sync_message_text) { continue; }
//                 // 文本消息
//                 if ( sync_message_text.has_text()) {
//                     sync_message_text.set_sub_type(::TaoTieChat::SyncSubTypeMessage::SYNC_MESSAGE_TEXT);
//                     body->mutable_messages()->add_message()->CopyFrom(sync_message_text);
//                     count++;
//                 }
//             }
//             // 文件消息
//             else if (pts_sub_type == "MESSAGE_FILE") {
//                 auto [success_sync_message_file, sync_message_file] = db_sqlcipher.get_message_record_interface(event_id);
//                 // LOG_DEBUG("sync_message_file : {}", sync_message_file.DebugString());
//                 if (!success_sync_message_file) { continue; }
//                 if ( sync_message_file.has_file() && !sync_message_file.file().file_id().empty()) {
//                     const std::string& file_id = sync_message_file.file().file_id();
//                     // 获取文件数据
//                     auto [success_get_file_info, file_metadata, file_status] = db_sqlcipher.get_file_info_interface(file_id);
//                     // LOG_DEBUG("file_metadata : {}", file_metadata.DebugString());
//                     if(!success_get_file_info) { continue; }
//                     const std::string& file_name = file_metadata.file_name();
//                     const std::string& file_md5  = file_metadata.file_md5 ();
//                     int64_t file_size = file_metadata.file_size();
//                     if(file_name.empty() || file_size < 0 || file_md5.empty()) { continue; }
//                     if(file_status != FileStatusCode::FILE_STATUS_UPLOADED) { continue; }
//                     // 添加到返回消息中
//                     sync_message_file.set_sub_type(::TaoTieChat::SyncSubTypeMessage::SYNC_MESSAGE_FILE);
//                     sync_message_file.mutable_file()->set_file_id  (file_id  );
//                     sync_message_file.mutable_file()->set_file_name(file_name);
//                     sync_message_file.mutable_file()->set_file_md5 (file_md5 );
//                     sync_message_file.mutable_file()->set_file_size(file_size);
//                     body->mutable_messages()->add_message()->CopyFrom(sync_message_file);
//                     count++;
//                 }
//             }
//             continue;
//         }
// 
//         // ...
//         else if (pts_type == SyncType::SYNC_GROUPS) {
//             // ...
//         }
//     } while (false);
// }

void sync_service_realtime_sync(const Data::PtsInfo& pts_data) {
    if (pts_data.account_id  .empty() ||
        pts_data.pts_type    .empty() ||
        pts_data.pts_sub_type.empty() ||
        pts_data.pts_value    <= 0    ||
        pts_data.pts_event_id.empty()
    ) {
        return;
    }

    SyncReply body_reply;
    auto* body = body_reply.mutable_body();
    auto* tag  = body->mutable_tag();
    // tag->CopyFrom(request_tag);
    tag->set_type(Utils::Transform::to_sync_type(pts_data.pts_type));
    tag->set_id_start(pts_data.pts_value);
    tag->set_id_end(pts_data.pts_value);
    tag->set_count(0);  // 需要读取实际写入的数量而非查询到的数量
    // int64_t count = 0;  // 记录实际添加的记录数

    LOG_TRACE(std::format("sync pts_type : {}, pts_value : {}", pts_data.pts_type, pts_data.pts_value));

    DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
    // auto get_string = [](const SqlRow&row, const std::string&key, const std::string default_value = std::string{}) -> string {
    //     if (row.contains(key)) { return to_string(row.at(key)); }
    //     return default_value;
    // };
    // auto get_first_string = [](const SqlRows&rows, const std::string&key, const std::string default_value = std::string{}) -> string {
    //     for(auto&row:rows) {
    //         if (row.contains(key)) {
    //             return to_string(row.at(key));
    //         }
    //     }
    //     return default_value;
    // };

    do {
        auto&self_account_id = pts_data.account_id  ;
        auto pts_type        = Utils::Transform::to_sync_type(pts_data.pts_type);
        auto&pts_sub_type    = pts_data.pts_sub_type;
        auto&pts_value       = pts_data.pts_value   ;
        auto&pts_event_id    = pts_data.pts_event_id;
        // 如果用户请求获取账号相关的更新，则从查找到的记录里面提取并填充
        if (pts_type == SyncType::SYNC_CONTACTS) {
            // 添加数据 // 有效才允许添加
            // auto* sync_contact = body->mutable_contacts()->add_contact();
            ::TaoTieChat::SyncContact  __contact;
            ::TaoTieChat::SyncContact* sync_contact = &__contact;
            // 按照子类型填充数据
            if (pts_sub_type == "FRIEND_REQUEST") {
                sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_REUQEST);
                // 他人向自己发来的好友申请
                // 获取请求内容(friend_requests)及好友 ID 和资料()进行填充返回
                auto contact_request = sync_contact->mutable_request();
                // 获取申请记录
                auto[success_request_info, rows_request_info] = db_sqlcipher.get_friend_request_info_by_event_id(pts_event_id);
                if (!success_request_info) { continue; }
                contact_request->set_message_id(pts_event_id);
                // 获取发起者的账号 ID
                auto [rrai, request_account_id] = get_first_string(rows_request_info, "request_account_id");
                if (request_account_id.empty()) { continue; }
                // 校验接收者为自己
                auto [rtai, target_account_id] = get_first_string(rows_request_info, "target_account_id");
                if (target_account_id != self_account_id) { continue; }
                // 填充发送时间
                auto [rrt, request_timestamp] = get_first_string(rows_request_info, "timestamp");
                if (!request_timestamp.empty()) {
                    const auto&tp = Utils::to_time_point(request_timestamp);
                    const auto&ts = Utils::to_proto_timestamp(tp);
                    contact_request->mutable_timestamp()->CopyFrom(ts);
                }
                // 填充附加消息
                auto [rrmt, request_message_text] = get_first_string(rows_request_info, "message");
                if (!request_message_text.empty()) { contact_request->set_message_text(request_message_text); }
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
            // 好友申请通过
            else if (pts_sub_type == "FRIEND_ACCEPT") {
                // 同意他人好友申请时更新好友列表即可，不需要广播允许状态
                // 表 friends 目前没有 event_id 字段
                // 简化处理，只推送新添加了好友即可
                sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_REPLY);
                // 从好友表中找对方，进行状态推送
                // 基础信息
                auto [success_account, target_account_info] = db_sqlcipher.get_active_account_info_interface(pts_event_id);
                if (!success_account) { continue; }
                // 好友信息
                auto [success_ship, friend_info] = db_sqlcipher.get_friend_info_interface(self_account_id, pts_event_id);
                if (!success_ship) { continue; }
                // 复制回传
                auto contact_info = sync_contact->mutable_ship()->mutable_contact_ship();
                LOG_TRACE(std::format("target_account_info : \n{}", target_account_info.DebugString()));
                LOG_TRACE(std::format("friend_info : \n{}", friend_info->DebugString()));
                contact_info->mutable_base_info()->CopyFrom(target_account_info.base_info());  // 基础信息
                contact_info->mutable_friend_info()->CopyFrom(*friend_info.get());  // 好友信息
            }
            // 好友资料变更  // 广播有利于多客户端通知/同步
            else if (pts_sub_type == "CONTACT_INFO_CHANGE") {
                sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_INFO);
                // 从好友表中找对方，进行状态推送
                // 基础信息
                auto [success_account, target_account_info] = db_sqlcipher.get_active_account_info_interface(pts_event_id);
                if (!success_account) { continue; }
                // 好友信息
                auto [success_ship, friend_info] = db_sqlcipher.get_friend_info_interface(self_account_id, pts_event_id);
                // if (!success_ship) { continue; }
                // TODO:自己信息更新时也会触发该操作，当自己不是自己好友时需要从自己资料中查询
                if ( success_ship && friend_info && !friend_info->account_id().empty()) {
                    // 复制回传
                    auto contact_info = sync_contact->mutable_update()->mutable_contact_update();
                    LOG_TRACE(std::format("target_account_info : \n{}", target_account_info.DebugString()));
                    LOG_TRACE(std::format("friend_info : \n{}", friend_info->DebugString()));
                    // 基础信息
                    contact_info->mutable_base_info()->CopyFrom(target_account_info.base_info());
                    // 好友信息
                    contact_info->mutable_friend_info()->CopyFrom(*friend_info.get());
                }
                else {
                    // 不是好友时不需要查询好友信息
                    auto [success_self, self_info] = db_sqlcipher.get_active_account_info_interface(self_account_id);
                    if ( success_self && self_info.has_base_info() && !self_info.base_info().account_id().empty()) {
                        // 复制回传
                        auto contact_info = sync_contact->mutable_update()->mutable_contact_update();
                        LOG_TRACE(std::format("target_account_info : \n{}", target_account_info.DebugString()));
                        LOG_TRACE(std::format("self_info : \n{}", self_info.DebugString()));
                        // 基础信息
                        contact_info->mutable_base_info()->CopyFrom(self_info.base_info());
                        // 好友信息  // 实际这里查询的是自己的信息，该消息为空
                        // contact_info->mutable_friend_info()->CopyFrom(self_info.friend_info());
                    }
                }
            }
            // 成功才填充
            auto contact = body->mutable_contacts()->add_contact();
            contact->CopyFrom(__contact);
        }
        else if(pts_type == SyncType::SYNC_MESSAGES) {
            // // 文本消息
            auto [success_sync_message, sync_message] = db_sqlcipher.get_message_record_interface(pts_event_id);
            if (!success_sync_message) { continue; }
            if (sync_message.has_text()) {
                sync_message.set_sub_type(::TaoTieChat::SyncSubTypeMessage::SYNC_MESSAGE_TEXT);
            }
            else if (sync_message.has_file()) {
                sync_message.set_sub_type(::TaoTieChat::SyncSubTypeMessage::SYNC_MESSAGE_FILE);
                // 补充文件信息
                if (sync_message.has_file() && !sync_message.file().file_id().empty()) {
                    auto [success_file_info, file_info, file_code] = db_sqlcipher.get_file_info_interface(sync_message.file().file_id());
                    if (success_file_info && file_code == TaoTieChat::FileStatusCode::FILE_STATUS_UPLOADED) {
                        if (!file_info.file_name().empty()) {
                            sync_message.mutable_file()->set_file_name(file_info.file_name());
                        }
                        if (!file_info.file_md5().empty()) {
                            sync_message.mutable_file()->set_file_md5(file_info.file_md5());
                        }
                        if (file_info.file_size() >= 0) {
                            sync_message.mutable_file()->set_file_size(file_info.file_size());
                        }
                    }
                }
            }
            body->mutable_messages()->add_message()->CopyFrom(sync_message);
        }
        SessionManager::instance().push_to_user(pts_data.account_id, body_reply);
    } while(false);
}

// 服务端实现
class SyncServiceImpl final : public SyncService::Service {
public:
    // GetLastID 获取当前账号各事件类型的最新 PTS 值，供客户端判断是否有更新需要同步
    Status GetLastID(ServerContext* context, const AuthRequest* request, GetLastIDReply* response) override {
        using Request = AuthRequest;
        using Reply   = GetLastIDReply;
        // =========================== 初始化操作 =========================== //
        log_grpc_context(context);
        if (!context || !request || !response) {
            return Status(StatusCode::INTERNAL, std::format("Invalid request or response pointer"));
        }
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        shared_ptr<Request> params = make_shared<Request>(*request);
        // =========================== 执行操作 =========================== //
        shared_ptr<Reply> result = db_sqlcipher.get_last_id(params);
        if (!result) {
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
        if (!context || !stream) {
            return Status(StatusCode::INTERNAL, std::format("Invalid stream pointer"));
        }
        // =========================== 执行操作 =========================== //
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();

        auto safe_writer = std::make_shared<SyncStreamWriter>(stream);

        std::string current_account_id;
        std::string current_device_id; // 标识当前客户端设备

        // RAII 清理器：无论异常退出、客户端断开还是函数返回，都会自动卸载该设备的 Session
        struct SessionCleaner {
            std::string& account_id;
            std::string& device_id;
            ~SessionCleaner() {
                if (!account_id.empty() && !device_id.empty()) {
                    SessionManager::instance().unregister_session(account_id, device_id);
                }
            }
        } cleaner{current_account_id, current_device_id};

        Request request;
        while(stream->Read(&request)) {
            // 发送鉴权结果 header
            SyncReply header_reply;
            auto* header = header_reply.mutable_header();
            header->mutable_request()->CopyFrom(request);

            // 获取身份校验信息
            if (!request.has_request_user_auth()) {
                set_error(header->mutable_result(), ErrorCode::AUTH_INFO_NONE);
                safe_writer->Write(header_reply);
                continue;
            }

            // 身份校验
            auto auth_request = make_shared<AuthRequest>(request.request_user_auth());
            auto auth_reply = db_sqlcipher.auth_check(auth_request);
            if (!auth_reply || !auth_reply->result().has_success_code()) {
                set_error(header->mutable_result(), ErrorCode::AUTH_FAILED);
                safe_writer->Write(header_reply);
                continue;
            }

            // 获取账号 ID
            auto self_account_id = auth_reply->account_id();
            if (self_account_id.empty()) {
                set_error(header->mutable_result(), ErrorCode::AUTH_FAILED);
                safe_writer->Write(header_reply);
                continue;
            }

            // --- 2. 获取设备 ID 并注册多端 Session ---
            if (current_account_id.empty()) {
                current_account_id = self_account_id;

                current_device_id = context->peer() + "_" + std::to_string(reinterpret_cast<uint64_t>(stream));
                // // 优先获取客户端 Request 中携带的 device_id，若没有则可以自动生成或使用 context Peer 信息
                // if (request.has_device_id() && !request.device_id().empty()) {
                //     current_device_id = request.device_id();
                // } else {
                //     // 回退逻辑：用客户端 peer 地址加随机/指针生成唯一标识
                //     current_device_id = context->peer() + "_" + std::to_string(reinterpret_cast<uint64_t>(stream));
                // }

                // 注册到 SessionManager
                SessionManager::instance().register_session(current_account_id, current_device_id, safe_writer);
            }

            set_success(header->mutable_result(), SuccessCode::SUCCESS);
            if (!safe_writer->Write(header_reply)) {
                break; // 客户端断开连接
            }

            // =========================================================== //

            // 逐个 SyncRequestTag 返回 body
            for(int i = 0; i < request.sync_tag_size(); ++i) {
                // 读取记录
                const auto& request_tag = request.sync_tag(i);
                auto id_start = request_tag.id_start();
                auto id_end   = request_tag.id_end  ();
                id_start = std::max(decltype(id_start)(0), id_start);
                id_end   = std::max(decltype(id_end  )(0), id_end  );
                if (id_start >= id_end) { continue; }

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
                    // auto get_string = [](const SqlRow&row, const std::string&key, const std::string default_value = std::string{}) -> string {
                    //     if (row.contains(key)) { return to_string(row.at(key)); }
                    //     return default_value;
                    // };
                    // auto get_first_string = [](const SqlRows&rows, const std::string&key, const std::string default_value = std::string{}) -> string {
                    //     for(auto&row:rows) {
                    //         if (row.contains(key)) {
                    //             return to_string(row.at(key));
                    //         }
                    //     }
                    //     return default_value;
                    // };

                    auto pts_event_id = row_string(row, "pts_event_id");  // 获取事件 ID  // 必须有效
                    auto pts_type     = request_tag.type();
                    auto pts_sub_type = row_string(row, "pts_sub_type");  // 获取子类型   // 可以不存在
                    if (pts_event_id.empty()) { continue; }

                    // 如果用户请求获取账号相关的更新，则从查找到的记录里面提取并填充
                    if (pts_type == SyncType::SYNC_CONTACTS) {
                        // 添加数据 // 有效才允许添加
                        // auto* sync_contact = body->mutable_contacts()->add_contact();
                        ::TaoTieChat::SyncContact  __contact;
                        ::TaoTieChat::SyncContact* sync_contact = &__contact;
                        // 按照子类型填充数据
                        if (pts_sub_type == "FRIEND_REQUEST") {
                            sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_REUQEST);
                            // 他人向自己发来的好友申请
                            // 获取请求内容(friend_requests)及好友 ID 和资料()进行填充返回
                            auto contact_request = sync_contact->mutable_request();
                            // 获取申请记录
                            auto[success_request_info, rows_request_info] = db_sqlcipher.get_friend_request_info_by_event_id(pts_event_id);
                            if (!success_request_info) { continue; }
                            contact_request->set_message_id(pts_event_id);
                            // 获取发起者的账号 ID
                            auto [rrai, request_account_id] = get_first_string(rows_request_info, "request_account_id");
                            if (request_account_id.empty()) { continue; }
                            // 校验接收者为自己
                            auto [rtai, target_account_id] = get_first_string(rows_request_info, "target_account_id");
                            if (target_account_id != self_account_id) { continue; }
                            // 填充发送时间
                            auto [rrt, request_timestamp] = get_first_string(rows_request_info, "timestamp");
                            if (!request_timestamp.empty()) {
                                const auto&tp = Utils::to_time_point(request_timestamp);
                                const auto&ts = Utils::to_proto_timestamp(tp);
                                contact_request->mutable_timestamp()->CopyFrom(ts);
                            }
                            // 填充附加消息
                            auto [rrmt, request_message_text] = get_first_string(rows_request_info, "message");
                            if (!request_message_text.empty()) { contact_request->set_message_text(request_message_text); }
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
                        else if (pts_sub_type == "FRIEND_ACCEPT") {
                            // 同意他人好友申请时更新好友列表即可，不需要广播允许状态
                            // 表 friends 目前没有 event_id 字段
                            // 简化处理，只推送新添加了好友即可
                            sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_REPLY);
                            // 从好友表中找对方，进行状态推送
                            // 基础信息
                            auto [success_account, target_account_info] = db_sqlcipher.get_active_account_info_interface(pts_event_id);
                            if (!success_account) { continue; }
                            // 好友信息
                            auto [success_ship, friend_info] = db_sqlcipher.get_friend_info_interface(self_account_id, pts_event_id);
                            if (!success_ship) { continue; }
                            // 复制回传
                            auto contact_info = sync_contact->mutable_ship()->mutable_contact_ship();
                            LOG_TRACE(std::format("target_account_info : \n{}", target_account_info.DebugString()));
                            LOG_TRACE(std::format("friend_info : \n{}", friend_info->DebugString()));
                            contact_info->mutable_base_info()->CopyFrom(target_account_info.base_info());  // 基础信息
                            contact_info->mutable_friend_info()->CopyFrom(*friend_info.get());  // 好友信息
                        }
                        // 好友资料变更  // 广播有利于多客户端通知/同步
                        else if (pts_sub_type == "CONTACT_INFO_CHANGE") {
                            sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_INFO);
                            // 从好友表中找对方，进行状态推送
                            // 基础信息
                            auto [success_account, target_account_info] = db_sqlcipher.get_active_account_info_interface(pts_event_id);
                            if (!success_account) { continue; }
                            // 好友信息
                            auto [success_ship, friend_info] = db_sqlcipher.get_friend_info_interface(self_account_id, pts_event_id);
                            // if (!success_ship) { continue; }
                            // TODO:自己信息更新时也会触发该操作，当自己不是自己好友时需要从自己资料中查询
                            if ( success_ship && friend_info && !friend_info->account_id().empty()) {
                                // 复制回传
                                auto contact_info = sync_contact->mutable_update()->mutable_contact_update();
                                LOG_TRACE(std::format("target_account_info : \n{}", target_account_info.DebugString()));
                                LOG_TRACE(std::format("friend_info : \n{}", friend_info->DebugString()));
                                // 基础信息
                                contact_info->mutable_base_info()->CopyFrom(target_account_info.base_info());
                                // 好友信息
                                contact_info->mutable_friend_info()->CopyFrom(*friend_info.get());
                            }
                            else {
                                // 不是好友时不需要查询好友信息
                                auto [success_self, self_info] = db_sqlcipher.get_active_account_info_interface(self_account_id);
                                if ( success_self && self_info.has_base_info() && !self_info.base_info().account_id().empty()) {
                                    // 复制回传
                                    auto contact_info = sync_contact->mutable_update()->mutable_contact_update();
                                    LOG_TRACE(std::format("target_account_info : \n{}", target_account_info.DebugString()));
                                    LOG_TRACE(std::format("self_info : \n{}", self_info.DebugString()));
                                    // 基础信息
                                    contact_info->mutable_base_info()->CopyFrom(self_info.base_info());
                                    // 好友信息  // 实际这里查询的是自己的信息，该消息为空
                                    // contact_info->mutable_friend_info()->CopyFrom(self_info.friend_info());
                                }
                            }
                        }
                        // 成功才填充
                        auto contact = body->mutable_contacts()->add_contact();
                        contact->CopyFrom(__contact);
                        count++;
                        continue;
                    }

                    // MESSAGES (消息与文件详情增量)
                    else if (pts_type == SyncType::SYNC_MESSAGES) {
                        // 文本消息
                        if (pts_sub_type == "MESSAGE_TEXT") {
                            auto [success_sync_message_text, sync_message_text] = db_sqlcipher.get_message_record_interface(pts_event_id);
                            // LOG_DEBUG("sync_message_text : {}", sync_message_text.DebugString());
                            if (!success_sync_message_text) { continue; }
                            // 文本消息
                            if ( sync_message_text.has_text()) {
                                sync_message_text.set_sub_type(::TaoTieChat::SyncSubTypeMessage::SYNC_MESSAGE_TEXT);
                                body->mutable_messages()->add_message()->CopyFrom(sync_message_text);
                                count++;
                            }
                        }
                        // 文件消息
                        else if (pts_sub_type == "MESSAGE_FILE") {
                            auto [success_sync_message_file, sync_message_file] = db_sqlcipher.get_message_record_interface(pts_event_id);
                            // LOG_DEBUG("sync_message_file : {}", sync_message_file.DebugString());
                            if (!success_sync_message_file) { continue; }
                            if ( sync_message_file.has_file() && !sync_message_file.file().file_id().empty()) {
                                const std::string& file_id = sync_message_file.file().file_id();
                                // 获取文件数据
                                auto [success_get_file_info, file_metadata, file_status] = db_sqlcipher.get_file_info_interface(file_id);
                                // LOG_DEBUG("file_metadata : {}", file_metadata.DebugString());
                                if(!success_get_file_info) { continue; }
                                const std::string& file_name = file_metadata.file_name();
                                const std::string& file_md5  = file_metadata.file_md5 ();
                                int64_t file_size = file_metadata.file_size();
                                if(file_name.empty() || file_size < 0 || file_md5.empty()) { continue; }
                                if(file_status != FileStatusCode::FILE_STATUS_UPLOADED) { continue; }
                                // 添加到返回消息中
                                sync_message_file.set_sub_type(::TaoTieChat::SyncSubTypeMessage::SYNC_MESSAGE_FILE);
                                sync_message_file.mutable_file()->set_file_id  (file_id  );
                                sync_message_file.mutable_file()->set_file_name(file_name);
                                sync_message_file.mutable_file()->set_file_md5 (file_md5 );
                                sync_message_file.mutable_file()->set_file_size(file_size);
                                body->mutable_messages()->add_message()->CopyFrom(sync_message_file);
                                count++;
                            }
                        }
                        continue;
                    }

                    // ...
                    else if (pts_type == SyncType::SYNC_GROUPS) {
                        // ...
                    }
                }

                tag->set_count(count);
                // 将记录发送给客户端  // TODO:客户端意外断开会导致服务端崩溃
                // 发送增量数据
                if (!safe_writer->Write(body_reply)) {
                    // 如果写入失败，说明客户端异常断开，退出循环
                    return Status::OK;
                }
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
                    if (id_start >= id_end) { continue; }

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
                        // auto get_string = [](const SqlRow&row, const std::string&key, const std::string default_value = std::string{}) -> string {
                        //     if (row.contains(key)) { return to_string(row.at(key)); }
                        //     return default_value;
                        // };
                        // auto get_first_string = [](const SqlRows&rows, const std::string&key, const std::string default_value = std::string{}) -> string {
                        //     for(auto&row:rows) {
                        //         if (row.contains(key)) {
                        //             return to_string(row.at(key));
                        //         }
                        //     }
                        //     return default_value;
                        // };

                        auto event_id       = row_string(row, "pts_event_id");  // 获取事件 ID  // 必须有效
                        auto event_sub_type = row_string(row, "pts_sub_type");  // 获取子类型   // 可以不存在
                        if (event_id.empty()) { continue; }

                        // 如果用户请求获取账号相关的更新，则从查找到的记录里面提取并填充
                        if (request_tag.type() == SyncType::SYNC_CONTACTS) {
                            // 添加数据 // 有效才允许添加
                            // auto* sync_contact = body->mutable_contacts()->add_contact();
                            ::TaoTieChat::SyncContact  __contact;
                            ::TaoTieChat::SyncContact* sync_contact = &__contact;
                            // 按照子类型填充数据
                            if (event_sub_type == "FRIEND_REQUEST") {
                                sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_REUQEST);
                                // 他人向自己发来的好友申请
                                // 获取请求内容(friend_requests)及好友 ID 和资料()进行填充返回
                                auto contact_request = sync_contact->mutable_request();
                                // 获取申请记录
                                auto[success_request_info, rows_request_info] = db_sqlcipher.get_friend_request_info_by_event_id(event_id);
                                if (!success_request_info) { continue; }
                                // 获取发起者的账号 ID
                                auto [rrai, request_account_id] = get_first_string(rows_request_info, "request_account_id");
                                if (request_account_id.empty()) { continue; }
                                // 校验接收者为自己
                                auto [rtai, target_account_id] = get_first_string(rows_request_info, "target_account_id");
                                if (target_account_id != self_account_id) { continue; }
                                // 填充发送时间
                                auto [rrt, request_timestamp] = get_first_string(rows_request_info, "timestamp");
                                if (!request_timestamp.empty()) {
                                    const auto&tp = Utils::to_time_point(request_timestamp);
                                    const auto&ts = Utils::to_proto_timestamp(tp);
                                    contact_request->mutable_timestamp()->CopyFrom(ts);
                                }
                                // 填充附加消息
                                auto [rrmt, request_message_text] = get_first_string(rows_request_info, "message");
                                if (!request_message_text.empty()) { contact_request->set_message_text(request_message_text); }
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
                            else if (event_sub_type == "FRIEND_ACCEPT") {
                                // 自己加他人为好友时，他人的回复
                                sync_contact->set_sub_type(::TaoTieChat::SyncSubTypeContact::SYNC_CONTACT_REPLY);
                                auto contact_reply = sync_contact->mutable_reply();
                                // 获取申请记录
                                auto[success_request_info, rows_request_info] = db_sqlcipher.get_friend_request_info_by_event_id(event_id);
                                if (!success_request_info) { continue; }
                                // 校验申请发送者为自己
                                auto [rrai, request_account_id] = get_first_string(rows_request_info, "request_account_id");
                                if (request_account_id != self_account_id) { continue; }  // 不是自己的申请
                                // 获取接收者账号
                                auto [rtai, target_account_id] = get_first_string(rows_request_info, "target_account_id");
                                if (request_account_id.empty()) { continue; }
                                // 填充发送时间
                                auto [rrt, request_timestamp] = get_first_string(rows_request_info, "timestamp");
                                if (!request_timestamp.empty()) {
                                    const auto&tp = Utils::to_time_point(request_timestamp);
                                    const auto&ts = Utils::to_proto_timestamp(tp);
                                    contact_reply->mutable_timestamp()->CopyFrom(ts);
                                }
                                // 填充附加消息
                                auto [rrm, request_message] = get_first_string(rows_request_info, "message");
                                if (!request_message.empty()) { contact_reply->set_message(request_message); }
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
                            // ...
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
