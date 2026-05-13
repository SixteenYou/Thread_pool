# Thread_pool
一个轻量级、高吞吐的 C++ 线程池实现 🚀
项目重点关注：
* 高并发任务调度
* 多生产者场景优化
* 动态线程扩缩容
* benchmark 性能测试
* 并发基础设施实验
本项目并非简单教学线程池，而是偏向：
> “高性能并发调度实验”
# 项目特点
* ✅ 动态线程扩缩容
* ✅ 支持自定义最大/最小线程数量
* ✅ 支持 `moodycamel::ConcurrentQueue`
* ✅ 高吞吐微任务调度
* ✅ 多生产者并发提交
* ✅ 自动线程负载检测
* ✅ benchmark 性能测试
* ✅ 轻量级设计
* ✅ 纯 C++ 实现
# 项目结构
Thread_pool/
│
├── threadpool/
│   ├── thread_pool.h
│   └── thread_pool.cpp
│
├── compare/
│   └── BS_thread_pool.hpp
│
├── third_party/
│   └── concurrentqueue.h
│
└── test.cpp
# 设计目标
本项目主要目标：
* 提高任务吞吐量
* 降低任务调度开销
* 优化多线程并发提交
* 研究线程动态扩缩容
* benchmark 驱动优化
因此：
本项目更偏向：
# “并发基础设施实验”
而不是传统：
# “教学线程池”
# 核心架构
## 线程池模型
text
生产者线程
     │
     ▼
任务队列
     │
     ▼
工作线程
     │
     ▼
任务执行
# 动态扩缩容机制
线程池会周期性检测：
当前线程数量
当前任务数量
任务负载阈值
当任务量超过当前线程负载时：
* 自动扩容线程
当任务量远低于当前负载时：
* 自动缩容线程
扩容策略：
* 默认倍增扩容
* 极端压力下直接扩容至最大线程数
这样能够减少频繁扩缩容带来的性能损耗。
# 队列实现
项目支持两种任务队列模式。
# 1. 普通队列模式
使用：
std::queue
std::mutex
std::condition_variable
特点：
* 空闲时 CPU 占用更低
* 逻辑更简单
# 2. ConcurrentQueue 模式（推荐）
使用：
moodycamel::ConcurrentQueue
特点：
* 大幅降低锁竞争
* 多生产者场景吞吐极高
* 更适合高并发任务提交
编译前定义：
#define concurrentqueue
即可启用。
# 第三方库
本项目引用：
* [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue)
许可证：
* Simplified BSD License
项目遵守原仓库开源协议。
# benchmark 测试
## 对比对象
本项目 benchmark 对比：
* wty::thread_pool
* [BS::thread_pool](https://github.com/bshoshany/thread-pool)
# benchmark 类型
## 1. 微任务测试（Micro Task）
测试极小任务调度性能。
例如：
done.fetch_add(1);
重点测试：
* 调度开销
* 队列竞争
* 提交效率
## 2. CPU 密集测试（CPU Heavy）
测试大量数学计算任务。
包括：
sin
cos
sqrt
重点测试：
* CPU 饱和场景
* 调度稳定性
* 高负载性能
## 3. 多生产者测试（Multi Producer）
多个线程同时提交任务。
重点测试：
* 多生产者竞争
* 并发提交性能
* MPMC 场景吞吐
## 4. 延迟测试（Latency）
测试：
* 平均调度延迟
* 最大调度延迟
重点：
* 调度响应速度
* 队列等待时间
* 调度抖动
# benchmark 测试结果
## 测试环境
Windows
MSVC
x64 Release
# 微任务测试
| 线程池           | 总耗时(ms)| 吞吐量(task/s) |
| BS::thread_pool  |    2706  |      369549   |
| wty::thread_pool |    344   |     2906976   |
## 结果分析
在微任务场景下：
wty::thread_pool
拥有非常高的吞吐性能。
主要原因：
* ConcurrentQueue 降低锁竞争
* 调度路径更轻量
* 提交开销更低
# CPU 密集测试
| 线程池            | 总耗时(ms) | 吞吐量(task/s) |
| BS::thread_pool  |    4558    |     10969     |
| wty::thread_pool |    4905    |     10193     |
## 结果分析
CPU 密集型任务下：
双方性能接近。
因为：
此时真正瓶颈已经变成：
# “CPU计算本身”
而不是线程调度。
# 多生产者测试
| 线程池           | 总耗时(ms) | 吞吐量(task/s) |
| BS::thread_pool  |   13901   |      115099    |
| wty::thread_pool |    197    |     8121827    |
## 结果分析
在多生产者场景下：
本项目吞吐量极高。
主要原因：
* ConcurrentQueue 高并发优化
* 降低 enqueue 锁竞争
* 更轻量的任务提交路径
# 延迟测试
| 线程池            | 平均延迟(ns) | 最大延迟(ns) |
| BS::thread_pool  |     1855     |   360000    |
| wty::thread_pool |    10536     |   368700    |
## 结果分析
本项目：
# 更偏向“高吞吐”
而不是：
# “低延迟”
因此：
* 平均延迟略高
* 但整体吞吐远高于传统线程池
这是设计上的主动取舍。
# 使用示例
#include "thread_pool.h"
int main()
{
    wty::thread_pool pool(4, 16);

    for (int i = 0; i < 1000; ++i)
    {
        pool.add_task([i]
        {
            std::cout << i << std::endl;
        });
    }

    return 0;
}
# 编译
## MSVC
```bash
cl /std:c++17 test.cpp
# 当前不足
当前版本依然存在：
* 中央任务队列
* ConcurrentQueue 模式下轻度轮询
* 未实现 work stealing
未来优化方向：
* work stealing
* 每线程本地队列
* NUMA 优化
* CPU affinity
* task graph
* lock-free 调度优化
# 未来计划

* work stealing
* 协程支持
* 更智能的 idle 策略
* Linux 测试
* 性能可视化
* 更完整 benchmark
* 无锁任务窃取
* 更低延迟调度
# 项目目的
本项目主要用于：
* 学习并发系统设计
* 研究线程调度
* benchmark 性能分析
* 高吞吐任务调度实验
核心目标：
尽可能降低高频任务系统中的调度开销。
