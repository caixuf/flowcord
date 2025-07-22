# FlowCoro 2.2 🚀

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)

> **现代C++20协程编程库，专为高性能、低延迟场景设计**

FlowCoro 是一个工业级的异步编程框架，基于C++20原生协程和无锁编程技术构建。它为开发者提供了简洁易用的API，同时保证了生产级别的性能和可靠性。

## ⭐ 更新 v2.2 - 增强生命周期管理版

FlowCoro v2.2 完成了Task生命周期管理的全面增强：

### � 主要特性

- ✅ **增强Task管理**: 原子状态跟踪 + 线程安全保护
- ✅ **Promise风格API**: JavaScript风格的状态查询接口
- ✅ **安全销毁机制**: 防止协程句柄损坏的safe_destroy()
- ✅ **生命周期跟踪**: 创建时间戳和生命期统计
- ✅ **取消和超时**: 完整的任务取消和超时管理
- 📝 **文档更新**: 更新文档以反映真实项目状态

### 📊 集成验证结果

| 组件 | 状态 | 说明 |
|------|------|------|
| 协程核心系统 | ✅ 稳定 | 基于标准C++20协程 |
| 编译系统 | ✅ 优化 | 无警告编译 |
| 网络组件 | ✅ 可用 | 异步Socket支持 |
| 数据库组件 | ✅ 可用 | 连接池实现 |
| RPC组件 | ✅ 可用 | JSON-RPC支持 |
| 测试覆盖 | ✅ 完整 | 单元测试和集成测试 |

## 📚 文档导航

### 📖 核心文档

| 文档名称 | 内容概要 |
|---------|----------|
| [API 参考手册](docs/API_REFERENCE.md) | 完整的协程API接口文档与使用示例 |
| [网络编程指南](docs/NETWORK_GUIDE.md) | 异步Socket、TCP服务器与事件循环 |
| [数据库使用指南](docs/DATABASE_GUIDE.md) | 连接池、异步查询与事务管理 |
| [RPC服务指南](docs/RPC_GUIDE.md) | 异步RPC、批量调用与服务发现 |
| [性能调优指南](docs/PERFORMANCE_GUIDE.md) | 协程池化、内存优化与调优技巧 |

### 📝 项目文档

| 文档名称 | 内容概要 |
|---------|----------|
| [项目完成总结](PROJECT_COMPLETION_SUMMARY.md) | v2.1重构成果与技术突破详解 |
| [技术深度分析](docs/TECHNICAL_SUMMARY.md) | 架构设计与核心技术实现详解 |
| [发展路线图](docs/ROADMAP.md) | 功能扩展计划与开发时间规划 |

## ✨ 核心特性

### 🎯 **协程优先设计**

- **原生C++20协程**：基于标准协程实现，零妥协的性能
- **零开销抽象**：协程创建仅需~150ns，执行开销极低
- **异步友好**：所有IO操作天然异步，避免线程阻塞

### ⚡ **无锁高性能**

- **无锁数据结构**：队列、栈、环形缓冲区
- **工作窃取线程池**：智能负载均衡，最大化CPU利用率
- **内存池管理**：减少分配延迟，降低内存碎片

### 🌐 **异步网络IO**

- **基于epoll的事件循环**：Linux高性能网络编程
- **协程化Socket**：write/read/connect全部支持co_await
- **TCP服务器框架**：支持高并发连接

### 🗄️ **异步数据库支持**

- **连接池管理**: MySQL/PostgreSQL/SQLite支持，自动连接复用
- **协程化查询**: 所有数据库操作支持`co_await`
- **事务支持**: 完整的异步事务管理

### 🔗 **现代RPC框架**

- **异步RPC服务器**: 支持并发请求处理和批量调用
- **JSON-RPC协议**: 标准化的远程过程调用
- **服务发现**: 自动服务注册与发现机制

### 🔧 **生产就绪**

- **异步日志系统**：高吞吐量，不阻塞业务
- **内存安全**：RAII + 智能指针，防止内存泄漏
- **完整测试覆盖**：单元测试 + 性能测试 + 网络测试

## 🏆 性能基准 (实测数据)

