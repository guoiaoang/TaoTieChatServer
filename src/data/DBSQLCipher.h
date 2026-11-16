#pragma once

#include "headers.h"
#include "taotie_chat.pb.h"
#include "utils.h"
#include "DBSQLCipherCmds.h"
#include "Data.h"

using namespace SQL::COMMANDS::PRAGMA;
using namespace SQL::COMMANDS::CREATE;
using namespace SQL::COMMANDS::QUERY ;
using namespace SQL::COMMANDS::INSERT;
using namespace SQL::COMMANDS::UPDATE;
using namespace Utils::Transform;

using Utils::set_success         ;
using Utils::set_error           ;
using Utils::hash_token          ;
using Utils::hash_password       ;
using Utils::hash_security_answer;
using Utils::generate_uuid       ;
using Utils::generate_account_id ;
using Utils::generate_token      ;
using Utils::generate_message_id ;
using Utils::generate_file_id    ;
using Utils::current_time_point  ;
using Utils::to_local_string     ;
using Utils::split               ;
using Utils::join                ;
using Utils::success_reply       ;
using Utils::error_reply         ;
using Utils::Transform::to_string;

// =========================================================================== //

// SyncService 服务中向客户端发送实时更新消息
// struct PtsData{
//     const string  account_id;
//     const string  pts_type;
//     const string  pts_sub_type;
//     const int64_t pts_value;
//     const string  pts_event_id;
// };
void sync_service_realtime_sync(const Data::PtsInfo& pts_data);

// =========================================================================== //

// 定义一个简单的多类型值容器（C++17）
// 0,1,2,3,4 分别表示 bool/int/int64_t/double/string/nullptr_t
using SqlValue   = variant<bool, int, int64_t, double, string, nullptr_t>;
using SqlParams  = map<string, SqlValue>;  // 参数其实不必保留顺序
using SqlRow     = map<string, SqlValue>;  // 不保留顺序
using SqlRows    = vector<SqlRow>;  // 保留顺序
using SqlCommand = pair<string_view, shared_ptr<SqlParams>>;  // string_view 无法延长生命周期

struct SQLResult{
    bool    done = false;  // 仅表示是否执行完成，不代表是否成功
    string  msg ;  // 错误消息
    SqlRows rows;  // 列名 - 列值
    SQLResult(bool done = false, const string& msg = "", const SqlRows& rows = {})
        : done(done), msg(msg), rows(rows) {}
};

string to_string(const SqlValue& v);

bool    get_bool  (const SqlValue& v, const bool     default_value =    false);
// int  get_int   (const SqlValue& v, const int      default_value =       -1);
int64_t get_int64 (const SqlValue& v, const int64_t  default_value =       -1);
double  get_double(const SqlValue& v, const double   default_value =     -1.0);
string  get_string(const SqlValue& v, const string & default_value = string{});

// const SqlRow     & row,  // 要查找的行
// const string& key,  // 要查找的关键词
// const string& default_value = string{}, // 失败时的默认值
// const function<bool(const string&)>& func = nullptr  // 条件符合时返回默认值
string     row_string    (const SqlRow& row, const string& key, const string    & default_value =     string{}, const function<bool(const string &)>& func = nullptr);
stringlist row_stringlist(const SqlRow& row, const string& key, const stringlist& default_value = stringlist{}, const function<bool(const string &)>& func = nullptr);
// int     row_int       (const SqlRow& row, const string& key, const int       & default_value =           -1, const function<bool(const int    &)>& func = nullptr);
int64_t    row_int64     (const SqlRow& row, const string& key, const int64_t   & default_value =           -1, const function<bool(const int64_t&)>& func = nullptr);
bool       row_bool      (const SqlRow& row, const string& key, const bool      & default_value =        false, const function<bool(const bool   &)>& func = nullptr);

tuple<bool, bool> check_not_empty(const SqlRows& rows);
// tuple<bool, string> get_string   (const SqlRow & row , const string& key, const string default_value = string{});
tuple<bool, string > get_first_string(const SqlRows& rows, const string& key, const string  default_value = string{});
tuple<bool, int64_t> get_first_int64 (const SqlRows& rows, const string& key,       int64_t default_value =       -1);

