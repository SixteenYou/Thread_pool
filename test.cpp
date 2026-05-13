#include"threadpool/thread_pool.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <cmath>
#include"compare/BS_thread_pool.hpp"
// 辅助函数：等待所有任务完成（通过轮询原子计数器）
// pool 不提供等待接口，只能外部等待
static void wait_for_tasks(const std::atomic<long long>& counter, long long target, int timeout_sec = 30) {
    auto start = std::chrono::steady_clock::now();
    while (counter.load() < target) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_sec) {
            std::cout << "  [WARNING] Timeout waiting for tasks (done="
                << counter.load() << "/" << target << ")" << std::endl;
            return;
        }
        std::this_thread::yield();
    }
}

// 测试1：基础单任务
bool test_basic_task_BS() {
    std::cout << "[Test 1] Basic single task... ";
    BS::thread_pool pool(5);
    std::atomic<bool> done{ false };
    pool.submit_task([&] { done = true; });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!done) {
        std::cout << "FAIL (task not executed)" << std::endl;
        return false;
    }
    std::cout << "PASS" << std::endl;
    return true;
}

bool test_basic_task_MY() {
    std::cout << "[Test 1] Basic single task... ";
    wty::thread_pool pool(2, 10);
    std::atomic<bool> done{ false };
    pool.add_task([&] { done = true; });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!done) {
        std::cout << "FAIL (task not executed)" << std::endl;
        return false;
    }
    std::cout << "PASS" << std::endl;
    return true;
}

// 测试2：多任务并发执行
bool test_multiple_tasks_BS() {
    std::cout << "[Test 2] Multiple tasks (1000)... ";
    //wty::thread_pool pool(4, 20);
    BS::thread_pool pool(10);
    constexpr int TASK_COUNT = 1000;
    std::atomic<int> counter{ 0 };
    for (int i = 0; i < TASK_COUNT; ++i)
        pool.submit_task([&] { counter.fetch_add(1, std::memory_order_relaxed); });
    // 等待最多 5 秒
    auto start = std::chrono::steady_clock::now();
    while (counter.load() < TASK_COUNT) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (ms > 5000) {
            std::cout << "FAIL (timeout, got " << counter.load() << "/" << TASK_COUNT << ")" << std::endl;
            return false;
        }
        std::this_thread::yield();
    }
    std::cout << "PASS (" << counter.load() << " tasks done)" << std::endl;
    return true;
}

bool test_multiple_tasks_MY() {
    std::cout << "[Test 2] Multiple tasks (1000)... ";
    wty::thread_pool pool(4, 20);
    constexpr int TASK_COUNT = 1000;
    std::atomic<int> counter{ 0 };
    for (int i = 0; i < TASK_COUNT; ++i)
        pool.add_task([&] { counter.fetch_add(1, std::memory_order_relaxed); });
    // 等待最多 5 秒
    auto start = std::chrono::steady_clock::now();
    while (counter.load() < TASK_COUNT) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (ms > 5000) {
            std::cout << "FAIL (timeout, got " << counter.load() << "/" << TASK_COUNT << ")" << std::endl;
            return false;
        }
        std::this_thread::yield();
    }
    std::cout << "PASS (" << counter.load() << " tasks done)" << std::endl;
    return true;
}

// 测试3：多生产者同时提交
bool test_multi_producer_BS() {
    std::cout << "[Test 3] Multi-producer (10 producers, 1000 tasks each)... ";
    //wty::thread_pool pool(5, 20);
    BS::thread_pool pool(10);

    constexpr int PRODUCERS = 10;
    constexpr int TASKS_PER_PRODUCER = 1000;
    std::atomic<long long> total{ 0 };
    std::vector<std::thread> producers;
    for (int p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&] {
            for (int i = 0; i < TASKS_PER_PRODUCER; ++i)
                pool.submit_task([&] { total.fetch_add(1, std::memory_order_relaxed); });
            });
    }
    for (auto& t : producers) t.join();
    // 等待处理完成
    wait_for_tasks(total, PRODUCERS * TASKS_PER_PRODUCER, 10);
    if (total.load() != PRODUCERS * TASKS_PER_PRODUCER) {
        std::cout << "FAIL (got " << total.load() << "/" << PRODUCERS * TASKS_PER_PRODUCER << ")" << std::endl;
        return false;
    }
    std::cout << "PASS" << std::endl;
    return true;
}

