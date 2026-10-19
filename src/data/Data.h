#pragma once

#include "headers.h"

namespace Data {
    // 表格中的列为基础，亦可扩展

    // table meta
    struct MetaInfo {
        int        id     = -1;  // 数据库中的索引
        string     name       ;
        string     value      ;
        string     common     ;
    };

    // table users
    struct UserInfo {
        int32  id                   = -1;  // 数据库中的索引
        string account_id               ;  // 系统生成，永久唯一
        string user_id                  ;  // 用户可改，在有效用户中逻辑唯一
        int32  status = 0               ;  //
        string password_hash            ;  // 密码哈希
        string nickname                 ;  // 昵称
        string avatar_id                ;  // 头像 ID
        string signature                ;  // 个性签名
        string description              ;  // 个人简介
        int32  gender                   ;  // 0:未设置, 1:男, 2:女, 3:保密
        string region                   ;  // 建议存区域代码 (Code)
        string birthday                 ;  // 格式: YYYY-MM-DD
        string phones                   ;  // 建议存 JSON 数组或独立表  -- 暂定为以 `;` 划分的字符串数组
        string emails                   ;  // 建议存 JSON 数组或独立表  -- 暂定为以 `;` 划分的字符串数组
        string social_links             ;  // 建议存 JSON 数组或独立表  -- 暂定为以 `;` 划分的字符串数组
        string register_time            ;  // 注册时间
        string update_time              ;  // 更新时间
        string deactivate_time          ;  // 账号停用/冻结/注销时间
        string security_question        ;  // 安全问题
        string security_answer_hash     ;  // 安全答案
        string common                   ;  // 用于存储扩展的 JSON 配置
    };

    // user_tokens
    struct TokenInfo {
        int32  id           = -1;
        string account_id       ;
        string token_hash       ;
        string token_type       ;
        int32  token_status =  0;
        string device_info      ;
        string last_ip          ;
        string create_time      ;
        string expire_time      ;
        string common           ;
    };

    // table friends
    struct FriendInfo {
        int32      id                =    -1;  // 数据库中的索引
        string     account_id               ;  // 账号 ID  // 获取好友信息时由服务器填充该字段
        string     friend_account_id        ;  // 账号 ID  // 获取好友信息时由服务器填充该字段
        int32      status            =    -1;  // 是否被拉黑
        bool       starred           = false;  // 是否标星
        bool       silent            = false;  // 是否免打扰
        string     notes                    ;  // 备注
        stringlist tags                     ;  // 标签，逗号分隔
        string     added_source             ;  // 添加来源
        string     added_time               ;  // 添加时间
        string     removed_time             ;  // 添加时间
        string     common                   ;  // 添加时间
    };

    // messages
    struct MessageInfo {
        int32  id                 = -1;
        string chatroom_id            ;
        string server_message_id      ;
        string client_message_id      ;
        string sender_account_id      ;
        string receiver_account_id    ;
        string message_type           ;
        string short_text             ;
        string big_context_id         ;
        string file_id                ;
        string timestamp              ; // timestamp 消息时间戳。需要返回该值，以便客户端进行消息排序和去重。
        string common                 ;
    };

    // friend_requests
    struct FriendRequestInfo {
        int32  id                 = -1;
        string event_id               ;
        string request_account_id     ;
        string target_account_id      ;
        int32  status             =  0;
        string timestamp              ;
        string message                ;
        string reply                  ;
        string common                 ;
    };

    // file_metadatas
    struct FileMetadataInfo {
        int32  id          = -1;
        string file_id         ;
        string file_name       ;
        int64  file_size   = -1;
        string file_md5        ;
        string file_status =  0;
        string upload_time     ;
        string common          ;
    };

    // file_permissions
    struct FilePermissionInfo {
        int32  id              = -1;
        string file_id             ;
        string account_id          ;
        int32  permission_code =  0;
        string update_time         ;
        string common              ;
    };

    // pts
    struct PtsInfo {
        int32   id           = -1;  // 记录 ID
        string  account_id       ;  // 账号 ID
        string  pts_type         ;  // 事件主类型 -- 用于客户端更新对比
        string  pts_sub_type     ;  // 事件子类型 -- 用于指明查找位置
        int64_t pts_value    =  0;  // 事件序号   -- 自增 ID
        string  pts_event_id     ;  // 事件 ID    -- 事件对应的全局唯一 ID，如消息 ID 等
        string  update_time      ;  // 更新时间
        string  common           ;  // common 通用字段
    };

    // pts_stats
    struct PtsStatInfo {
        int32  id          = -1;  // 记录 ID
        string account_id      ;  // 账号 ID
        string pts_type        ;  // 事件类型
        int32  pts_value   =  0;  // 事件序号  -- 对用户的自增 ID
        string update_time     ;  // 更新时间
        string common          ;  // common 通用字段
    };
};