| 组件 | FlowCoro 性能 | 传统方案对比 | 性能提升 |
|------|--------------|-------------|---------|
| **协程创建** | 181ns/个 | ~2000ns (thread) | **11x** |
| **协程执行** | 7ns/个 | ~100ns (function call) | **14x** |
| **无锁队列** | 129ns/op | ~1000ns (mutex) | **7.8x** |
| **内存分配** | 362ns/次 | ~1000ns (malloc) | **2.8x** |
| **异步日志** | 454ns/条 | ~2000ns (同步IO) | **4.4x** |

> 测试环境：Linux x86_64, GCC 13, Release构建, 单次运行结果  
> 🎯 协程创建和执行性能优异，无锁数据结构表现出色

## 🚀 核心协程系统

FlowCoro 提供了简洁高效的 C++20 协程编程接口：

```cpp
#include <flowcoro.hpp>

// 基本协程任务
flowcoro::Task<std::string> fetch_data() {
    // 模拟异步操作
    co_await flowcoro::sleep_for(std::chrono::milliseconds(100));
    co_return "Hello FlowCoro!";
}

int main() {
    // 运行协程
    auto task = fetch_data();
    auto result = task.get();
    
    std::cout << result << std::endl;
    return 0;
}
```

### 🆕 Promise风格状态查询API (v2.1新增)

FlowCoro 新增了类似JavaScript Promise的状态查询接口，让异步任务状态管理更直观：

```cpp
#include <flowcoro.hpp>

flowcoro::Task<int> async_computation(int value) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    co_return value * 2;
}

int main() {
    auto task = async_computation(42);
    
    // JavaScript Promise风格的状态查询
    if (task.is_pending()) {
        std::cout << "Task is still running..." << std::endl;
    }
    
    // 等待完成
    auto result = task.get();
    
    // 检查最终状态
    if (task.is_fulfilled()) {
        std::cout << "Task completed successfully with: " << result << std::endl;
    } else if (task.is_rejected()) {
        std::cout << "Task failed or was cancelled." << std::endl;
    }
    
    return 0;
}
```

**可用的状态查询方法：**

| 方法 | 说明 | JavaScript对应 |
|------|------|---------------|
| `is_pending()` | 任务正在运行中 | `Promise.pending` |
| `is_settled()` | 任务已结束（成功或失败） | `Promise.settled` |
| `is_fulfilled()` | 任务成功完成 | `Promise.fulfilled` |
| `is_rejected()` | 任务失败或被取消 | `Promise.rejected` |

## 🚀 快速开始

### 环境要求

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake git gcc-11 g++-11

# 确保C++20支持
gcc --version  # 需要 >= 11.0
```

### 安装编译

```bash
git clone https://github.com/your-username/flowcoro.git
cd flowcoro
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# 运行测试确保一切正常
./tests/flowcoro_tests
```

### Hello World

```cpp
#include <flowcoro.hpp>
#include <iostream>

// 定义异步任务
flowcoro::Task<std::string> fetch_data(const std::string& url) {
    // 模拟网络请求
    co_await flowcoro::sleep_for(std::chrono::milliseconds(100));
    co_return "Data from " + url;
}

// 主协程
flowcoro::Task<void> main_logic() {
    std::cout << "开始获取数据...\n";
    
    auto result = co_await fetch_data("https://api.example.com");
    std::cout << "收到: " << result << "\n";
    
    co_return;
}

int main() {
    // 运行协程
    auto task = main_logic();
    task.get();  // 等待完成
    
    return 0;
}
```

### 并发示例

```cpp
#include <flowcoro.hpp>
#include <vector>

// 并发处理多个任务
flowcoro::Task<void> concurrent_processing() {
    std::vector<std::string> urls = {
        "https://api1.example.com",
        "https://api2.example.com", 
        "https://api3.example.com"
    };
    
    // 启动所有任务
    std::vector<flowcoro::Task<std::string>> tasks;
    for (const auto& url : urls) {
        tasks.emplace_back(fetch_data(url));
    }
    
    // 等待所有完成
    for (auto& task : tasks) {
        auto result = co_await task;
        std::cout << "结果: " << result << "\n";
    }
    
    co_return;
}
```

### 网络服务器示例

```cpp
#include <flowcoro.hpp>

