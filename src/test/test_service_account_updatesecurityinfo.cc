#include "test_headers.h"

class ServiceAccountUpdateSecurityInfo : public ::testing::Test {
protected:
    const std::filesystem::path db_path = std::filesystem::current_path() / (get_current_file_name(__FILE__) + ".db");
    const std::string encryption_key = "your_secure_encryption_key";

    // 每个 TEST_F 运行前都会执行
    void SetUp() override {
        // `ASSERT_FALSE(false);` 会中断测试并标记为失败，而 `EXPECT_FALSE(false);` 会记录失败但继续执行测试。
        std::cout << std::format("database file: {}", db_path.string()) << std::endl;

        // 所有操作都必须成功，否则测试无法继续进行

        // 删除旧的数据库文件，确保每次测试都从干净的状态开始
        if(exists(db_path)) {
            try {
                // remove 返回 bool 值：成功删除返回 true，文件不存在返回 false
                if (remove(db_path)) {
                    std::cout << std::format("file {} removed successfully.", db_path.string()) << std::endl;
                } else {
                    std::cerr << std::format("file {} does not exist, no need to remove.", db_path.string()) << std::endl;
                }
            } catch (const std::filesystem::filesystem_error& e) {
                // 如果文件正在被占用或权限不足，会抛出异常
                std::cerr << std::format("remove {} failed: {}", db_path.string(), e.what()) << std::endl;
            }
            // remove(db_path);
        }
        ASSERT_FALSE(exists(db_path)) << std::format("Failed to remove old database file: {}", db_path.string());

        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        db_sqlcipher.set_init_params(db_path, encryption_key);
        ASSERT_FALSE(!db_sqlcipher.database_open()) << std::format("Failed to open database with encryption key.") << std::endl;
        ASSERT_FALSE(!db_sqlcipher.database_initialize()) << std::format("Failed to initialize database schema.") << std::endl;
    }

    // 每个 TEST_F 运行后都会执行
    void TearDown() override {
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        db_sqlcipher.database_close();
    }
};

// 部分测试需要有已经注册的用户
std::tuple<grpc::Status, SignUpReply> SignUp(
    const string& user_id          ,
    const string& password         ,
    const string& confirm          ,
    const string& security_question,
    const string& security_answer
) {
    // 创建 AccountServiceImpl 实例
    AccountServiceImpl service;

    // 构造测试请求
    SignUpRequest request;
    if(user_id          .empty()) { request.clear_user_id          (); } else { request.set_user_id          (user_id          ); }
    if(password         .empty()) { request.clear_password         (); } else { request.set_password         (password         ); }
    if(confirm          .empty()) { request.clear_confirm          (); } else { request.set_confirm          (confirm          ); }
    if(security_question.empty()) { request.clear_security_question(); } else { request.set_security_question(security_question); }
    if(security_answer  .empty()) { request.clear_security_answer  (); } else { request.set_security_answer  (security_answer  ); }

    // 构造响应对象
    SignUpReply response;

    // 调用 SignUp 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.SignUp(&context, &request, &response);
    return {status, response};
}

// union 与 string 的默认构造冲突
struct UpdateSecurityInfoParams {
    string user_id;
    // union OldAuth {
    struct OldAuth {
        struct OldSecurityInfo {
            string old_security_question = string{};  // 安全问题  // 安全问题也要进行校验
            string old_security_answer   = string{};  // 安全答案
        } osi;
        struct OldPasswordInfo {
            string old_password = string{};  // 旧密码
        } opi;
    } old_auth;
    // union NewAuth {
    struct NewAuth {
        struct NewPasswordInfo {
            string new_password = string{};  // 新密码
            string new_confirm  = string{};  // 确认密码  // 用于客户端
        } npi;
        struct NewSecurityInfo {
            string new_security_question = string{};  // 新安全问题
            string new_security_answer   = string{};  // 新安全答案
        } nsi;
    } new_auth;
};