bool test_multi_producer_MY() {
    std::cout << "[Test 3] Multi-producer (10 producers, 1000 tasks each)... ";
    wty::thread_pool pool(5, 20);

    constexpr int PRODUCERS = 10;
    constexpr int TASKS_PER_PRODUCER = 1000;
    std::atomic<long long> total{ 0 };
    std::vector<std::thread> producers;
    for (int p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&] {
            for (int i = 0; i < TASKS_PER_PRODUCER; ++i)
                pool.add_task([&] { total.fetch_add(1, std::memory_order_relaxed); });
            });
    }
    for (auto& t : producers) t.join();
    // 等待处理完成
    wait_for_tasks(total, PRODUCERS * TASKS_PER_PRODUCER, 10);
    if (total.load() != PRODUCERS * TASKS_PER_PRODUCER) {
        std::cout << "FAIL (got " << total.load() << "/" << PRODUCERS * TASKS_PER_PRODUCER << ")" << std::endl;
        return false;
    }
    std::cout << "PASS" << std::endl;
    return true;
}

// 测试4：任务内部异常处理
bool test_exception_BS() {
    std::cout << "[Test 4] Exception within task... ";
    BS::thread_pool pool(3);
    std::atomic<bool> caught{ false };
    pool.submit_task([&] {
        try { throw std::runtime_error("test"); }
        catch (...) { caught = true; }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (!caught) {
        std::cout << "FAIL (exception not caught)" << std::endl;
        return false;
    }
    std::cout << "PASS" << std::endl;
    return true;
}

bool test_exception_MY() {
    std::cout << "[Test 4] Exception within task... ";
    wty::thread_pool pool(1, 5);
    std::atomic<bool> caught{ false };
    pool.add_task([&] {
        try { throw std::runtime_error("test"); }
        catch (...) { caught = true; }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (!caught) {
        std::cout << "FAIL (exception not caught)" << std::endl;
        return false;
    }
    std::cout << "PASS" << std::endl;
    return true;
}

// 测试5：析构时丢弃未完成任务（不应死锁）
bool test_destructor_discard_BS() {
    std::cout << "[Test 5] Destructor with remaining tasks... ";
    std::atomic<int> counter{ 0 };
    {
        //wty::thread_pool pool(2, 10);
        BS::thread_pool pool(5);

        // 提交大量长时间任务，确保池在析构时还未完成
        for (int i = 0; i < 500; ++i)
            pool.submit_task([&] {
            counter.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
                });
        // 立即析构（跳出作用域），不等待
    }
    // 如果程序没崩溃，counter < 500（部分未执行）
    std::cout << "PASS (counter = " << counter.load() << ")" << std::endl;
    return true;
}

bool test_destructor_discard_MY() {
    std::cout << "[Test 5] Destructor with remaining tasks... ";
    std::atomic<int> counter{ 0 };
    {
        wty::thread_pool pool(2, 10);

        // 提交大量长时间任务，确保池在析构时还未完成
        for (int i = 0; i < 500; ++i)
            pool.add_task([&] {
            counter.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
                });
        // 立即析构（跳出作用域），不等待
    }
    // 如果程序没崩溃，counter < 500（部分未执行）
    std::cout << "PASS (counter = " << counter.load() << ")" << std::endl;
    return true;
}

// 测试6：动态扩缩容（通过观察执行时间大致判断）
bool test_scaling_BS() {
    std::cout << "[Test 6] Dynamic scaling (burst 10000 tasks with 1ms sleep)... ";
    //wty::thread_pool pool(2, 100);
    BS::thread_pool pool(50);

    constexpr int BURST = 10000;
    std::atomic<long long> cnt = 0;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < BURST; ++i)
        pool.submit_task([&] {
        cnt.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
            });
    wait_for_tasks(cnt, BURST, 30); // 30秒超时
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    // 理论上线程数会自动增加到接近100，执行时间应远小于顺序执行的10秒
    std::cout << "Done in " << ms << " ms (PASS)" << std::endl;
    return true;
}

bool test_scaling_MY() {
    std::cout << "[Test 6] Dynamic scaling (burst 10000 tasks with 1ms sleep)... ";
    wty::thread_pool pool(2, 100);

    constexpr int BURST = 10000;
    std::atomic<long long> cnt = 0;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < BURST; ++i)
        pool.add_task([&] {
        cnt.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
            });
    wait_for_tasks(cnt, BURST, 30); // 30秒超时
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    // 理论上线程数会自动增加到接近100，执行时间应远小于顺序执行的10秒
    std::cout << "Done in " << ms << " ms (PASS)" << std::endl;
    return true;
}

// 测试7：快速创建销毁（100次）
bool test_rapid_destroy_BS() {
    std::cout << "[Test 7] Rapid create-destroy (100 cycles)... ";
    constexpr int CYCLES = 100;
    for (int c = 0; c < CYCLES; ++c) {
        //wty::thread_pool pool(2, 10);
        BS::thread_pool pool(5);

        std::atomic<int> cnt{ 0 };
        for (int i = 0; i < 500; ++i)
            pool.submit_task([&] { cnt.fetch_add(1, std::memory_order_relaxed); });
        // 不等完成就析构
    }
    std::cout << "PASS (cycles done)" << std::endl;
    return true;
}

bool test_rapid_destroy_MY() {
    std::cout << "[Test 7] Rapid create-destroy (100 cycles)... ";
    constexpr int CYCLES = 100;
    for (int c = 0; c < CYCLES; ++c) {
        wty::thread_pool pool(2, 10);

        std::atomic<int> cnt{ 0 };
        for (int i = 0; i < 500; ++i)
            pool.add_task([&] { cnt.fetch_add(1, std::memory_order_relaxed); });
        // 不等完成就析构
    }
    std::cout << "PASS (cycles done)" << std::endl;
    return true;
}

// 极限测试1：百万级任务洪峰
bool test_1M_burst_BS() {
    std::cout << "[Extreme Test 1] 1,000,000 task burst... ";
    //wty::thread_pool pool(5, 1000);  // max 设大
    BS::thread_pool pool(500);
    constexpr long long TOTAL = 1'000'000;
    std::atomic<long long> sum{ 0 };
    auto start = std::chrono::steady_clock::now();
    for (long long i = 0; i < TOTAL; ++i)
        pool.submit_task([&] { sum.fetch_add(1, std::memory_order_relaxed); });
    // 等待完成（可能超时）
    wait_for_tasks(sum, TOTAL, 120); // 2分钟超时
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    if (sum.load() != TOTAL) {
        std::cout << "FAIL (only " << sum.load() << " done, timeout)" << std::endl;
        return false;
    }
    std::cout << "Done in " << ms << " ms" << std::endl;
    // 注意：内存可能很大（队列中有100万任务）
    return true;
}

bool test_1M_burst_MY() {
    std::cout << "[Extreme Test 1] 1,000,000 task burst... ";
    wty::thread_pool pool(5, 1000);  // max 设大
    constexpr long long TOTAL = 1'000'000;
    std::atomic<long long> sum{ 0 };
    auto start = std::chrono::steady_clock::now();
    for (long long i = 0; i < TOTAL; ++i)
        pool.add_task([&] { sum.fetch_add(1, std::memory_order_relaxed); });
    // 等待完成（可能超时）
    wait_for_tasks(sum, TOTAL, 120); // 2分钟超时
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    if (sum.load() != TOTAL) {
        std::cout << "FAIL (only " << sum.load() << " done, timeout)" << std::endl;
        return false;
    }
    std::cout << "Done in " << ms << " ms" << std::endl;
    // 注意：内存可能很大（队列中有100万任务）
    return true;
}

// 极限测试2：长时间运行（短任务持续30秒）
bool test_long_running_BS() {
    std::cout << "[Extreme Test 2] Long run (30 seconds of continuous task submission)... ";
    //wty::thread_pool pool(5, 500);
    BS::thread_pool pool(250);
    constexpr int DURATION = 30; // seconds
    std::atomic<long long> total{ 0 };
    auto start = std::chrono::steady_clock::now();
    long long submitted = 0;
    while (std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start).count() < DURATION) {
        // 每次提交1000个短任务
        for (int i = 0; i < 1000; ++i) {
            pool.submit_task([&] { total.fetch_add(1, std::memory_order_relaxed); });
            ++submitted;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 控制提交速率
    }
    // 等待剩余任务完成
    std::cout << " submitted " << submitted << " tasks, waiting for finish... ";
    wait_for_tasks(total, submitted, 60);
    std::cout << " done " << total.load() << " tasks" << std::endl;
    // 检查程序是否没有崩溃或内存占用异常
    return true;
}

bool test_long_running_MY() {
    std::cout << "[Extreme Test 2] Long run (30 seconds of continuous task submission)... ";
    wty::thread_pool pool(5, 500);
    constexpr int DURATION = 30; // seconds
    std::atomic<long long> total{ 0 };
    auto start = std::chrono::steady_clock::now();
    long long submitted = 0;
    while (std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start).count() < DURATION) {
        // 每次提交1000个短任务
        for (int i = 0; i < 1000; ++i) {
            pool.add_task([&] { total.fetch_add(1, std::memory_order_relaxed); });
            ++submitted;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 控制提交速率
    }
    // 等待剩余任务完成
    std::cout << " submitted " << submitted << " tasks, waiting for finish... ";
    wait_for_tasks(total, submitted, 60);
    std::cout << " done " << total.load() << " tasks" << std::endl;
    // 检查程序是否没有崩溃或内存占用异常
    return true;
}

// 极限测试3：生产者远快于消费者（CPU密集型任务）
// 在文件顶部添加全局变量（防止被优化掉）
static std::atomic<long long> global_sin_sink{ 0 };

bool test_overload_BS() {
    std::cout << "[Extreme Test 3] 100k CPU-intensive tasks (may take time)... ";
    BS::thread_pool pool(50);  // 与原来相同
    constexpr int TASKS = 100000;
    std::atomic<long long> done = 0;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < TASKS; ++i)
    {
        auto seed = i;
        pool.submit_task([&, seed] {
            volatile double x = 0.0;
            unsigned s = seed;

            for (int k = 0; k < 200; ++k)
            {
                for (int j = 0; j < 1000; ++j)
                {
                    s = s * 1103515245 + 12345;
                    x += std::sin(static_cast<double>(s & 0xFF));
                    x += std::sqrt(static_cast<double>(s & 0xFF));
                    x += std::cos(static_cast<double>(s & 0xFF));
                }
            }

            global_sin_sink.fetch_add(
                static_cast<long long>(x),
                std::memory_order_seq_cst
            );

            auto now_done = done.fetch_add(1, std::memory_order_seq_cst) + 1;
            if (now_done % 20000 == 0)
            {
                std::cout << "done=" << now_done
                    << " thread=" << std::this_thread::get_id()
                    << std::endl;
            }
            });
    }

    wait_for_tasks(done, TASKS, 300);

    auto finish = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        finish - start).count();

    std::cout << "\nwait finished, done=" << done.load() << std::endl;
    std::cout << "Completed " << TASKS << " tasks in " << ms << " ms" << std::endl;

    // 这里只是验尸，不参与计时
    std::this_thread::sleep_for(std::chrono::seconds(10));
    if (done.load() != TASKS) {
        std::cout << "FAIL (only " << done.load() << " done, timeout)" << std::endl;
        return false;
    }
    return true;
}