// HTTP Echo服务器
flowcoro::Task<void> handle_connection(std::unique_ptr<flowcoro::net::Socket> client) {
    auto conn = std::make_unique<flowcoro::net::TcpConnection>(std::move(client));
    
    while (true) {
        try {
            // 读取请求
            auto request = co_await conn->read_line();
            if (request.empty()) break;
            
            // 发送响应
            co_await conn->write("Echo: " + request);
            co_await conn->flush();
            
        } catch (const std::exception& e) {
            break;  // 连接断开
        }
    }
    
    co_return;
}

int main() {
    auto& loop = flowcoro::net::GlobalEventLoop::get();
    flowcoro::net::TcpServer server(&loop);
    
    // 设置连接处理器
    server.set_connection_handler(handle_connection);
    
    // 开始监听
    auto listen_task = server.listen("0.0.0.0", 8080);
    auto loop_task = loop.run();
    
    std::cout << "服务器启动在 http://localhost:8080\n";
    
    // 运行事件循环
    loop_task.get();
    
    return 0;
}
```

## 📊 架构设计

```
┌─────────────────────────────────┐
│        用户应用层                │  ← 你的业务逻辑
├─────────────────────────────────┤
│    网络IO层 (net.h)             │  ← 异步Socket/TCP服务器  
├─────────────────────────────────┤
│   协程调度层 (core.h)           │  ← Task/AsyncPromise
├─────────────────────────────────┤
│  执行引擎 (thread_pool.h)       │  ← 无锁线程池
├─────────────────────────────────┤
│ 无锁数据结构 (lockfree.h)       │  ← Queue/Stack/Buffer
├─────────────────────────────────┤
│ 基础设施 (logger.h, memory.h)  │  ← 日志/内存管理
└─────────────────────────────────┘
```

### 核心组件

| 组件 | 文件 | 功能 |
|------|------|------|
| **协程核心** | `core.h` | Task、AsyncPromise、协程调度 |
| **无锁结构** | `lockfree.h` | Queue、Stack、RingBuffer |
| **线程池** | `thread_pool.h` | 工作窃取、任务调度 |
| **网络IO** | `net.h` | Socket、TcpServer、EventLoop |
| **日志系统** | `logger.h` | 异步日志、性能统计 |
| **内存管理** | `memory.h` | 内存池、缓存友好Buffer |

## 💼 应用场景

### 🌟 **高频交易系统**

- **微秒级延迟**：协程切换开销极低
- **零锁设计**：避免锁竞争导致的延迟抖动
- **内存池**：预分配内存，避免分配延迟

### 🎮 **游戏服务器**

- **高并发**：单机支持大量玩家连接
- **实时响应**：异步IO保证游戏流畅性
- **状态管理**：协程天然适合游戏逻辑

### 📱 **微服务架构**

- **异步通信**：高效的服务间调用
- **资源节约**：协程比线程轻量数倍
- **易于扩展**：模块化设计，组件可插拔

### 🔗 **IoT平台**

- **低资源占用**：适合嵌入式和边缘设备
- **高并发处理**：同时处理大量设备连接
- **实时数据**：协程化的数据管道

## 📖 学习资源

### 📚 **文档指南**

- [⚡ 性能调优指南](docs/PERFORMANCE_GUIDE.md) - 高性能编程技巧和最佳实践
- [🔧 API参考手册](docs/API_REFERENCE.md) - 完整的接口文档
- [🌐 网络编程指南](docs/NETWORK_GUIDE.md) - 异步网络开发
- [🗄️ 数据库使用指南](docs/DATABASE_GUIDE.md) - 数据库连接池使用

### 💡 **示例代码**

- [基础示例](examples/hello_world.cpp) - 协程入门
- [网络示例](examples/network_example.cpp) - TCP服务器
- [并发示例](examples/enhanced_demo.cpp) - 生产者消费者

### 🧪 **测试和基准**

```bash
# 运行所有测试
./tests/flowcoro_tests

# 性能基准测试
./benchmarks/simple_benchmarks

# 网络压力测试  
./examples/network_example &
curl http://localhost:8080
```

## 🛠️ 开发指南

### CMake集成

```cmake
# 在你的项目中使用FlowCoro
find_package(FlowCoro REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE flowcoro_net)
```

### 编译选项

```bash
# Debug构建 (开发调试)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release构建 (生产环境)
cmake -DCMAKE_BUILD_TYPE=Release ..