// 获取指定用户的安全问题
std::tuple<grpc::Status, UpdateSecurityInfoReply> UpdateSecurityInfo(const UpdateSecurityInfoParams& params) {
    // 创建 AccountServiceImpl 实例
    AccountServiceImpl service;

    // 构造测试请求
    UpdateSecurityInfoRequest request;
    if(!params.user_id.empty()) { request.set_user_id(params.user_id); }
    if(!params.old_auth.osi.old_security_question.empty()) { request.mutable_security_info_verify()->set_old_security_question(params.old_auth.osi.old_security_question); }
    if(!params.old_auth.osi.old_security_answer  .empty()) { request.mutable_security_info_verify()->set_old_security_answer  (params.old_auth.osi.old_security_answer  ); }
    if(!params.old_auth.opi.old_password         .empty()) { request.mutable_password_verify     ()->set_old_password         (params.old_auth.opi.old_password         ); }
    if(!params.new_auth.npi.new_password         .empty()) { request.mutable_password_update     ()->set_new_password         (params.new_auth.npi.new_password         ); }
    if(!params.new_auth.npi.new_confirm          .empty()) { request.mutable_password_update     ()->set_new_confirm          (params.new_auth.npi.new_confirm          ); }
    if(!params.new_auth.nsi.new_security_question.empty()) { request.mutable_security_info_update()->set_new_security_question(params.new_auth.nsi.new_security_question); }
    if(!params.new_auth.nsi.new_security_answer  .empty()) { request.mutable_security_info_update()->set_new_security_answer  (params.new_auth.nsi.new_security_answer  ); }

    // 构造响应对象
    UpdateSecurityInfoReply response;

    // 调用 UpdateSecurityInfo 方法
    grpc::ServerContext context;  // 模拟 gRPC 上下文
    grpc::Status status = service.UpdateSecurityInfo(&context, &request, &response);
    return {status, response};
}

// 测试 AccountService 的 UpdateSecurityInfo 方法

void demo_params() {
    UpdateSecurityInfoParams demo_params_full = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            },
            .opi = {
                .old_password = "value_password"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            },
            .nsi = {
                .new_security_question = "value_new_security_question",
                .new_security_answer   = "value_new_security_answer"
            }
        }
    };

    UpdateSecurityInfoParams demo_params_pup = {
        .user_id = "value_user_id",
        .old_auth = {
            .opi = {
                .old_password = "value_password"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            }
        }
    };

    UpdateSecurityInfoParams demo_params_sup = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            },
        }
    };

    UpdateSecurityInfoParams demo_params_sus = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            },
        },
        .new_auth = {
            .nsi = {
                .new_security_question = "value_new_security_question",
                .new_security_answer   = "value_new_security_answer"
            }
        }
    };
}

// ========================== 非数据库的空参数校验 ========================== //

// ========================== pup(Password Update Password)

// 未提供用户 ID，返回错误码 USER_ID_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, pup_user_id_empty) {
    // 使用密码更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "",
        .old_auth = {
            .opi = {
                .old_password = "value_password"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_EMPTY);
}

// 没有提供旧密码，返回错误码 UNSUPPORTED // 之所以不是 `PASSWORD_EMPTY` 是因为既没有密码校验也没有安全问题的校验
TEST_F(ServiceAccountUpdateSecurityInfo, pup_old_password_empty) {
    // 使用密码更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .opi = {
                .old_password = ""
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::UNSUPPORTED);
}

// 没有提供新密码，返回错误码 TARGET_PASSWORD_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, pup_new_password_empty) {
    // 使用密码更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .opi = {
                .old_password = "value_password"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "",
                .new_confirm  = "value_new_password"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_PASSWORD_EMPTY);
}

// 没有提供确认密码，返回错误码 TARGET_CONFIRM_PASSWORD_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, pup_new_confirm_empty) {
    // 使用密码更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .opi = {
                .old_password = "value_password"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = ""
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_CONFIRM_PASSWORD_EMPTY);
}

// 新密码与确认密码不匹配，返回错误码 DIFF_PASSWORD_WITH_CONFIRM
TEST_F(ServiceAccountUpdateSecurityInfo, pup_new_password_mismatch) {
    // 使用密码更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .opi = {
                .old_password = "value_password"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_mismatched_password"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::DIFF_PASSWORD_WITH_CONFIRM);
}

