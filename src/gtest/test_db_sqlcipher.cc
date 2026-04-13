#include "gtest/gtest.h"

#include "DBSQLCipher.h"

TEST(SQLiteWrapperTest, QueryNamedBindsParametersAndReadsRows) {
    const auto db_path = std::filesystem::temp_directory_path() / "test_db_sqlcipher.db";
    std::error_code ec;
    std::filesystem::remove(db_path, ec);

    DBSQLCipher& db = DBSQLCipher::instance();
    db.set_init_params(db_path, "unit_test_key");
    ASSERT_TRUE(db.database_open());

    auto create_result = db.execute("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, user_id TEXT NOT NULL);");
    ASSERT_TRUE(create_result);
    ASSERT_TRUE(create_result->done) << create_result->msg;

    auto clear_result = db.execute("DELETE FROM users;");
    ASSERT_TRUE(clear_result);
    ASSERT_TRUE(clear_result->done) << clear_result->msg;

    auto insert_params = std::make_shared<SqlParams>();
    (*insert_params)[":id"] = 1;
    (*insert_params)[":user_id"] = std::string("alice");
    auto insert_result = db.execute("INSERT INTO users (id, user_id) VALUES (:id, :user_id);", insert_params);
    ASSERT_TRUE(insert_result);
    ASSERT_TRUE(insert_result->done) << insert_result->msg;

    auto query_params = std::make_shared<SqlParams>();
    (*query_params)[":user_id"] = std::string("alice");
    auto query_result = db.execute("SELECT id, user_id FROM users WHERE user_id = :user_id;", query_params);

    ASSERT_TRUE(query_result);
    ASSERT_TRUE(query_result->done) << query_result->msg;
    ASSERT_EQ(query_result->rows.size(), 1u);

    const auto& row = query_result->rows.front();
    ASSERT_TRUE(row.contains("id"));
    ASSERT_TRUE(row.contains("user_id"));
    EXPECT_EQ(get_int(row.at("id")), 1);
    EXPECT_EQ(get_string(row.at("user_id")), "alice");
}