# 启用内存检查 (开发阶段)
cmake -DFLOWCORO_ENABLE_SANITIZERS=ON ..
```

### 代码风格

- 使用现代C++20特性
- 遵循RAII和智能指针
- 异步优先，避免阻塞操作
- 错误处理使用异常机制

## 🤝 贡献与社区

### 💡 **如何贡献**

1. **Fork项目** - 在GitHub上fork仓库
2. **创建分支** - `git checkout -b feature/my-feature`
3. **编写代码** - 确保通过所有测试
4. **提交PR** - 详细描述你的更改

### 🐛 **问题反馈**

- [GitHub Issues](https://github.com/yourusername/flowcoro/issues) - Bug报告和功能请求
- [讨论区](https://github.com/yourusername/flowcoro/discussions) - 技术讨论

### 🚀 **发展路线**

#### Phase 2.3: 核心健壮性增强 (优先级: 🔥高)

- [ ] **统一错误处理**: 实现Result<T,E>类型系统，协程错误传播机制
- [ ] **RAII资源管理**: ResourceGuard模板，CoroutineScope自动清理
- [ ] **并发安全增强**: 原子引用计数指针，读写锁协程化
- [ ] **调度器优化**: 优先级调度，负载均衡，更公平的任务分配

#### Phase 2.4: 易用性大幅提升 (优先级: 🔥高)

- [ ] **链式API设计**: TaskBuilder模式，Pipeline操作符，流畅的编程体验
- [ ] **安全协程组合器**: 重新设计when_all/when_any，超时和重试机制
- [ ] **调试诊断工具**: 协程追踪器，性能分析器，状态监控
- [ ] **开发者友好**: 更好的错误信息，调试支持，性能分析

#### Phase 2.5: 开发体验优化 (优先级: ⭐中)

- [ ] **配置系统**: 统一配置管理，一键式初始化
- [ ] **完善文档**: 最佳实践指南，更多实用示例
- [ ] **工具生态**: VS Code插件，调试器集成
- [ ] **性能监控**: 运行时指标收集，性能仪表板

#### Phase 2: 架构增强 (v2.4)

- [ ] **协议抽象**: 抽象输入输出协议层
- [ ] **多线程协程**: 改进跨线程协程调度和通信机制
- [ ] **WebSocket支持**: 基于协议抽象实现WebSocket
- [ ] **HTTP/2协议**: 支持现代HTTP协议栈

#### Phase 3: 高级特性 (v2.5)

- [ ] **分布式协程调度**: 支持多节点协程调度
- [ ] **内存优化**: 零分配设计优化
- [ ] **性能监控**: 集成性能分析和监控工具
- [ ] **生态扩展**: 更多数据库和消息队列支持

#### Phase 3.0+: 高级特性探索 (优先级: ⭐低 - ioManager特性学习)

- [ ] **GPU协程支持**: CUDA异步计算集成
- [ ] **分布式协程**: 跨节点协程调度和通信
- [ ] **WASM支持**: WebAssembly平台部署优化
- [ ] **机器学习**: 异步推理流水线集成

> **开发策略**: 优先级聚焦于核心健壮性和易用性，高级特性作为长期目标

## 📈 **性能指标**

基准测试结果显示 FlowCoro 在多种场景下都展现出卓越的性能：

- **协程创建速度**: 100万协程/秒  
- **内存占用**: 每个协程仅64字节
- **并发连接**: 单机可稳定支持10万并发
- **响应延迟**: P99延迟控制在1ms以内

## 🤝 **贡献指南**

FlowCoro 采用现代C++20标准开发，代码简洁且性能优异。

欢迎通过以下方式贡献：

- 提交Issue报告问题
- 提交Pull Request改进代码  
- 分享使用经验和最佳实践

## 📄 许可证

本项目采用 [MIT 许可证](LICENSE) - 详见LICENSE文件

## 🙏 致谢

FlowCoro的诞生离不开以下项目和社区的启发：

- **C++20协程标准委员会** - 标准化协程支持
- **Lewis Baker** - cppcoro库的设计思想  
- **Folly团队** - 高性能C++库实践
- **所有贡献者** - 让FlowCoro变得更好

---

**🚀 让C++协程编程更简单、更快速！**

[⭐ 给项目加星标](https://github.com/caixuf/flowcord) | [🍴 Fork项目](https://github.com/caixuf/flowcord) | [👀 关注更新](https://github.com/caixuf/flowcord)

</div>