// ========================== sup(Security Update Password)

// 未提供用户 ID，返回错误码 USER_ID_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, sup_user_id_empty) {
    // 使用安全问题更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            },
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_EMPTY);
}

// 没有提供旧的安全信息，返回错误码 SECURITY_QUESTION_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, sup_old_question_empty) {
    // 使用安全问题更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            },
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::SECURITY_QUESTION_EMPTY);
}

// 没有提供旧的安全信息答案，返回错误码 SECURITY_ANSWER_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, sup_old_answer_empty) {
    // 使用安全问题更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = ""
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            },
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::SECURITY_ANSWER_EMPTY);
}

// 没有提供新密码，返回错误码 TARGET_PASSWORD_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, sup_new_password_empty) {
    // 使用安全问题更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "",
                .new_confirm  = "value_new_password"
            },
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_PASSWORD_EMPTY);
}

// 没有提供确认密码，返回错误码 TARGET_CONFIRM_PASSWORD_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, sup_new_confirm_empty) {
    // 使用安全问题更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = ""
            },
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_CONFIRM_PASSWORD_EMPTY);
}

// 新密码与确认密码不匹配，返回错误码 DIFF_PASSWORD_WITH_CONFIRM
TEST_F(ServiceAccountUpdateSecurityInfo, sup_new_password_mismatch) {
    // 使用安全问题更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_mismatched_password"
            },
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::DIFF_PASSWORD_WITH_CONFIRM);
}

// ========================== sus(Security Update Security Question and Answer)

// 未提供用户 ID，返回错误码 USER_ID_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, sus_user_id_empty) {
    // 使用安全问题更新安全信息的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            },
        },
        .new_auth = {
            .nsi = {
                .new_security_question = "value_new_security_question",
                .new_security_answer   = "value_new_security_answer"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_EMPTY);
}

// 没有提供旧的安全信息，返回错误码 SECURITY_QUESTION_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, sus_old_security_question_empty) {
    // 使用安全问题更新安全信息的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "",
                .old_security_answer   = "value_security_answer"
            },
        },
        .new_auth = {
            .nsi = {
                .new_security_question = "value_new_security_question",
                .new_security_answer   = "value_new_security_answer"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::SECURITY_QUESTION_EMPTY);
}

// 没有提供旧的安全信息答案，返回错误码 SECURITY_ANSWER_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, sus_old_security_answer_empty) {
    // 使用安全问题更新安全信息的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = ""
            },
        },
        .new_auth = {
            .nsi = {
                .new_security_question = "value_new_security_question",
                .new_security_answer   = "value_new_security_answer"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::SECURITY_ANSWER_EMPTY);
}

// 没有提供新的安全问题，返回错误码 TARGET_SECURITY_QUESTION_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, sus_new_security_question_empty) {
    // 使用安全问题更新安全信息的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            },
        },
        .new_auth = {
            .nsi = {
                .new_security_question = "",
                .new_security_answer   = "value_new_security_answer"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_SECURITY_QUESTION_EMPTY);
}

// 没有提供新的安全问题的答案，返回错误码 TARGET_SECURITY_ANSWER_EMPTY
TEST_F(ServiceAccountUpdateSecurityInfo, sus_new_security_answer_empty) {
    // 使用安全问题更新安全信息的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            },
        },
        .new_auth = {
            .nsi = {
                .new_security_question = "value_new_security_question",
                .new_security_answer   = ""
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::TARGET_SECURITY_ANSWER_EMPTY);
}

// ========================== 读写数据库的参数校验 ========================== //

// ========================== pup(Password Update Password)

// 用户 ID 不存在，返回错误码 USER_ID_NOT_FOUND
TEST_F(ServiceAccountUpdateSecurityInfo, pup_user_id_not_found) {
    // 使用密码更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .opi = {
                .old_password = "value_password"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_NOT_FOUND);
}