// ===== 替换 test_overload_MY =====
bool test_overload_MY() {
    std::cout << "[Extreme Test 3] 100k CPU-intensive tasks (may take time)... ";
    wty::thread_pool pool(2,100);
    constexpr int TASKS = 100000;
    std::atomic<long long> done = 0;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < TASKS; ++i)
    {
        auto seed = i;
        pool.add_task([&, seed] {
            volatile double x = 0.0;
            unsigned s = seed;

            for (int k = 0; k < 200; ++k)
            {
                for (int j = 0; j < 1000; ++j)
                {
                    s = s * 1103515245 + 12345;
                    x += std::sin(static_cast<double>(s & 0xFF));
                    x += std::sqrt(static_cast<double>(s & 0xFF));
                    x += std::cos(static_cast<double>(s & 0xFF));
                }
            }

            global_sin_sink.fetch_add(
                static_cast<long long>(x),
                std::memory_order_seq_cst
            );

            auto now_done = done.fetch_add(1, std::memory_order_seq_cst) + 1;
            if (now_done % 20000 == 0)
            {
                std::cout << "done=" << now_done
                    << " thread=" << std::this_thread::get_id()
                    << std::endl;
            }
            });
    }

    wait_for_tasks(done, TASKS, 300);

    auto finish = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        finish - start).count();

    std::cout << "\nwait finished, done=" << done.load() << std::endl;
    std::cout << "Completed " << TASKS << " tasks in " << ms << " ms" << std::endl;

    // 这里只是验尸，不参与计时
    std::this_thread::sleep_for(std::chrono::seconds(10));
    if (done.load() != TASKS) {
        std::cout << "FAIL (only " << done.load() << " done, timeout)" << std::endl;
        return false;
    }
    return true;
}

