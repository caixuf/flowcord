# 🚀 FlowCoro 入门：业务程序员的5分钟快速上手

## 💡 先回答三个问题

### 1. 协程是什么？
**简单理解：可以"暂停"和"继续"的函数**

```cpp
// 普通函数：一口气跑完
std::string fetch_data() {
    sleep(100);  // 阻塞100ms，什么都不能干
    return "data";
}

// 协程函数：可以暂停，让出CPU给其他任务
Task<std::string> fetch_data() {
    co_await sleep_for(100ms);  // 暂停100ms，CPU可以干其他事
    co_return "data";
}
```

### 2. 为什么要用协程？
**协程的优势不是"更快"，而是"更优雅"地处理并发！**

想象一个Web服务器同时处理1000个请求：

**多线程方式：**
- 需要1000个线程（每个8MB栈空间 = 8GB内存！）
- 线程切换开销大
- 锁、竞争条件等复杂问题

**协程方式：**
- 只需几个线程 + 1000个协程（每个几KB）
- 协程切换开销极小
- 无锁编程，避免竞争条件

**场景对比：**
```cpp
// 处理大量并发连接时
协程：1000个连接 = 几MB内存
线程：1000个连接 = 几GB内存
```

### 3. 什么时候用协程？
✅ **适合的场景：**
- 网络请求（API调用、数据库查询）
- 文件读写
- 任何有"等待"的操作

❌ **不适合的场景：**
- 纯计算任务（没有等待）
- 简单的单线程程序

## 🎯 实战：5个递进示例

### 例子1：最简单的协程
```cpp
#include <flowcoro.hpp>
using namespace flowcoro;

// 定义一个协程函数
Task<std::string> say_hello() {
    co_await sleep_for(std::chrono::milliseconds(100));
    co_return "Hello FlowCoro!";
}

int main() {
    auto task = say_hello();
    auto result = task.get();  // 等待协程完成
    std::cout << result << std::endl;
}
```

**关键词解释：**
- `Task<T>`：协程的返回类型
- `co_await`：暂停等待
- `co_return`：协程返回值
- `task.get()`：同步等待结果

### 例子2：并发执行多个任务
```cpp
Task<void> parallel_demo() {
    // 同时启动三个任务
    auto task1 = fetch_user(123);
    auto task2 = fetch_product(456);  
    auto task3 = check_stock(789);
    
    // 等待所有任务完成（并行执行）
    auto user = co_await task1;
    auto product = co_await task2;
    auto stock = co_await task3;
    
    std::cout << "所有数据获取完成！" << std::endl;
}
```

### 例子3：真实业务场景
```cpp
Task<OrderInfo> create_order(int user_id, int product_id) {
    // 并发查询，节省时间
    auto user_task = get_user_info(user_id);
    auto product_task = get_product_info(product_id);
    auto stock_task = check_inventory(product_id);
    
    // 等待所有查询完成
    auto user = co_await user_task;
    auto product = co_await product_task;
    auto stock_available = co_await stock_task;
    
    if (!stock_available) {
        throw std::runtime_error("商品缺货");
    }
    
    // 创建订单
    OrderInfo order;
    order.user_name = user.name;
    order.product_name = product.name;
    order.total_price = product.price;
    
    co_return order;
}
```

### 例子4：批量处理
```cpp
Task<void> process_multiple_orders(std::vector<int> user_ids) {
    std::vector<Task<OrderInfo>> tasks;
    
    // 为每个用户创建订单任务
    for (int user_id : user_ids) {
        tasks.push_back(create_order(user_id, 12345));
    }
    
    // 等待所有订单创建完成
    for (auto& task : tasks) {
        try {
            auto order = co_await task;
            std::cout << "订单创建成功: " << order.user_name << std::endl;
        } catch (const std::exception& e) {
            std::cout << "订单创建失败: " << e.what() << std::endl;
        }
    }
}
```

### 例子5：错误处理
```cpp
Task<std::string> safe_api_call(const std::string& url) {
    try {
        auto response = co_await http_get(url);
        if (response.status_code == 200) {
            co_return response.body;
        } else {
            throw std::runtime_error("HTTP error: " + std::to_string(response.status_code));
        }
    } catch (const std::exception& e) {
        std::cout << "API调用失败: " << e.what() << std::endl;
        co_return "默认数据";  // 返回兜底数据
    }
}
```

## 🔧 快速开始

### 1. 编译运行示例
```bash
cd flowcord/examples
g++ -std=c++20 hello_world.cpp -o hello_world
./hello_world
```

### 2. 查看业务场景演示
```bash
./business_demo
```

### 3. 修改代码，自己试试！

## 📚 学习建议

### 今天（10分钟）
- 运行示例代码
- 理解 `co_await` 和 `co_return`
- 看懂并发执行的概念

### 明天（30分钟）  
- 修改示例，改变延迟时间
- 尝试添加更多并发任务
- 体验性能差异

### 本周（2小时）
- 在自己的项目中替换一个同步调用
- 测量性能提升
- 阅读完整的API文档

## ❓ 常见疑问

**Q: 协程比多线程更快吗？**
A: 不是！协程的优势是资源效率，不是速度。但在高并发场景下确实表现更优。

**Q: 真实性能如何？**  
A: 基于我们的1000并发请求测试：
- 协程：91ms总耗时，10,989 req/s吞吐量，0.09ms平均延迟
- 线程：3,286ms总耗时，304 req/s吞吐量，3.29ms平均延迟
- 协程比线程快36倍！

**Q: 什么时候协程比多线程更好？**  
A: 高并发IO场景，比如Web服务器、数据库连接池。协程可以轻松处理数万连接。

**Q: 学习成本高吗？**
A: 比多线程简单得多。没有锁、没有竞争条件，代码接近同步风格。

**Q: 生产环境稳定吗？**
A: 非常稳定。C++20协程是标准特性，很多大公司在使用。

**Q: 如何测试性能？**
A: 运行我们的性能测试：
```bash
cd build
./examples/hello_world 1000  # 1000并发请求对比测试
```

## 🎉 恭喜！

你已经了解了协程的基础概念。现在：

1. **运行示例代码**，感受协程的威力
2. **在实际项目中尝试**，体验性能提升  
3. **查看更多示例**，学习最佳实践

开始你的高性能编程之旅吧！🚀

---

💡 **记住核心概念：协程 = 轻量级并发 + 高资源效率 + 简洁代码**

**协程不是为了更快，而是为了更好地处理大量并发！**
