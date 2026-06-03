#pragma once
#include "headers.h"

class ThreadPool {
public:
    // 构造函数：启动指定数量的工作线程
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency()) : m_stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            m_workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        // 锁定任务队列
                        std::unique_lock<std::mutex> lock(this->m_queueMutex);

                        // 等待新任务到来，或者线程池停止通知
                        this->m_cv.wait(lock, [this] {
                            return this->m_stop || !this->m_tasks.empty();
                        });

                        // 如果线程池停止且队列为空，该工作线程安全退出
                        if (this->m_stop && this->m_tasks.empty()) {
                            return;
                        }

                        // 从队列首部取出任务
                        task = std::move(this->m_tasks.front());
                        this->m_tasks.pop();
                    }

                    // 在锁外执行任务，避免阻塞其他工作线程取任务
                    task();
                }
            });
        }
    }

    // 🛑 核心模板函数：支持任意参数、任意返回值的任务投递
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        // 自动推导可调用对象 F 在传入参数 Args... 时的返回值类型
        using return_type = std::invoke_result_t<F, Args...>;

        // 1. 将可调用对象与参数绑定，并封装为 packaged_task 智能指针
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        // 2. 从 packaged_task 中获取与之关联的 future
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);

            if (m_stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            // 3. 将任务包装为 void() 类型的闭包，推入任务队列
            m_tasks.emplace([task]() {
                (*task)();
            });
        }

        // 4. 唤醒一个正在静默等待的工作线程
        m_cv.notify_one();

        return res;
    }

    // 析构函数：优雅停止所有线程
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_stop = true;
        }
        // 唤醒所有阻塞在 wait 处的线程
        m_cv.notify_all();

        // 汇合（Join）所有工作线程，确保内存及文件句柄安全释放
        for (std::thread &worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    // 工作线程容器
    std::vector<std::thread> m_workers;
    // 任务队列
    std::queue<std::function<void()>> m_tasks;

    // 同步基础设施
    std::mutex m_queueMutex;
    std::condition_variable m_cv;

    // 状态控制标志
    bool m_stop;
};

namespace TestThreadPool {

// 场景 1：普通带多参数、带返回值的函数
int add(int a, int b, const std::string& msg) {
    std::cout << "[Thread " << std::this_thread::get_id() << "] " << msg << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 模拟耗时
    return a + b;
}

// 场景 2：业务类及其成员函数
class DatabaseService {
public:
    std::string queryUser(int userId, const std::string& token) {
        // 模拟复杂加密或慢查询
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        return "User_Data_For_" + std::to_string(userId);
    }
};

int main() {
    // 初始化一个包含 4 个线程的线程池
    ThreadPool pool(4);

    // -------------------------------------------------------------
    // 测试 1: 投递普通函数（传入 3 个不同类型的参数，并获取未来返回值）
    // -------------------------------------------------------------
    auto future1 = pool.enqueue(add, 42, 58, "Processing calculation task...");

    // -------------------------------------------------------------
    // 测试 2: 投递带参数的 Lambda 表达式（无需获取返回值，丢下不管）
    // -------------------------------------------------------------
    pool.enqueue([](const std::string& name, int age) {
        std::cout << "[Thread " << std::this_thread::get_id()
                  << "] Lambda executed for: " << name << ", age: " << age << std::endl;
    }, "Alice", 25);

    // -------------------------------------------------------------
    // 测试 3: 投递类的成员函数（需要绑定对象指针 &db）
    // -------------------------------------------------------------
    DatabaseService db;
    auto future2 = pool.enqueue(&DatabaseService::queryUser, &db, 1001, "session_token_xyz");

    // -------------------------------------------------------------
    // 测试 4: 完美契合你之前的 gRPC 异步状态机场景
    // -------------------------------------------------------------
    // 假定你在 RpcSignUp 状态机中，可以直接丢给线程池，而绝对不会卡死网关的 CQ 网络流
    /*
    m_threadPool.enqueue([this, params]() {
        DBSQLCipher& db_sqlcipher = DBSQLCipher::instance();
        m_dbResult = db_sqlcipher.sign_up(params);
        // 执行完毕后扔回 gRPC 完成队列
        m_alarm.Set(m_cq, gpr_now(GPR_CLOCK_REALTIME), this);
    });
    */

    // -------------------------------------------------------------
    // 获取异步执行结果（.get() 会阻塞当前主线程直到后台任务算完）
    // -------------------------------------------------------------
    std::cout << ">>> Main thread is waiting for result 1..." << std::endl;
    std::cout << "Result 1 (add): " << future1.get() << std::endl;

    std::cout << ">>> Main thread is waiting for result 2..." << std::endl;
    std::cout << "Result 2 (DB Query): " << future2.get() << std::endl;

    std::cout << ">>> All tasks finalized. Program exiting safely." << std::endl;
    return 0;
}
}