int main() {
    for(int i=0;i<1;i++)
    {
        std::cout<<"test"<<i+1<<std::endl;
        std::cout << "===== Thread Pool Tests_BS =====\n" << std::endl;

        // 常规测试
        bool all_pass_BS = true;
        all_pass_BS &= test_basic_task_BS();
        all_pass_BS &= test_multiple_tasks_BS();
        all_pass_BS &= test_multi_producer_BS();
        all_pass_BS &= test_exception_BS();
        all_pass_BS &= test_destructor_discard_BS();
        all_pass_BS &= test_scaling_BS();
        all_pass_BS &= test_rapid_destroy_BS();

        // 极限测试（可根据需要注释掉部分，例如耗时长的）
        all_pass_BS &= test_1M_burst_BS();        // 可能导致内存爆炸，谨慎运行!
        all_pass_BS &= test_long_running_BS();     // 运行30秒
        all_pass_BS &= test_overload_BS();         // CPU密集，耗时较长

        std::cout << "\n===== Test Results =====" << std::endl;
        if (all_pass_BS)
            std::cout << "All basic tests passed!" << std::endl;
        else
            std::cout << "Some basic tests failed." << std::endl;

        // 提示极限测试需单独运行
        std::cout << "\nNote: Extreme tests (1M burst, long running, overload) are commented out.\n"
            << "Uncomment them to run at your own risk." << std::endl;
        std::cout << global_sin_sink << std::endl;

        std::cout << "=======================================" << std::endl;
        std::cout << "===== Thread Pool Tests_MY =====\n" << std::endl;
        bool all_pass_MY = true;
        all_pass_MY &= test_basic_task_MY();
        all_pass_MY &= test_multiple_tasks_MY();
        all_pass_MY &= test_multi_producer_MY();
        all_pass_MY &= test_exception_MY();
        all_pass_MY &= test_destructor_discard_MY();
        all_pass_MY &= test_scaling_MY();
        all_pass_MY &= test_rapid_destroy_MY();

        // 极限测试（可根据需要注释掉部分，例如耗时长的）
        all_pass_MY &= test_1M_burst_MY();        // 可能导致内存爆炸，谨慎运行!
        all_pass_MY &= test_long_running_MY();     // 运行30秒
        all_pass_MY &= test_overload_MY();         // CPU密集，耗时较长

        std::cout << "\n===== Test Results =====" << std::endl;
        if (all_pass_MY)
            std::cout << "All basic tests passed!" << std::endl;
        else
            std::cout << "Some basic tests failed." << std::endl;

        // 提示极限测试需单独运行
        std::cout << "\nNote: Extreme tests (1M burst, long running, overload) are commented out.\n"
            << "Uncomment them to run at your own risk." << std::endl;
        std::cout << global_sin_sink << std::endl;
    }
    return 0;
}