class DBSQLCipher {
    using list_friends = list<shared_ptr<Data::FriendInfo>>;
private:
    DBSQLCipher() = default;
    DBSQLCipher(const DBSQLCipher&) = default;
    DBSQLCipher& operator=(const DBSQLCipher&) = default;

public:
    static DBSQLCipher& instance();
    ~DBSQLCipher();

public:
    void set_init_params(const path& db_path, const string& encryption_key);
    // 打开数据库并进行初始化验证
    // 初始化仅写入 version 信息
    bool database_open       ();
    bool database_initialize ();
    bool database_close      ();
    void print_sqlcipher_info();

public:
    // 执行查询并将结果存入 vector<map>
    // transaction=true 时：
    // 1) 若当前不在事务中，则自动 BEGIN IMMEDIATE
    // 2) 语句成功后自动 COMMIT
    // 3) 任意失败自动 ROLLBACK
    shared_ptr<SQLResult> execute(const string_view& sql, shared_ptr<SqlParams> params = nullptr, bool transaction = false);
    // 批量执行 SQL；transaction=true 时整批语句在同一事务中原子执行
    shared_ptr<SQLResult> execute(const vector<SqlCommand>& commands, bool transaction);
    int         get_last_error_code    ();
    int         get_extended_error_code();
    const char* get_error_message      ();

public:
    // ================== 业务层不要包含 SQL 语句与参数构造 ================== //

    // 获取在线状态
    shared_ptr<StatusReply             > get_status           (shared_ptr<StatusRequest            > request);
    // 用户注册  // 注册成功后向同步队列添加一条消息，以便同步消息时自动获取账号信息
    // TODO: account_id 建议全局全时段唯一，即无论该账号是否注销、是否为用户、群组、服务号等的 ID 都不应重复。
    // TODO: user_id 在在用状态下，即是否为用户、群组、服务号等的 ID 时，也应该唯一。
    shared_ptr<SignUpReply             > sign_up              (shared_ptr<SignUpRequest            > request);
    // 内部使用
    // 密码或 token 验证  // 验证身份并返回 account_id  // 传入 user_id 时进行转换
    shared_ptr<AuthReply               > auth_check           (shared_ptr<AuthRequest              > request);
    // 用户登录  // TODO:使用 token 登陆时延长 token 有效期，而不是生成新的
    shared_ptr<SignInReply             > sign_in              (shared_ptr<SignInRequest            > request);
    // 搜索用户 ID，该接口可获取公开的信息及好友信息。
    // 陌生人查询或查询好友以是否有鉴权信息作为区分依据。
    // 1. 使用目标用户 ID 获取公开信息，无鉴权信息时为陌生人查询，此时可返回查询成功；
    // 2. 有鉴权信息时执行查询好友的操作，先验证鉴权再进行查询。
    shared_ptr<SearchUserReply         > search_user          (shared_ptr<SearchUserRequest        > request);
    // 添加为联系人的申请
    shared_ptr<ContactRequestReply     > contact_request      (shared_ptr<ContactRequestRequest    > request);
    // 添加为联系人的回复
    shared_ptr<ContactReplyReply       > contact_reply        (shared_ptr<ContactReplyRequest      > request);
    // 客户端发送消息，服务端写入消息到数据库
    shared_ptr<MessageReply            > write_message        (shared_ptr<MessageContext           > request);
    // 更新用户信息
    // 1. 校验身份
    // 2. 有好友信息时则认为更新的是好友的信息，否则才认为更新的是自己的信息
    shared_ptr<UpdateProfileReply      > update_profile       (shared_ptr<UpdateProfileRequest     > request);
    // 获取安全问题
    shared_ptr<GetSecurityQuestionReply> get_security_question(shared_ptr<AuthRequest              > request);
    // 更新用户安全信息
    shared_ptr<UpdateSecurityInfoReply > update_security_info (shared_ptr<UpdateSecurityInfoRequest> request);
    // 当前接口目前仅是获取一个上传 ID，不校验文件是否已经存在及是否正确
    // 秒传功能在上传接口中
    // 当前接口会查询 metadata 获取到文件的 ID，如果不存在就返回一个新的文件 ID
    //
    // 用户使用文件上传接口进行上传完成时会更新文件的 metadata 和权限信息，
    // 如果没有数据库中 metadata 信息则认为需要进行上传，
    // 有 metadata 信息则认为已经上传完成。
    shared_ptr<GetFileIDReply          > get_upload_file_id   (shared_ptr<GetFileIDRequest         > request);
    // 执行上传前检查文件是否存在
    // 1. 只处理数据库相关的操作，文件读写不在这里；
    // 2. 需要检查文件元数据防止上传文件不一致，但不检查上传用户，谁都可以上传文件内容。
    //    1. 文件 ID 用来判断用户是否有权进行上传;
    //    2. 文件状态判断用户是否需要上传；
    //    3. 文件大小用来防止用户上传文件异常，便于截断，防止撑爆服务器；
    //    4. 文件 md5 用来判断上传的文件是否正确。
    tuple<bool, bool                          > update_file_status_interface     (const string &file_id           , FileStatusCode file_status     );
    tuple<bool, int64_t                       > get_file_permission_interface    (const string &account_id        , const string &file_id          );
    tuple<bool, SyncMessage                   > get_message_record_interface     (const string &server_message_id                                  );
    tuple<bool, ContactInfo                   > get_active_account_info_interface(const string &account_id                                         );
    tuple<bool, FileMetadata , FileStatusCode > get_file_info_interface          (const string &file_id                                            );
    tuple<bool, shared_ptr<ContactInfoFriend >> get_friend_info_interface        (const string &self_account_id   , const string &friend_account_id);
    // tuple<bool, bool                       > record_file_path_interface       (const string &file_md5          , const string &file_path        );
    // tuple<bool, string                     > get_file_path_interface          (const string &file_md5                                           );

