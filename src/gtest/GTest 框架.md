# GTest 测试框架

## GTest 的使用

1. 修改源文件后，需要在 VSCode 的 CMake 面板中重新点击配置与生成按钮，
   进行 `ALL_BUILD` 的编译，然后再点击测试按钮执行测试，
   否则新修改的测试代码不会被执行。

2. 调试测试用例时，直接点击调试按钮可能没有反应或编译不到测试用例或无法命中断点，
   可像调试正常程序一样，在 CMake 面板中选择当前测试用例的可执行程序进行编译、运行及调试。

3. 测试用例的 `std::cout` 输出可以在测试面板中的相关测试用例输出捕获中进行查看。

<!--
4. `TEST_F(test_fixture,test_name)` 中两个名称均完全一致时计数才会累积。

5. 名字空间中的 `TEST_F(test_fixture,test_name)` 可以被 GTest 识别到而累计计数，
   但编辑器将无法识别，左侧未添加运行按钮，可以引出相关变量到名字空间外部再调用。
-->

## GTest 公共属性提前初始化

在 GTest 中，处理“公共属性提前初始化”的需求主要有三个层级，
取决于你希望这些初始化代码的**作用域**（是每个测试用例跑一遍，还是整个测试套件跑一遍）。

---

### 1. Test Fixture (最常用：每个测试用例初始化)
如果你希望每个 `TEST_F` 开始前都有一套全新的环境（避免测试间互相干扰），请使用 `SetUp()` 和 `TearDown()`。

```cpp
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
    }

    // 每个 TEST_F 运行后都会执行
    void TearDown() override {
        delete service;
    }
};

// GTest 要求 TEST_F 的第一个参数必须精确匹配 Fixture 类的名称。
TEST_F(MyServiceTest, TestFeatureA) {
    EXPECT_TRUE(service->IsReady());
}
```

---

### 2. Static Setup (同类测试共享：只初始化一次)
如果初始化非常耗时（例如建立真实的数据库连接、加载大型模型），你希望在该测试类下的**所有用例共享同一个实例**，请使用 `SetUpTestSuite()`。

```cpp
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
```

---

### 3. Global Environment (全局初始化：整个程序生命周期)
如果你需要在**所有测试用例**运行前后进行一些操作（例如初始化全局日志系统、启动 gRPC 的内存服务器），你需要继承 `testing::Environment`。

```cpp
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
    ::testing::AddGlobalTestEnvironment(new GlobalEnvironment);
    return RUN_ALL_TESTS();
}
```

---

### 总结与对比

| 方法 | 作用范围 | 适用场景 |
| :--- | :--- | :--- |
| **`SetUp()`** | 每个 `TEST_F` | **最推荐**。保证测试隔离，防止数据污染。 |
| **`SetUpTestSuite()`** | 同一个类中的所有测试 | 耗时资源初始化（如网络池、配置文件解析）。 |
| **`Environment`** | 整个 binary 中所有测试 | 全局框架初始化（如 gRPC Runtime, Protobuf 库）。 |

**小建议：**
在 gRPC 的测试中，如果你的公共属性是 `grpc::ServerContext` 或 `ClientContext`，
请务必放在 `SetUp()` 中，因为 **Context 是一次性的**，不能在多个 RPC 调用间复用。