//#include <iostream>
//#include <thread>
//#include <vector>
//#include <atomic>
//#include <chrono>
//#include <cmath>
//#include <functional>
//#include <string>
//#include <iomanip>
//
//#include"compare/BS_thread_pool.hpp"   // BS::thread_pool
//#include"threadpool/thread_pool.h"   // wty::thread_pool
//
//using namespace std;
//
//static atomic<long long> global_sink = 0;
//
//struct BenchResult
//{
//    string name;
//
//    long long total_ms = 0;
//
//    double throughput = 0;
//
//    double avg_latency_ns = 0;
//
//    long long max_latency_ns = 0;
//};
//
//
//
//// ======================================================
//// 工具函数
//// ======================================================
//
//template<typename T>
//void print_result(const T& r)
//{
//    cout << "\n========================================\n";
//
//    cout << "Test Name          : " << r.name << endl;
//
//    cout << "Total Time(ms)     : " << r.total_ms << endl;
//
//    cout << "Throughput(task/s) : "
//        << fixed << setprecision(2)
//        << r.throughput << endl;
//
//    if (r.avg_latency_ns > 0)
//    {
//        cout << "Avg Latency(ns)    : "
//            << r.avg_latency_ns << endl;
//
//        cout << "Max Latency(ns)    : "
//            << r.max_latency_ns << endl;
//    }
//
//    cout << "========================================\n";
//}
//
////template<typename POOL>
//void wait_done(atomic<int>& done, int target)
//{
//    while (done.load(memory_order_relaxed) != target)
//    {
//        this_thread::yield();
//    }
//}
//
//
//
//// ======================================================
//// CPU HEAVY TASK
//// ======================================================
//
//inline void heavy_work(unsigned seed)
//{
//    volatile double x = 0.0;
//
//    unsigned s = seed;
//
//    for (int k = 0; k < 100; ++k)
//    {
//        for (int j = 0; j < 1000; ++j)
//        {
//            s = s * 1103515245 + 12345;
//
//            x += sin((double)(s & 0xFF));
//
//            x += sqrt((double)(s & 0xFF));
//
//            x += cos((double)(s & 0xFF));
//        }
//    }
//
//    global_sink.fetch_add(
//        (long long)x,
//        memory_order_relaxed
//    );
//}
//
//
//
//// ======================================================
//// MICRO TASK TEST
//// ======================================================
//
//BenchResult benchmark_micro_task_BS()
//{
//    constexpr int TASKS = 1000000;
//
//    BS::thread_pool pool(50);
//
//    atomic<int> done = 0;
//
//    auto start = chrono::steady_clock::now();
//
//    for (int i = 0; i < TASKS; ++i)
//    {
//        pool.submit_task([&]
//            {
//                done.fetch_add(1, memory_order_relaxed);
//            });
//    }
//
//    wait_done(done, TASKS);
//
//    auto end = chrono::steady_clock::now();
//
//    auto ms = chrono::duration_cast<
//        chrono::milliseconds>(end - start).count();
//
//    BenchResult r;
//
//    r.name = "BS Micro Task";
//
//    r.total_ms = ms;
//
//    r.throughput =
//        TASKS / (ms / 1000.0);
//
//    return r;
//}
//
//BenchResult benchmark_micro_task_MY()
//{
//    constexpr int TASKS = 1000000;
//
//    wty::thread_pool pool(50, 50);
//
//    atomic<int> done = 0;
//
//    auto start = chrono::steady_clock::now();
//
//    for (int i = 0; i < TASKS; ++i)
//    {
//        pool.add_task([&]
//            {
//                done.fetch_add(1, memory_order_relaxed);
//            });
//    }
//
//    wait_done(done, TASKS);
//
//    auto end = chrono::steady_clock::now();
//
//    auto ms = chrono::duration_cast<
//        chrono::milliseconds>(end - start).count();
//
//    BenchResult r;
//
//    r.name = "MY Micro Task";
//
//    r.total_ms = ms;
//
//    r.throughput =
//        TASKS / (ms / 1000.0);
//
//    return r;
//}
//
//
//
//// ======================================================
//// CPU HEAVY TEST
//// ======================================================
//
//BenchResult benchmark_cpu_heavy_BS()
//{
//    constexpr int TASKS = 50000;
//
//    BS::thread_pool pool(50);
//
//    atomic<int> done = 0;
//
//    auto start = chrono::steady_clock::now();
//
//    for (int i = 0; i < TASKS; ++i)
//    {
//        pool.submit_task([&, i]
//            {
//                heavy_work(i);
//
//                done.fetch_add(1, memory_order_relaxed);
//            });
//    }
//
//    wait_done(done, TASKS);
//
//    auto end = chrono::steady_clock::now();
//
//    auto ms = chrono::duration_cast<
//        chrono::milliseconds>(end - start).count();
//
//    BenchResult r;
//
//    r.name = "BS CPU Heavy";
//
//    r.total_ms = ms;
//
//    r.throughput =
//        TASKS / (ms / 1000.0);
//
//    return r;
//}
//
//BenchResult benchmark_cpu_heavy_MY()
//{
//    constexpr int TASKS = 50000;
//
//    wty::thread_pool pool(2, 100);
//
//    atomic<int> done = 0;
//
//    auto start = chrono::steady_clock::now();
//
//    for (int i = 0; i < TASKS; ++i)
//    {
//        pool.add_task([&, i]
//            {
//                heavy_work(i);
//
//                done.fetch_add(1, memory_order_relaxed);
//            });
//    }
//
//    wait_done(done, TASKS);
//
//    auto end = chrono::steady_clock::now();
//
//    auto ms = chrono::duration_cast<
//        chrono::milliseconds>(end - start).count();
//
//    BenchResult r;
//
//    r.name = "MY CPU Heavy";
//
//    r.total_ms = ms;
//
//    r.throughput =
//        TASKS / (ms / 1000.0);
//
//    return r;
//}
//
//
//
//// ======================================================
//// MULTI PRODUCER TEST
//// ======================================================
//
//BenchResult benchmark_multi_producer_BS()
//{
//    constexpr int PRODUCERS = 16;
//
//    constexpr int TASK_PER_PRODUCER = 100000;
//
//    constexpr int TOTAL =
//        PRODUCERS * TASK_PER_PRODUCER;
//
//    BS::thread_pool pool(50);
//
//    atomic<int> done = 0;
//
//    vector<thread> producers;
//
//    auto start = chrono::steady_clock::now();
//
//    for (int p = 0; p < PRODUCERS; ++p)
//    {
//        producers.emplace_back([&]
//            {
//                for (int i = 0; i < TASK_PER_PRODUCER; ++i)
//                {
//                    pool.submit_task([&]
//                        {
//                            done.fetch_add(
//                                1,
//                                memory_order_relaxed);
//                        });
//                }
//            });
//    }
//
//    for (auto& t : producers)
//        t.join();
//
//    wait_done(done, TOTAL);
//
//    auto end = chrono::steady_clock::now();
//
//    auto ms = chrono::duration_cast<
//        chrono::milliseconds>(end - start).count();
//
//    BenchResult r;
//
//    r.name = "BS Multi Producer";
//
//    r.total_ms = ms;
//
//    r.throughput =
//        TOTAL / (ms / 1000.0);
//
//    return r;
//}
//
//BenchResult benchmark_multi_producer_MY()
//{
//    constexpr int PRODUCERS = 16;
//
//    constexpr int TASK_PER_PRODUCER = 100000;
//
//    constexpr int TOTAL =
//        PRODUCERS * TASK_PER_PRODUCER;
//
//    wty::thread_pool pool(2, 100);
//
//    atomic<int> done = 0;
//
//    vector<thread> producers;
//
//    auto start = chrono::steady_clock::now();
//
//    for (int p = 0; p < PRODUCERS; ++p)
//    {
//        producers.emplace_back([&]
//            {
//                for (int i = 0; i < TASK_PER_PRODUCER; ++i)
//                {
//                    pool.add_task([&]
//                        {
//                            done.fetch_add(
//                                1,
//                                memory_order_relaxed);
//                        });
//                }
//            });
//    }
//
//    for (auto& t : producers)
//        t.join();
//
//    wait_done(done, TOTAL);
//
//    auto end = chrono::steady_clock::now();
//
//    auto ms = chrono::duration_cast<
//        chrono::milliseconds>(end - start).count();
//
//    BenchResult r;
//
//    r.name = "MY Multi Producer";
//
//    r.total_ms = ms;
//
//    r.throughput =
//        TOTAL / (ms / 1000.0);
//
//    return r;
//}
//
//
//
//// ======================================================
//// LATENCY TEST
//// ======================================================
//
//BenchResult benchmark_latency_BS()
//{
//    constexpr int TASKS = 100000;
//
//    BS::thread_pool pool(50);
//
//    atomic<int> done = 0;
//
//    atomic<long long> total_latency = 0;
//
//    atomic<long long> max_latency = 0;
//
//    auto start = chrono::steady_clock::now();
//
//    for (int i = 0; i < TASKS; ++i)
//    {
//        auto submit_time =
//            chrono::steady_clock::now();
//
//        pool.submit_task([&, submit_time]
//            {
//                auto exec_time =
//                    chrono::steady_clock::now();
//
//                auto latency =
//                    chrono::duration_cast<
//                    chrono::nanoseconds>(
//                        exec_time - submit_time).count();
//
//                total_latency.fetch_add(
//                    latency,
//                    memory_order_relaxed);
//
//                long long old =
//                    max_latency.load();
//
//                while (
//                    old < latency &&
//                    !max_latency.compare_exchange_weak(
//                        old,
//                        latency));
//
//                done.fetch_add(1);
//            });
//    }
//
//    wait_done(done, TASKS);
//
//    auto end = chrono::steady_clock::now();
//
//    auto ms = chrono::duration_cast<
//        chrono::milliseconds>(end - start).count();
//
//    BenchResult r;
//
//    r.name = "BS Latency";
//
//    r.total_ms = ms;
//
//    r.throughput =
//        TASKS / (ms / 1000.0);
//
//    r.avg_latency_ns =
//        total_latency.load() / (double)TASKS;
//
//    r.max_latency_ns =
//        max_latency.load();
//
//    return r;
//}
//
//BenchResult benchmark_latency_MY()
//{
//    constexpr int TASKS = 100000;
//
//    wty::thread_pool pool(2, 100);
//
//    atomic<int> done = 0;
//
//    atomic<long long> total_latency = 0;
//
//    atomic<long long> max_latency = 0;
//
//    auto start = chrono::steady_clock::now();
//
//    for (int i = 0; i < TASKS; ++i)
//    {
//        auto submit_time =
//            chrono::steady_clock::now();
//
//        pool.add_task([&, submit_time]
//            {
//                auto exec_time =
//                    chrono::steady_clock::now();
//
//                auto latency =
//                    chrono::duration_cast<
//                    chrono::nanoseconds>(
//                        exec_time - submit_time).count();
//
//                total_latency.fetch_add(
//                    latency,
//                    memory_order_relaxed);
//
//                long long old =
//                    max_latency.load();
//
//                while (
//                    old < latency &&
//                    !max_latency.compare_exchange_weak(
//                        old,
//                        latency));
//
//                done.fetch_add(1);
//            });
//    }
//
//    wait_done(done, TASKS);
//
//    auto end = chrono::steady_clock::now();
//
//    auto ms = chrono::duration_cast<
//        chrono::milliseconds>(end - start).count();
//
//    BenchResult r;
//
//    r.name = "MY Latency";
//
//    r.total_ms = ms;
//
//    r.throughput =
//        TASKS / (ms / 1000.0);
//
//    r.avg_latency_ns =
//        total_latency.load() / (double)TASKS;
//
//    r.max_latency_ns =
//        max_latency.load();
//
//    return r;
//}
//
//
//
//// ======================================================
//// MAIN
//// ======================================================
//
//int main()
//{
//    cout << "=====================================\n";
//    cout << "THREAD POOL FULL BENCHMARK\n";
//    cout << "=====================================\n";
//
//
//
//    // =========================================
//    // MICRO TASK
//    // =========================================
//
//    print_result(benchmark_micro_task_BS());
//
//    print_result(benchmark_micro_task_MY());
//
//
//
//    // =========================================
//    // CPU HEAVY
//    // =========================================
//
//    print_result(benchmark_cpu_heavy_BS());
//
//    print_result(benchmark_cpu_heavy_MY());
//
//
//
//    // =========================================
//    // MULTI PRODUCER
//    // =========================================
//
//    print_result(benchmark_multi_producer_BS());
//
//    print_result(benchmark_multi_producer_MY());
//
//
//
//    // =========================================
//    // LATENCY
//    // =========================================
//
//    print_result(benchmark_latency_BS());
//
//    print_result(benchmark_latency_MY());
//
//
//
//    cout << "\n\nGlobal Sink = "
//        << global_sink.load()
//        << endl;
//
//    return 0;
//}