    // 获取指定账号各事件类型的最新 PTS 值（用于客户端同步）
    shared_ptr<GetLastIDReply> get_last_id(shared_ptr<AuthRequest> request);
    // 查询指定账号与类型的增量事件明细
    tuple<bool, SqlRows> get_pts_records(const string& account_id, const string& pts_type, int64_t id_start, int64_t id_end);
    tuple<bool, SqlRows> get_pts_record (const string& pts_event_id);
    tuple<bool, SqlRows> get_friend_request_info(const string& request_account_id, const string& target_account_id);
    tuple<bool, SqlRows> get_friend_request_info_by_event_id(const string& event_id);

private:
    // 从传入参数中提取用户 ID
    // 注意：未执行鉴权操作，且直接传入用户 ID 时未对该用户 ID 进行状态判断
    string get_active_user_id(shared_ptr<AuthRequest> auth_request);

private:
    // ========================= 包含 SQL 语句的函数 ========================= //
    // 执行失败、执行成功但无匹配均返回{false, {}}，执行成功且匹配才返回 {true,xx}

    // ============================ 解析查询结果 ============================ //

    tuple<bool, string                       > get_version_value (const SqlRows& rows);
    tuple<bool, ContactInfo                  > get_contact_info  (const SqlRows& rows);
    // 获取好友关联信息  // 一次查询实际只有一条  // 查询结果为空时要进行标志
    tuple<bool, shared_ptr<ContactInfoFriend>> get_friend_info   (const SqlRows& rows);
    tuple<bool, shared_ptr<list_friends     >> get_friends_list  (const SqlRows& rows);
    // 获取文件 ID 对应的信息
    tuple<bool, FileMetadata, FileStatusCode > get_file_info     (const SqlRows& rows);
    tuple<bool, SyncMessage                  > get_message_record(const SqlRows& rows);
    tuple<bool, shared_ptr<Data::PtsInfo    >> get_pts_record    (const SqlRows& rows);
    // ============================== 读数据库 ============================== //

    // 获取数据版本
    tuple<bool, string> get_meta_version();
    // 用户注册时搜索 user_id 是否可用  // 仅查找在用的用户 ID 即可
    tuple<bool, bool  > search_active_user_id(const string& user_id);
    // 通过用户 ID 查询账号 ID  // 账号停用时返回空，表示失败
    tuple<bool, string> get_active_account_id(const string& user_id);
    // 通过账号 ID 查询用户 ID  // 账号停用时返回空，表示失败
    tuple<bool, string> get_active_user_id   (const string& account_id);
    // 通过账号 ID 查询安全问题  // 安全问题不允许为空，为空则代表查询失败  // 账号停用时也返回空
    tuple<bool, string> get_active_account_security_question(const string& account_id);
    tuple<bool, ContactInfo> get_active_account_info        (const string& account_id);
    tuple<bool, shared_ptr<ContactInfoFriend>> get_friend_info (const string& self_account_id, const string& friend_account_id);
    tuple<bool, shared_ptr<list_friends     >> get_friends_list(const string& self_account_id);
    tuple<bool, bool> check_exist_account_id (const string& account_id);
    tuple<bool, bool> check_active_account_id(const string& account_id);
    tuple<bool, bool> check_password         (const string& account_id, const string& password);
    tuple<bool, bool> check_token            (const string& account_id, const string& token   );
    // 检查服务端生成的消息 ID 是否重复，重复则代表消息已经存入
    tuple<bool, bool> check_exist_server_message_id(const string& message_id);
    tuple<bool, SyncMessage> get_message_record(const string& server_message_id);
    // 客户端生成的消息 ID // 同一账号生成的 ID 不重复即可认为是新消息，不需要全局唯一性校验
    // ID 存在代表消息重复发送
    tuple<bool, bool> check_exist_client_message_id(const string& sender_account_id, const string& message_id);
    // 检查文件 ID 是否已经存在
    tuple<bool, bool> check_exist_file_id(const string& file_id);
    // 获取好友状态。0：非好友，1：好友，2：被对方拉黑了（可能没有这个状态，先预留）
    tuple<bool, int64_t > get_contact_status(const string& request_account_id, const string& target_account_id);
    tuple<bool, int64_t > get_friend_request_status(const string& request_account_id, const string& target_account_id);
    tuple<bool, bool> check_active_account_security(const string& account_id, const string& question, const string& answer);
    // 获取已经上传文件的文件 ID
    tuple<bool, string> get_exist_file_id(const string& file_name, int64_t file_size, const string& file_md5);
    tuple<bool, FileMetadata, FileStatusCode> get_file_info(const string& file_id);
    tuple<bool, int64_t> get_file_permission(const string &account_id, const string &file_id);
    // tuple<bool, string> get_file_path(const string& file_md5);