// 密码错误，返回错误码 PASSWORD_ERROR
TEST_F(ServiceAccountUpdateSecurityInfo, pup_password_error) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 使用密码更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .opi = {
                .old_password = "wrong_old_password"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::PASSWORD_ERROR);
}

// 设置成功
TEST_F(ServiceAccountUpdateSecurityInfo, pup_success) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 使用密码更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .opi = {
                .old_password = "value_password"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);
}

// ========================== sup(Security Update Password)

// 用户 ID 不存在，返回错误码 USER_ID_NOT_FOUND
TEST_F(ServiceAccountUpdateSecurityInfo, sup_user_id_not_found) {
    // 使用安全问题更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            },
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_NOT_FOUND);
}

// 安全问题不一致，返回错误码 SECURITY_QUESTION_ERROR
TEST_F(ServiceAccountUpdateSecurityInfo, sup_security_question_mismatch) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 使用安全问题更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_mismatched_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            },
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::SECURITY_QUESTION_ERROR);
}

// 安全答案错误，返回错误码 SECURITY_ANSWER_ERROR
TEST_F(ServiceAccountUpdateSecurityInfo, sup_security_answer_mismatch) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 使用安全问题更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_mismatched_security_answer"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            },
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::SECURITY_ANSWER_ERROR);
}

// 设置成功
TEST_F(ServiceAccountUpdateSecurityInfo, sup_success) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 使用安全问题更新密码的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .npi = {
                .new_password = "value_new_password",
                .new_confirm  = "value_new_password"
            },
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);
}

// ========================== sus(Security Update Security Question and Answer)

// 用户 ID 不存在，返回错误码 USER_ID_NOT_FOUND
TEST_F(ServiceAccountUpdateSecurityInfo, sus_user_id_not_found) {
    // 使用安全问题更新安全信息的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .nsi = {
                .new_security_question = "value_new_security_question",
                .new_security_answer   = "value_new_security_answer"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::USER_ID_NOT_FOUND);
}

// 安全问题不一致，返回错误码 SECURITY_QUESTION_ERROR
TEST_F(ServiceAccountUpdateSecurityInfo, sus_security_question_mismatch) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 使用安全问题更新安全信息的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_mismatched_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .nsi = {
                .new_security_question = "value_new_security_question",
                .new_security_answer   = "value_new_security_answer"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::SECURITY_QUESTION_ERROR);
}

// 安全答案错误，返回错误码 SECURITY_ANSWER_ERROR
TEST_F(ServiceAccountUpdateSecurityInfo, sus_security_answer_mismatch) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 使用安全问题更新安全信息的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_mismatched_security_answer"
            }
        },
        .new_auth = {
            .nsi = {
                .new_security_question = "value_new_security_question",
                .new_security_answer   = "value_new_security_answer"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().error_code(), ErrorCode::SECURITY_ANSWER_ERROR);
}

// 设置成功
TEST_F(ServiceAccountUpdateSecurityInfo, sus_success) {
   // 注册一个用户，确保后续测试有一个有效的目标用户
    auto [status_signup, response_signup] = SignUp("value_user_id", "value_password", "value_password", "value_security_question", "value_security_answer");
    EXPECT_TRUE(status_signup.ok());
    EXPECT_EQ(response_signup.result().success_code(), SuccessCode::SUCCESS);
    EXPECT_TRUE(!response_signup.account_id().empty());  // 注册成功应该返回一个非空的 account_id
    std::cout << std::format("Registered request account_id: {}", response_signup.account_id()) << std::endl;
    auto request_account_id = response_signup.account_id();

    // 使用安全问题更新安全信息的参数示例
    UpdateSecurityInfoParams params = {
        .user_id = "value_user_id",
        .old_auth = {
            .osi = {
                .old_security_question = "value_security_question",
                .old_security_answer   = "value_security_answer"
            }
        },
        .new_auth = {
            .nsi = {
                .new_security_question = "value_new_security_question",
                .new_security_answer   = "value_new_security_answer"
            }
        }
    };
    auto [status, response] = UpdateSecurityInfo(params);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.result().success_code(), SuccessCode::SUCCESS);
}
