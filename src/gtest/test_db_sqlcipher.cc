#include "gtest/gtest.h"

#include "DBSQLCipher.h"

TEST(SQLiteWrapperTest, QueryNamedBindsParametersAndReadsRows) {
    sqlite3* db = nullptr;
    ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    SQLiteWrapper wrapper;
    wrapper.set_db(db);

    auto create_result = wrapper.query_named(
        "CREATE TABLE users (id INTEGER PRIMARY KEY, user_id TEXT NOT NULL);"
    );
    ASSERT_TRUE(create_result->done);
    ASSERT_TRUE(create_result->error_message.empty());

    auto insert_params = std::make_shared<WrapperNamedParam>();
    (*insert_params)[":id"] = 1;
    (*insert_params)[":user_id"] = std::string("alice");

    auto insert_result = wrapper.query_named(
        "INSERT INTO users (id, user_id) VALUES (:id, :user_id);",
        insert_params
    );
    ASSERT_TRUE(insert_result->done);
    ASSERT_TRUE(insert_result->error_message.empty());

    auto query_params = std::make_shared<WrapperNamedParam>();
    (*query_params)[":user_id"] = std::string("alice");

    auto query_result = wrapper.query_named(
        "SELECT id, user_id FROM users WHERE user_id = :user_id;",
        query_params
    );

    ASSERT_TRUE(query_result->done);
    ASSERT_TRUE(query_result->error_message.empty());
    ASSERT_EQ(query_result->results.size(), 1u);

    const auto& row = query_result->results.front();
    ASSERT_TRUE(row.contains("id"));
    ASSERT_TRUE(row.contains("user_id"));
    EXPECT_EQ(std::get<int>(row.at("id")), 1);
    EXPECT_EQ(std::get<std::string>(row.at("user_id")), "alice");

    sqlite3_close(db);
}