#include "gtest/gtest.h"

// ========================= 名称不同时计数不会累加  ========================= //
// 会被 GTest 及编辑器识别到。
// 左侧的 Test Explorer 会显示这个测试用例，
// 并且在运行时会执行这个测试用例，计数也会计数这个用例。
// 在编辑器中左侧也会显示测试结果按钮。
TEST(HelloWorldTest, BasicAssertions) {
    EXPECT_STREQ("Hello, World!", "Hello, World!");
    EXPECT_EQ(1 + 1, 2);
}

// 会被 GTest 及编辑器识别到。
// 与上一个测试用例是同一个测试套件（Test Suite），但不同的测试用例（Test Case），
// 计数会重新开始，不会累加 (1/1)。
TEST(HelloWorldTest, BasicAssertionsDifferentName) {
    EXPECT_STREQ("Hello, World!", "Hello, World!");
    EXPECT_EQ(1 + 1, 2);
}

// ========================== 名字空间中的测试用例  ========================== //
namespace MyTest {
// 名字空间内部的测试用例会被 GTest 识别到，但可能在编辑器左侧不会显示运行结果
// 可以将相关变量引出到名字空间外部再进行测试。
TEST(HelloWorldTest, BasicAssertionsNamespace) {
    EXPECT_STREQ("Hello, World!", "Hello, World!");
    EXPECT_EQ(1 + 1, 2);
}
}

// ========================================================================== //

// 如果你希望每个 `TEST_F` 开始前都有一套全新的环境（避免测试间互相干扰），
// 请使用 `SetUp()` 和 `TearDown()`。
namespace TestPart {
class MyService{
public:
    MyService(const std::string& db_path) : db_path_(db_path) {}
    void Init() {
        // 初始化数据库连接等资源
    }
    bool IsReady() const {
        // 检查服务是否准备就绪
        return true;
    }
private:
    std::string db_path_;
};

class MyServiceTest : public ::testing::Test {
protected:
    // 这里放公共属性
    MyService* service;
    std::string test_db_path;

    // 每个 TEST_F 运行前都会执行
    void SetUp() override {
        test_db_path = "/tmp/test_db";
        service = new MyService(test_db_path);
        service->Init();
        // 输出信息可以在测试面板的日志中看到。
        std::cout << "SetUp: Initialized MyService with db path: " << test_db_path << std::endl;
    }

    // 每个 TEST_F 运行后都会执行
    void TearDown() override {
        delete service;
        service = nullptr;
        // 输出信息可以在测试面板的日志中看到。
        std::cout << "TearDown: Cleaned up MyService" << std::endl;
    }
};

// 编辑器未识别到该测试用例。
// GTest 要求 TEST_F 的第一个参数必须精确匹配 Fixture 类的名称。
TEST_F(MyServiceTest, TestFeatureA) {
    EXPECT_TRUE(service->IsReady());
}
}
// 可将名字空间内部的变量引入到外部进行测试。
using MyServiceTest = TestPart::MyServiceTest;
TEST_F(MyServiceTest, TestFeatureA) {
    EXPECT_TRUE(service->IsReady());
}

// ========================================================================== //
// 如果初始化非常耗时（例如建立真实的数据库连接、加载大型模型），
// 你希望在该测试类下的**所有用例共享同一个实例**，请使用 `SetUpTestSuite()`。
namespace TestSuite {
class SharedResource {
public:
    void GlobalInit() {
        // 初始化共享资源，例如连接池、全局配置等
    }
};

class BigDataTest : public ::testing::Test {
protected:
    static SharedResource* resource;

    // 在整个 TestSuite 第一个用例开始前执行
    static void SetUpTestSuite() {
        resource = new SharedResource();
        resource->GlobalInit();
    }

    // 在整个 TestSuite 最后一个用例结束后执行
    static void TearDownTestSuite() {
        delete resource;
        resource = nullptr;
    }
};

// 静态成员变量需要在类外定义
SharedResource* BigDataTest::resource = nullptr;

// 这里的 test_fixture 名称要与 `::testing::Test` 子类一致，以便宏展开时正确调用
TEST_F(BigDataTest, TestCase1) {
    // 这里可以使用 resource 进行测试
    EXPECT_TRUE(resource != nullptr);
}
}

// ========================================================================== //
// 如果你需要在**所有测试用例**运行前后进行一些操作
// 例如初始化全局日志系统、启动 gRPC 的内存服务器，
// 你需要继承 `testing::Environment`。
namespace GlobalEnv {
class GlobalEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // 整个测试程序开始前执行
        std::cout << "--- Global Setup ---" << std::endl;
    }

    void TearDown() override {
        // 整个测试程序结束后执行
        std::cout << "--- Global TearDown ---" << std::endl;
    }
};

// 在 main 函数中注册
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new GlobalEnvironment);  // 这里需要调用实例化函数
    return RUN_ALL_TESTS();
}
}