    // ============================== 写数据库 ============================== //

    tuple<bool, bool> write_meta_table  ();
    tuple<bool, bool> write_meta_version();
    tuple<bool, bool> write_token    (const string &account_id, const string &token);
    tuple<bool, string> user_register(const string &user_id   , const string &password          , const string &security_question, const string &security_answer);
    tuple<bool, bool> contact_request(const string &event_id  , const string &request_account_id, const string &target_account_id, const string &message = string{});
    tuple<bool, bool> contact_add    (const string &request_account_id, const string &target_account_id);
    // 记录文本消息到数据库
    tuple<bool, bool> write_short_message(const string &chatroom_id ,
        const string &server_message_id, const string &client_message_id  ,
        const string &sender_account_id, const string &receiver_account_id,
        const string &short_text       , const string &timestamp
    );
    // 记录文件消息到数据库
    tuple<bool, bool> write_file_message(const string &chatroom_id ,
        const string &server_message_id, const string &client_message_id  ,
        const string &sender_account_id, const string &receiver_account_id,
        const string &file_id          , const string &timestamp
    );
    // 设置好友请求的状态为同意/拒绝/忽略
    tuple<bool, bool> set_contact_request_status(const string &request_account_id, const string &target_account_id, const ContactReplyCode &code);
    // 更新用户个人的非安全信息
    tuple<bool, bool> update_self_profile                (const string &account_id, const ContactInfoBase &self_base_info);
    // 更新好友信息
    tuple<bool, bool> update_friend_profile              (const string &account_id, const ContactInfoFriend &friend_info);
    // 通过验证账号的旧密码来更新新的密码
    tuple<bool, bool> update_account_password_by_password(const string &account_id, const string &old_password, const string &new_password);
    // 通过验证账号的安全问题来更新新的密码
    tuple<bool, bool> update_account_password_by_security(const string &account_id, const string &question, const string &answer, const string &password);
    // 通过验证账号的安全问题来更新新的安全问题和答案
    tuple<bool, bool> update_account_security_by_security(const string &account_id,
        const string& old_question, const string& old_answer,
        const string& new_question, const string& new_answer
    );
    tuple<bool, bool> write_file_metadata  (const string& file_id   , const string& file_name, const int64_t file_size, const string& file_md5, FileStatusCode status);
    tuple<bool, bool> write_file_permission(const string& account_id, const string& file_id  );
    // tuple<bool, bool> write_file_path   (const string& file_md5  , const string& file_path);
    // 写入或递增 pts_stats 中指定账号、类型的计数器
    tuple<bool, bool> write_pts_stats(const string& account_id, const string& pts_type);
    // 写入单条增量事件：先递增计数，再写入明细记录。
    tuple<bool, bool> write_pts_event(const string& account_id, const string& pts_type, const string& pts_sub_type, const string& pts_event_id);
    tuple<bool, bool> update_file_status(const string& file_id, const FileStatusCode& status);

private:
    path     m_db_path;
    sqlite3* m_db_sqlcipher = nullptr;
    string   m_encryption_key;
    int      SQL_RESULTS_MAX_LINES = 1000;  // 每次查询最多获取多少条数据
};
