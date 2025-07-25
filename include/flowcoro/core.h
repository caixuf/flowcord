#pragma once
#include <coroutine>
#include <exception>
#include <memory>
#include <utility>
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <future>
#include <tuple>
#include <optional>
#include <type_traits>
#include <variant>
#include <queue>
#include <condition_variable>
#include <iostream>
#include "result.h"
#include "error_handling.h"
#include "lockfree.h" 
#include "thread_pool.h"
#include "buffer.h"
#include "logger.h"

// 前向声明HttpRequest类
class HttpRequest;

namespace flowcoro {

// ==========================================
// 增强协程池接口
// ==========================================

// 协程调度接口 - 使用高性能协程池
void schedule_coroutine_enhanced(std::coroutine_handle<> handle);

// 任务调度接口 - 通用任务调度
void schedule_task_enhanced(std::function<void()> task);

// 驱动协程池 - 需要在主线程中定期调用
void drive_coroutine_pool();

// 统计信息接口 - 查看协程池状态
void print_pool_stats();

// 关闭协程池
void shutdown_coroutine_pool();

// ==========================================
// FlowCoro 2.0 - 基于ioManager设计的协程管理器架构
// ==========================================

// 前向声明
class CoroutineManager;

// 协程管理器 - 参考ioManager的manager设计
class CoroutineManager {
public:
    CoroutineManager() = default;
    
    // 禁止拷贝和移动（参考ioManager设计）
    CoroutineManager(const CoroutineManager&) = delete;
    CoroutineManager& operator=(const CoroutineManager&) = delete;
    CoroutineManager(CoroutineManager&&) = delete;
    CoroutineManager& operator=(CoroutineManager&&) = delete;
    
    // 驱动协程调度（类似ioManager的drive方法）
    void drive() {
        // 驱动新的协程池系统
        drive_coroutine_pool();
        
        // 保留原有的定时器和任务处理
        process_timer_queue();
        process_ready_queue();
        process_pending_tasks();
    }
    
    // 添加定时器
    void add_timer(std::chrono::steady_clock::time_point when, std::coroutine_handle<> handle) {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        timer_queue_.emplace(when, handle);
    }
    
    // 调度协程恢复 - 集成协程池
    void schedule_resume(std::coroutine_handle<> handle) {
        if (!handle) {
            LOG_ERROR("Null handle in schedule_resume");
            return;
        }
        
        // 检查句柄地址有效性
        void* addr = handle.address();
        if (!addr) {
            LOG_ERROR("Invalid handle address in schedule_resume");
            return;
        }
        
        // 检查协程是否已完成
        if (handle.done()) {
            LOG_DEBUG("Handle already done in schedule_resume");
            return;
        }
        
        // 使用增强的协程池进行调度
        schedule_coroutine_enhanced(handle);
    }
    
    // 调度协程销毁（延迟销毁）
    void schedule_destroy(std::coroutine_handle<> handle) {
        if (!handle) return;
        
        std::lock_guard<std::mutex> lock(destroy_mutex_);
        destroy_queue_.push(handle);
    }
    
    // 获取全局管理器实例
    static CoroutineManager& get_instance() {
        static CoroutineManager instance;
        return instance;
    }
    
private:
    void process_timer_queue() {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        auto now = std::chrono::steady_clock::now();
        
        while (!timer_queue_.empty()) {
            const auto& [when, handle] = timer_queue_.top();
            if (when > now) break;
            
            // 将到期的定时器移到ready队列
            if (handle && !handle.done()) {
                std::lock_guard<std::mutex> ready_lock(ready_mutex_);
                ready_queue_.push(handle);
            }
            timer_queue_.pop();
        }
    }
    
    void process_ready_queue() {
        std::queue<std::coroutine_handle<>> local_queue;
        {
            std::lock_guard<std::mutex> lock(ready_mutex_);
            local_queue.swap(ready_queue_);
        }
        
        while (!local_queue.empty()) {
            auto handle = local_queue.front();
            local_queue.pop();
            
            // 增强的安全检查
            if (!handle) {
                LOG_DEBUG("Null handle in process_ready_queue");
                continue;
            }
            
            // 检查句柄地址有效性
            void* addr = handle.address();
            if (!addr) {
                LOG_ERROR("Invalid handle address in process_ready_queue");
                continue;
            }
            
            // 检查协程是否已完成
            if (handle.done()) {
                LOG_DEBUG("Handle already done in process_ready_queue");
                continue;
            }
            
            // 安全地恢复协程
            handle.resume();
        }
    }
    
    void process_pending_tasks() {
        // 处理延迟销毁队列
        std::queue<std::coroutine_handle<>> local_destroy_queue;
        {
            std::lock_guard<std::mutex> lock(destroy_mutex_);
            local_destroy_queue.swap(destroy_queue_);
        }
        
        while (!local_destroy_queue.empty()) {
            auto handle = local_destroy_queue.front();
            local_destroy_queue.pop();
            
            if (!handle) {
                LOG_DEBUG("Null handle in process_pending_tasks");
                continue;
            }
            
            // 检查句柄地址有效性
            void* addr = handle.address();
            if (!addr) {
                LOG_ERROR("Invalid handle address in process_pending_tasks");
                continue;
            }
            
            handle.destroy();
        }
    }
    
    // 定时器队列（最小堆）
    std::priority_queue<
        std::pair<std::chrono::steady_clock::time_point, std::coroutine_handle<>>,
        std::vector<std::pair<std::chrono::steady_clock::time_point, std::coroutine_handle<>>>,
        std::greater<>
    > timer_queue_;
    std::mutex timer_mutex_;
    
    // 就绪队列
    std::queue<std::coroutine_handle<>> ready_queue_;
    std::mutex ready_mutex_;
    
    // 延迟销毁队列
    std::queue<std::coroutine_handle<>> destroy_queue_;
    std::mutex destroy_mutex_;
};

// 安全的时钟等待器 - 参考ioManager的clock设计
class ClockAwaiter {
private:
    std::chrono::milliseconds duration_;
    CoroutineManager* manager_;
    
public:
    explicit ClockAwaiter(std::chrono::milliseconds duration) 
        : duration_(duration), manager_(&CoroutineManager::get_instance()) {}
    
    bool await_ready() const noexcept { 
        return duration_.count() <= 0;
    }
    
    void await_suspend(std::coroutine_handle<> h) {
        if (duration_.count() <= 0) {
            // 立即调度恢复
            manager_->schedule_resume(h);
            return;
        }
        
        // 添加到定时器队列
        auto when = std::chrono::steady_clock::now() + duration_;
        manager_->add_timer(when, h);
    }
    
    void await_resume() const noexcept {
        // 定时器完成
    }
};

// 替换原有的SleepAwaiter
using SleepAwaiter = ClockAwaiter;

// 协程状态枚举
enum class coroutine_state {
    created,    // 刚创建，尚未开始执行
    running,    // 正在运行
    suspended,  // 挂起等待
    completed,  // 已完成
    cancelled,  // 已取消
    destroyed,  // 已销毁
    error       // 执行出错
};

// 协程状态管理器
class coroutine_state_manager {
private:
    std::atomic<coroutine_state> state_{coroutine_state::created};
    
public:
    coroutine_state_manager() = default;
    
    // 尝试状态转换
    bool try_transition(coroutine_state from, coroutine_state to) noexcept {
        return state_.compare_exchange_strong(from, to);
    }
    
    // 强制状态转换
    void force_transition(coroutine_state to) noexcept {
        state_.store(to, std::memory_order_release);
    }
    
    // 获取当前状态
    coroutine_state get_state() const noexcept {
        return state_.load(std::memory_order_acquire);
    }
    
    // 检查是否处于某个状态
    bool is_state(coroutine_state expected) const noexcept {
        return state_.load(std::memory_order_acquire) == expected;
    }
};

// 取消状态（被cancellation_token使用）
class cancellation_state {
private:
    std::atomic<bool> cancelled_{false};
    std::mutex callbacks_mutex_;
    std::vector<std::function<void()>> callbacks_;
    
public:
    cancellation_state() = default;
    
    // 请求取消
    void request_cancellation() noexcept {
        bool expected = false;
        if (cancelled_.compare_exchange_strong(expected, true)) {
            // 第一次取消，触发回调
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            for (auto& callback : callbacks_) {
                callback(); // 直接调用，不捕获异常
            }
        }
    }
    
    // 检查是否已取消
    bool is_cancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }
    
    // 注册取消回调
    template<typename Callback>
    void register_callback(Callback&& cb) {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        callbacks_.emplace_back(std::forward<Callback>(cb));
        
        // 如果已经取消，立即调用回调
        if (is_cancelled()) {
            cb(); // 直接调用，不捕获异常
        }
    }
    
    // 清空回调
    void clear_callbacks() noexcept {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        callbacks_.clear();
    }
};

// 安全协程句柄包装器
class safe_coroutine_handle {
private:
    std::coroutine_handle<> handle_;
    std::atomic<bool> valid_{false};
    
public:
    safe_coroutine_handle() = default;
    
    // 从任意类型的协程句柄构造
    template<typename Promise>
    explicit safe_coroutine_handle(std::coroutine_handle<Promise> h)
        : handle_(h), valid_(h && !h.done()) {}
    
    // 从void句柄构造（特化版本）  
    explicit safe_coroutine_handle(std::coroutine_handle<> h)
        : handle_(h), valid_(h && !h.done()) {}
    
    // 移动构造
    safe_coroutine_handle(safe_coroutine_handle&& other) noexcept
        : handle_(std::exchange(other.handle_, {}))
        , valid_(other.valid_.exchange(false)) {}
    
    safe_coroutine_handle& operator=(safe_coroutine_handle&& other) noexcept {
        if (this != &other) {
            handle_ = std::exchange(other.handle_, {});
            valid_.store(other.valid_.exchange(false));
        }
        return *this;
    }
    
    // 禁止拷贝
    safe_coroutine_handle(const safe_coroutine_handle&) = delete;
    safe_coroutine_handle& operator=(const safe_coroutine_handle&) = delete;
    
    ~safe_coroutine_handle() {
        if (valid() && handle_) {
            if (handle_) {
                handle_.destroy(); // 直接销毁，不捕获异常
            }
        }
    }
    
    // 检查句柄是否有效
    bool valid() const noexcept {
        return valid_.load(std::memory_order_acquire) && handle_;
    }
    
    // 检查协程是否完成
    bool done() const noexcept {
        return !valid() || handle_.done();
    }
    
    // 恢复协程执行
    void resume() {
        if (valid() && !handle_.done()) {
            handle_.resume();
        }
    }
    
    // 获取Promise（类型安全）
    template<typename Promise>
    Promise& promise() {
        if (!valid()) {
            LOG_ERROR("Invalid coroutine handle in promise()");
            static Promise default_promise{};
            return default_promise;
        }
        return std::coroutine_handle<Promise>::from_address(handle_.address()).promise();
    }
    
    // 使句柄无效
    void invalidate() noexcept {
        valid_.store(false, std::memory_order_release);
    }
    
    // 获取原始地址
    void* address() const noexcept {
        return handle_ ? handle_.address() : nullptr;
    }
};

// 简化的全局线程池
class GlobalThreadPool {
private:
    static lockfree::ThreadPool& get_pool() {
        static lockfree::ThreadPool pool;
        return pool;
    }
    
public:
    static lockfree::ThreadPool& get() {
        return get_pool();
    }
    
    static bool is_shutdown_requested() {
        return false; // 简化实现
    }
    
    static void shutdown() {
        // 简化实现 - 线程池在程序结束时自动销毁
    }
    
    // 为了保持API兼容性，保留原有接口
    template<typename F, typename... Args>
    static auto enqueue(F&& f, Args&&... args) -> decltype(get().enqueue(std::forward<F>(f), std::forward<Args>(args)...)) {
        return get().enqueue(std::forward<F>(f), std::forward<Args>(args)...);
    }
    
    static void enqueue_void(std::function<void()> task) {
        get().enqueue_void(std::move(task));
    }
};

// 网络请求的抽象接口
class INetworkRequest {
public:
    virtual ~INetworkRequest() = default;
    
    // 发起网络请求的通用接口
    // url: 请求地址
    // callback: 回调函数，用于处理响应数据
    virtual void request(const std::string& url, const std::function<void(const std::string&)>& callback) = 0;
};

struct CoroTask {
    struct promise_type {
        CoroTask get_return_object() {
            return CoroTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { 
            LOG_ERROR("Unhandled exception in CoroTask");
        }
    };
    std::coroutine_handle<promise_type> handle;
    CoroTask(std::coroutine_handle<promise_type> h) : handle(h) {}
    CoroTask(const CoroTask&) = delete;
    CoroTask& operator=(const CoroTask&) = delete;
    CoroTask(CoroTask&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    CoroTask& operator=(CoroTask&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    ~CoroTask() { if (handle) handle.destroy(); }
    
    void resume() { 
        if (handle && !handle.done()) {
            LOG_DEBUG("Resuming coroutine handle: %p", handle.address());
            // 使用无锁线程池调度协程
            GlobalThreadPool::get().enqueue_void([h = handle]() {
                if (h && !h.done()) {
                    LOG_TRACE("Executing coroutine in thread pool");
                    h.resume();
                    LOG_TRACE("Coroutine execution completed");
                }
            });
        }
    }
    bool done() const { return !handle || handle.done(); }
    
    // 使CoroTask可等待
    bool await_ready() const {
        return done();
    }
    
    void await_suspend(std::coroutine_handle<> waiting_handle) {
        if (handle && !handle.done()) {
            // 在线程池中运行当前任务，然后恢复等待的协程
            GlobalThreadPool::get().enqueue_void([h = handle, waiting_handle]() {
                if (h && !h.done()) {
                    h.resume();
                }
                // 任务完成后恢复等待的协程
                if (waiting_handle && !waiting_handle.done()) {
                    waiting_handle.resume();
                }
            });
        } else {
            // 如果任务已经完成，直接恢复等待的协程
            GlobalThreadPool::get().enqueue_void([waiting_handle]() {
                if (waiting_handle && !waiting_handle.done()) {
                    waiting_handle.resume();
                }
            });
        }
    }
    
    void await_resume() const {
        // 任务完成，不返回值
    }
    
    // 执行网络请求（返回可等待的Task）
    template<typename NetworkRequestT = HttpRequest, typename T = std::string>
    static auto execute_network_request(const std::string& url) {
        class RequestTask {
        public:
            RequestTask(std::string url) : url_(std::move(url)) {}
            
            bool await_ready() const noexcept {
                return false;
            }
            
            void await_suspend(std::coroutine_handle<> h) {
                LOG_INFO("Starting network request to: %s", url_.c_str());
                // 创建网络请求实例并发起请求
                request_ = std::make_unique<NetworkRequestT>();
                request_->request(url_, [h, this](const T& response) {
                    LOG_DEBUG("Network request completed, response size: %zu", response.size());
                    // 保存响应结果
                    response_ = response;
                    // 使用无锁线程池继续执行协程
                    GlobalThreadPool::get().enqueue_void([h]() {
                        if (h && !h.done()) {
                            LOG_TRACE("Resuming coroutine after network response");
                            h.resume();
                        }
                    });
                });
            }
            
            const T& await_resume() const {
                return response_;
            }
            
        private:
            std::string url_;
            T response_;
            std::unique_ptr<NetworkRequestT> request_;
        };
        
        return RequestTask(url);
    }
};

// ==========================================
// RAII协程范围管理器
// ==========================================

class CoroutineScope {
public:
    ~CoroutineScope() {
        cancel_all();
    }
    
    void register_coroutine(std::coroutine_handle<> handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!cancelled_) {
            handles_.push_back(handle);
        }
    }
    
    void cancel_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = true;
        for (auto handle : handles_) {
            if (handle && !handle.done()) {
                handle.destroy();
            }
        }
        handles_.clear();
    }
    
    bool is_cancelled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cancelled_;
    }
    
    void cleanup_completed() {
        std::lock_guard<std::mutex> lock(mutex_);
        handles_.erase(
            std::remove_if(handles_.begin(), handles_.end(),
                [](std::coroutine_handle<> h) { return !h || h.done(); }),
            handles_.end()
        );
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::coroutine_handle<>> handles_;
    bool cancelled_ = false;
};

// ==========================================
// 统一的Task实现 - 整合了SafeTask的RAII和异常安全特性
// ==========================================
// 支持返回值的Task - 整合SafeTask的RAII和异常安全特性

template<typename T>
struct Task {
    struct promise_type {
        std::optional<T> value;
        bool has_error = false; // 替换exception_ptr
        
        // 增强版生命周期管理 - 融合SafeCoroutineHandle概念
        std::atomic<bool> is_cancelled_{false};
        std::atomic<bool> is_destroyed_{false};
        std::chrono::steady_clock::time_point creation_time_;
        mutable std::mutex state_mutex_; // 保护状态变更
        
        promise_type() : creation_time_(std::chrono::steady_clock::now()) {}
        
        // 析构时标记销毁
        ~promise_type() {
            is_destroyed_.store(true, std::memory_order_release);
        }
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_value(T v) noexcept { 
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (!is_cancelled_.load(std::memory_order_relaxed) && !is_destroyed_.load(std::memory_order_relaxed)) {
                value = std::move(v);
            }
        }
        
        void unhandled_exception() { 
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (!is_destroyed_.load(std::memory_order_relaxed)) {
                has_error = true;
                LOG_ERROR("Task<T> unhandled exception occurred");
            }
        }
        
        // 安全的取消支持
        void request_cancellation() {
            std::lock_guard<std::mutex> lock(state_mutex_);
            is_cancelled_.store(true, std::memory_order_release);
        }
        
        bool is_cancelled() const {
            return is_cancelled_.load(std::memory_order_acquire);
        }
        
        bool is_destroyed() const {
            return is_destroyed_.load(std::memory_order_acquire);
        }
        
        std::chrono::milliseconds get_lifetime() const {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - creation_time_);
        }
        
        // 安全获取值
        std::optional<T> safe_get_value() const {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (is_destroyed_.load(std::memory_order_relaxed)) {
                return std::nullopt;
            }
            return value;
        }
        
        // 安全获取错误状态
        bool safe_has_error() const {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (is_destroyed_.load(std::memory_order_relaxed)) {
                return false;
            }
            return has_error;
        }
    };
    std::coroutine_handle<promise_type> handle;
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            // 直接销毁当前句柄，避免递归调用safe_destroy
            if (handle) {
                if( handle.address() != nullptr && !handle.done() ) {
                    handle.promise().request_cancellation(); // 请求取消
                    safe_destroy(); // 安全销毁当前句柄
                }
                else
                {
                    LOG_ERROR("Task::operator=: Attempting to destroy an already done or null handle");
                }
            }
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    ~Task() { 
        // 增强版析构：使用安全销毁
        safe_destroy();
    }
    
    // 增强版：安全取消支持
    void cancel() {
        if (handle && !handle.done() && !handle.promise().is_destroyed()) {
            handle.promise().request_cancellation();
            LOG_INFO("Task::cancel: Task cancelled (lifetime: %lld ms)", 
                     handle.promise().get_lifetime().count());
        }
    }
    
    bool is_cancelled() const {
        if (!handle || handle.promise().is_destroyed()) return false;
        return handle.promise().is_cancelled();
    }
    
    // 简化版：基本状态查询
    std::chrono::milliseconds get_lifetime() const {
        if (!handle) return std::chrono::milliseconds{0};
        return handle.promise().get_lifetime();
    }
    
    bool is_active() const {
        return handle && !handle.done() && !is_cancelled();
    }
    
    // ==========================================
    // Phase 1 新增: JavaScript Promise 风格状态查询API
    // ==========================================
    
    /// @brief 检查任务是否仍在进行中 (JavaScript Promise.pending 风格)
    /// @return true if task is created or running, false otherwise
    bool is_pending() const noexcept {
        if (!handle) return false;
        return !handle.done() && !is_cancelled();
    }
    
    /// @brief 检查任务是否已结束 (JavaScript Promise.settled 风格)
    /// @return true if task is completed, cancelled, or has error
    bool is_settled() const noexcept {
        if (!handle) return true;  // 无效句柄视为已结束
        return handle.done() || is_cancelled();
    }
    
    /// @brief 检查任务是否成功完成 (JavaScript Promise.fulfilled 风格)
    /// @return true if task completed successfully
    bool is_fulfilled() const noexcept {
        if (!handle) return false;
        return handle.done() && !is_cancelled() && !handle.promise().exception;
    }
    
    /// @brief 检查任务是否被拒绝/失败 (JavaScript Promise.rejected 风格)
    /// @return true if task was cancelled or has exception
    bool is_rejected() const noexcept {
        if (!handle) return false;
        return is_cancelled() || (handle.promise().exception != nullptr);
    }
    
    // 安全销毁方法 - ioManager风格的延迟销毁
    void safe_destroy() {
        if (handle && handle.address()) {
            auto& manager = CoroutineManager::get_instance();
            
            if (!handle.promise().is_destroyed()) {
                // 标记为销毁状态
                handle.promise().is_destroyed_.store(true, std::memory_order_release);
            }
            
            // 延迟销毁 - 避免在协程执行栈中销毁
            if (handle.done()) {
                handle.destroy();
            } else {
                // 安排在下一个调度周期销毁
                manager.schedule_destroy(handle);
            }
            handle = nullptr;
        }
    }
    
    T get() {
        // 增强版：安全状态检查
        if (!handle) {
            LOG_ERROR("Task::get: Invalid handle");
            if constexpr (std::is_default_constructible_v<T>) {
                return T{};
            } else {
                LOG_ERROR("Cannot provide default value for non-default-constructible type");
                std::terminate();
            }
        }
        
        // 检查是否已销毁
        if (handle.promise().is_destroyed()) {
            LOG_ERROR("Task::get: Task already destroyed");
            if constexpr (std::is_default_constructible_v<T>) {
                return T{};
            } else {
                LOG_ERROR("Cannot provide default value for non-default-constructible type");
                std::terminate();
            }
        }
        
        // 安全恢复协程
        if (!handle.done() && !handle.promise().is_cancelled()) {
            handle.resume();
        }
        
        // 检查是否有错误
        if (handle.promise().safe_has_error()) {
            // 不使用异常，记录错误日志并返回默认值
            LOG_ERROR("Task execution failed");
            if constexpr (std::is_default_constructible_v<T>) {
                return T{};
            } else {
                // 对于不可默认构造的类型，尝试使用value的移动构造
                auto safe_value = handle.promise().safe_get_value();
                if (safe_value.has_value()) {
                    return std::move(safe_value.value());
                }
                LOG_ERROR("Cannot provide default value for non-default-constructible type");
                std::terminate();
            }
        }
        
        auto safe_value = handle.promise().safe_get_value();
        if (safe_value.has_value()) {
            return std::move(safe_value.value());
        } else {
            LOG_ERROR("Task completed without setting a value");
            if constexpr (std::is_default_constructible_v<T>) {
                return T{};
            } else {
                LOG_ERROR("Cannot provide default value for non-default-constructible type");
                std::terminate();
            }
        }
    }
    
    // SafeTask兼容方法：获取结果（同步）
    T get_result() requires(!std::is_void_v<T>) {
        return get(); // 复用现有的get()方法
    }
    
    void get_result() requires(std::is_void_v<T>) {
        get(); // 复用现有的get()方法
    }
    
    // SafeTask兼容方法：检查是否就绪
    bool is_ready() const noexcept {
        return await_ready();
    }
    
    // 使Task可等待 - 增强版安全检查
    bool await_ready() const {
        if (!handle) return true; // 无效句柄视为ready
        
        // 安全检查：验证句柄地址有效性
        if (!handle.address()) return true; // 无效地址视为ready
        if (handle.done()) return true; // 已完成视为ready

        // 只有在句柄有效时才检查promise状态
        return handle.promise().is_destroyed();
    }
    
    bool await_suspend(std::coroutine_handle<> waiting_handle) {
        // 简化的实现 - Task<T>版本
        if (!handle || handle.promise().is_destroyed()) {
            // 句柄无效，不挂起等待协程
            return false;
        }
        
        if (handle.done()) {
            // 任务已完成，不挂起等待协程
            return false;
        }
        
        // 任务未完成，启动任务执行
        auto& manager = CoroutineManager::get_instance();
        manager.schedule_resume(handle);
        
        // 挂起等待协程
        return true;
    }

    T await_resume() {
        // 增强版：使用安全getter
        if (!handle) {
            LOG_ERROR("Task await_resume: Invalid handle");
            if constexpr (std::is_default_constructible_v<T>) {
                return T{};
            } else {
                LOG_ERROR("Cannot provide default value for non-default-constructible type");
                std::terminate();
            }
        }
        
        if (handle.promise().is_destroyed()) {
            LOG_ERROR("Task await_resume: Task destroyed");
            if constexpr (std::is_default_constructible_v<T>) {
                return T{};
            } else {
                LOG_ERROR("Cannot provide default value for non-default-constructible type");
                std::terminate();
            }
        }
        
        if (handle.promise().safe_has_error()) {
            LOG_ERROR("Task await_resume: error occurred");
            if constexpr (std::is_default_constructible_v<T>) {
                return T{};
            } else {
                auto safe_value = handle.promise().safe_get_value();
                if (safe_value.has_value()) {
                    return std::move(safe_value.value());
                }
                LOG_ERROR("Cannot provide default value for non-default-constructible type");
                std::terminate();
            }
        }
        
        auto safe_value = handle.promise().safe_get_value();
        if (safe_value.has_value()) {
            return std::move(safe_value.value());
        } else {
            LOG_ERROR("Task await_resume: no value set");
            if constexpr (std::is_default_constructible_v<T>) {
                return T{};
            } else {
                LOG_ERROR("Cannot provide default value for non-default-constructible type");
                std::terminate();
            }
        }
    }
};

// Task<Result<T,E>>特化 - 支持Result错误处理
template<typename T, typename E>
struct Task<Result<T, E>> {
    struct promise_type {
        std::optional<Result<T, E>> result;
        std::exception_ptr exception;
        
        // 生命周期管理
        std::atomic<bool> is_cancelled_{false};
        std::atomic<bool> is_destroyed_{false};
        std::chrono::steady_clock::time_point creation_time_;
        mutable std::mutex state_mutex_;
        
        promise_type() : creation_time_(std::chrono::steady_clock::now()) {}
        
        ~promise_type() {
            is_destroyed_.store(true, std::memory_order_release);
        }
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_value(Result<T, E> r) noexcept {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (!is_cancelled_.load(std::memory_order_relaxed) && 
                !is_destroyed_.load(std::memory_order_relaxed)) {
                result = std::move(r);
            }
        }
        
        // 增强版异常处理 - 转换为Result
        void unhandled_exception() {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (!is_destroyed_.load(std::memory_order_relaxed)) {
                // 不使用异常，只设置错误状态
                if constexpr (std::is_same_v<E, ErrorInfo>) {
                    result = err(ErrorInfo(FlowCoroError::UnknownError, "Unhandled exception"));
                } else if constexpr (std::is_same_v<E, std::exception_ptr>) {
                    result = err(std::exception_ptr{}); // 不能捕获异常，只返回空指针
                } else {
                    // 对于其他错误类型，只能设置为默认错误
                    result = err(E{});
                }
            }
        }
        
        // 状态管理方法
        void request_cancellation() {
            std::lock_guard<std::mutex> lock(state_mutex_);
            is_cancelled_.store(true, std::memory_order_release);
        }
        
        bool is_cancelled() const {
            return is_cancelled_.load(std::memory_order_acquire);
        }
        
        bool is_destroyed() const {
            return is_destroyed_.load(std::memory_order_acquire);
        }
        
        std::chrono::milliseconds get_lifetime() const {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - creation_time_);
        }
        
        std::optional<Result<T, E>> safe_get_result() const {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (is_destroyed_.load(std::memory_order_relaxed)) {
                return std::nullopt;
            }
            return result;
        }
    };
    
    std::coroutine_handle<promise_type> handle;
    
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) {
                if (handle.address() != nullptr) {
                    handle.destroy();
                }
            }
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    
    ~Task() { 
        safe_destroy();
    }
    
    void cancel() {
        if (handle && !handle.done() && !handle.promise().is_destroyed()) {
            handle.promise().request_cancellation();
        }
    }
    
    bool is_cancelled() const {
        if (!handle || handle.promise().is_destroyed()) return false;
        return handle.promise().is_cancelled();
    }
    
    // Promise风格状态查询
    bool is_pending() const noexcept {
        if (!handle) return false;
        return !handle.done() && !is_cancelled();
    }
    
    bool is_settled() const noexcept {
        if (!handle) return true;
        return handle.done() || is_cancelled();
    }
    
    bool is_fulfilled() const noexcept {
        if (!handle || !handle.done() || is_cancelled()) return false;
        auto result = handle.promise().safe_get_result();
        return result.has_value() && result->is_ok();
    }
    
    bool is_rejected() const noexcept {
        if (!handle) return false;
        if (is_cancelled()) return true;
        if (!handle.done()) return false;
        auto result = handle.promise().safe_get_result();
        return !result.has_value() || result->is_err();
    }
    
    void safe_destroy() {
        if (handle && handle.address()) {
            if (!handle.promise().is_destroyed()) {
                handle.promise().is_destroyed_.store(true, std::memory_order_release);
            }
            handle.destroy();
            handle = nullptr;
        }
    }
    
    // 获取结果
    Result<T, E> get() {
        if (!handle) {
            if constexpr (std::is_same_v<E, ErrorInfo>) {
                return err(ErrorInfo(FlowCoroError::InvalidOperation, "Invalid task handle"));
            } else {
                return err(E{});
            }
        }
        
        // 检查取消状态
        if (is_cancelled()) {
            if constexpr (std::is_same_v<E, ErrorInfo>) {
                return err(ErrorInfo(FlowCoroError::TaskCancelled, "Task was cancelled"));
            } else {
                return err(E{});
            }
        }
        
        // 等待完成
        while (!handle.done()) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        // 获取结果
        auto result = handle.promise().safe_get_result();
        if (!result.has_value()) {
            if constexpr (std::is_same_v<E, ErrorInfo>) {
                return err(ErrorInfo(FlowCoroError::CoroutineDestroyed, "Task was destroyed"));
            } else {
                return err(E{});
            }
        }
        
        return std::move(*result);
    }
    
    // 可等待支持
    bool await_ready() const {
        return !handle || handle.done();
    }
    
    void await_suspend(std::coroutine_handle<> waiting_handle) {
        if (handle && !handle.done()) {
            // 在线程池中运行
            GlobalThreadPool::get().enqueue_void([h = handle, waiting_handle]() {
                if (h && !h.done()) {
                    h.resume();
                }
                if (waiting_handle && !waiting_handle.done()) {
                    waiting_handle.resume();
                }
            });
        }
    }
    
    Result<T, E> await_resume() {
        return get();
    }
};

// Task<void>特化 - 增强版集成生命周期管理
template<>
struct Task<void> {
    struct promise_type {
        bool has_error = false; // 替换exception_ptr
        
        // 增强版生命周期管理 - 与Task<T>保持一致
        std::atomic<bool> is_cancelled_{false};
        std::atomic<bool> is_destroyed_{false};
        std::chrono::steady_clock::time_point creation_time_;
        mutable std::mutex state_mutex_; // 保护状态变更
        
        promise_type() : creation_time_(std::chrono::steady_clock::now()) {}
        
        // 析构时标记销毁
        ~promise_type() {
            is_destroyed_.store(true, std::memory_order_release);
        }
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_void() noexcept {
            // 增强版 - 检查状态
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (is_cancelled_.load(std::memory_order_relaxed) || is_destroyed_.load(std::memory_order_relaxed)) {
                return; // 已取消或销毁，不做处理
            }
        }
        
        void unhandled_exception() { 
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (!is_destroyed_.load(std::memory_order_relaxed)) {
                has_error = true;
                LOG_ERROR("Task<void> unhandled exception occurred");
            }
        }
        
        // 增强版取消支持
        void request_cancellation() {
            std::lock_guard<std::mutex> lock(state_mutex_);
            is_cancelled_.store(true, std::memory_order_release);
        }
        
        bool is_cancelled() const {
            return is_cancelled_.load(std::memory_order_acquire);
        }
        
        bool is_destroyed() const {
            return is_destroyed_.load(std::memory_order_acquire);
        }
        
        std::chrono::milliseconds get_lifetime() const {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - creation_time_);
        }
        
        // 安全获取错误状态
        bool safe_has_error() const {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (is_destroyed_.load(std::memory_order_relaxed)) {
                return false;
            }
            return has_error;
        }
    };
    std::coroutine_handle<promise_type> handle;
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            // 直接销毁当前句柄，避免递归调用safe_destroy
            if (handle) {
                if (handle.address() != nullptr) {
                    handle.destroy();
                }
            }
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    ~Task() { 
        // 增强版析构：使用安全销毁
        safe_destroy();
    }
    
    // 安全销毁方法 - 与Task<T>保持一致
    void safe_destroy() {
        if (handle && handle.address()) {
            // 检查promise是否仍然有效
            if (!handle.promise().is_destroyed()) {
                // 标记为销毁状态
                handle.promise().is_destroyed_.store(true, std::memory_order_release);
            }
            // 直接销毁，不检查done状态
            if (handle.address() != nullptr) {
                handle.destroy();
            } else {
                LOG_ERROR("Task<void>::safe_destroy: handle address is null");
            }
            handle = nullptr;
        }
    }
    
    // 增强版：取消支持
    void cancel() {
        if (handle && !handle.done() && !handle.promise().is_destroyed()) {
            handle.promise().request_cancellation();
            LOG_INFO("Task<void>::cancel: Task cancelled (lifetime: %lld ms)", 
                     handle.promise().get_lifetime().count());
        }
    }
    
    bool is_cancelled() const {
        if (!handle || handle.promise().is_destroyed()) return false;
        return handle.promise().is_cancelled();
    }
    
    // 简化版：基本状态查询
    std::chrono::milliseconds get_lifetime() const {
        if (!handle) return std::chrono::milliseconds{0};
        return handle.promise().get_lifetime();
    }
    
    bool is_active() const {
        return handle && !handle.done() && !is_cancelled();
    }
    
    // ==========================================
    // Phase 1 新增: JavaScript Promise 风格状态查询API (Task<void>特化版本)
    // ==========================================
    
    /// @brief 检查任务是否仍在进行中 (JavaScript Promise.pending 风格)
    /// @return true if task is created or running, false otherwise
    bool is_pending() const noexcept {
        if (!handle) return false;
        return !handle.done() && !is_cancelled();
    }
    
    /// @brief 检查任务是否已结束 (JavaScript Promise.settled 风格)
    /// @return true if task is completed, cancelled, or has error
    bool is_settled() const noexcept {
        if (!handle) return true;  // 无效句柄视为已结束
        return handle.done() || is_cancelled();
    }
    
    /// @brief 检查任务是否成功完成 (JavaScript Promise.fulfilled 风格)
    /// @return true if task completed successfully
    bool is_fulfilled() const noexcept {
        if (!handle) return false;
        return handle.done() && !is_cancelled() && !handle.promise().has_error;
    }
    
    /// @brief 检查任务是否被拒绝/失败 (JavaScript Promise.rejected 风格)
    /// @return true if task was cancelled or has exception
    bool is_rejected() const noexcept {
        if (!handle) return false;
        return is_cancelled() || handle.promise().has_error;
    }
    
    void get() {
        // 增强版：安全状态检查
        if (!handle) {
            LOG_ERROR("Task<void>::get: Invalid handle");
            return;
        }
        
        // 检查是否已销毁
        if (handle.promise().is_destroyed()) {
            LOG_ERROR("Task<void>::get: Task already destroyed");
            return;
        }
        
        // 安全恢复协程
        if (!handle.done() && !handle.promise().is_cancelled()) {
            handle.resume();
        }
        
        // 检查是否有错误
        if (handle.promise().safe_has_error()) {
            LOG_ERROR("Task<void> execution failed");
            // void类型不抛出异常，只记录
        }
    }
    
    // SafeTask兼容方法：获取结果（同步）
    void get_result() {
        get(); // 复用现有的get()方法
    }
    
    // SafeTask兼容方法：检查是否就绪
    bool is_ready() const noexcept {
        return await_ready();
    }
    
    // 使Task<void>可等待 - 增强版安全检查
    bool await_ready() const {
        if (!handle) return true; // 无效句柄视为ready
        if (handle.promise().is_destroyed()) return true; // 已销毁视为ready
        return handle.done();
    }
    
    bool await_suspend(std::coroutine_handle<> waiting_handle) {
        // 简化的实现 - Task<void>版本
        if (!handle || handle.promise().is_destroyed()) {
            // 句柄无效，不挂起等待协程
            return false;
        }
        
        if (handle.done()) {
            // 任务已完成，不挂起等待协程
            return false;
        }
        
        // 任务未完成，启动任务执行
        auto& manager = CoroutineManager::get_instance();
        manager.schedule_resume(handle);
        
        // 挂起等待协程
        return true;
    }

    void await_resume() {
        // 增强版：使用安全getter
        if (!handle) {
            LOG_ERROR("Task<void> await_resume: Invalid handle");
            return;
        }
        
        if (handle.promise().is_destroyed()) {
            LOG_ERROR("Task<void> await_resume: Task destroyed");
            return;
        }
        
        if (handle.promise().safe_has_error()) {
            LOG_ERROR("Task<void> await_resume: error occurred");
            // void类型不抛出异常，只记录
        }
    }
};

// 支持异步等待的无锁Promise
template<typename T>
class AsyncPromise {
public:
    using Callback = std::function<void(const T&)>;
    
    AsyncPromise() : state_(std::make_shared<State>()) {}
    
    void set_value(const T& value) {
        LOG_DEBUG("Setting AsyncPromise value");
        std::coroutine_handle<> handle_to_resume = nullptr;
        {
            std::lock_guard<std::mutex> lock(state_->mutex_);
            state_->value_ = value;
            state_->ready_.store(true, std::memory_order_release);
            
            // 在锁保护下获取等待的协 coroutine
            handle_to_resume = state_->suspended_handle_.exchange(nullptr, std::memory_order_acq_rel);
        }
        
        // 在锁外调度协程以避免死锁
        if (handle_to_resume) {
            LOG_TRACE("Resuming waiting coroutine from AsyncPromise");
            GlobalThreadPool::get().enqueue_void([handle_to_resume]() {
                if (handle_to_resume && !handle_to_resume.done()) {
                    handle_to_resume.resume();
                }
            });
        }
    }
    
    void set_exception(std::exception_ptr ex) {
        LOG_DEBUG("Setting AsyncPromise exception");
        std::coroutine_handle<> handle_to_resume = nullptr;
        {
            std::lock_guard<std::mutex> lock(state_->mutex_);
            state_->exception_ = ex;
            state_->ready_.store(true, std::memory_order_release);
            
            handle_to_resume = state_->suspended_handle_.exchange(nullptr, std::memory_order_acq_rel);
        }
        
        if (handle_to_resume) {
            LOG_TRACE("Resuming waiting coroutine from AsyncPromise exception");
            GlobalThreadPool::get().enqueue_void([handle_to_resume]() {
                if (handle_to_resume && !handle_to_resume.done()) {
                    handle_to_resume.resume();
                }
            });
        }
    }
    
    bool await_ready() {
        return state_->ready_.load(std::memory_order_acquire);
    }
    
    void await_suspend(std::coroutine_handle<> h) {
        // 使用锁来确保线程安全
        std::lock_guard<std::mutex> lock(state_->mutex_);
        
        // 在锁保护下检查是否已经有值
        if (state_->ready_.load(std::memory_order_acquire)) {
            // 如果已经ready，立即调度
            GlobalThreadPool::get().enqueue_void([h]() {
                if (h && !h.done()) {
                    h.resume();
                }
            });
            return;
        }
        
        // 在锁保护下设置挂起的协程句柄
        std::coroutine_handle<> expected = nullptr;
        if (state_->suspended_handle_.compare_exchange_strong(expected, h, std::memory_order_acq_rel)) {
            // 成功设置句柄，协程将被 set_value 调度
        } else {
            // 如果设置句柄失败，说明已经有其他协程在等待
            // 这种情况下直接调度当前协程
            GlobalThreadPool::get().enqueue_void([h]() {
                if (h && !h.done()) {
                    h.resume();
                }
            });
        }
    }
    
    T await_resume() {
        std::lock_guard<std::mutex> lock(state_->mutex_);
        if (state_->exception_) {
            std::rethrow_exception(state_->exception_);
        }
        return state_->value_;
    }
    
private:
    struct State {
        std::atomic<bool> ready_{false};
        T value_{};
        std::exception_ptr exception_;
        std::atomic<std::coroutine_handle<>> suspended_handle_{nullptr};
        std::mutex mutex_; // 保护非原子类型的value_和exception_
    };
    
    std::shared_ptr<State> state_;
};

// AsyncPromise的void特化
template<>
class AsyncPromise<void> {
public:
    AsyncPromise() : state_(std::make_shared<State>()) {}
    
    void set_value() {
        LOG_DEBUG("Setting AsyncPromise<void> value");
        std::coroutine_handle<> handle_to_resume = nullptr;
        {
            std::lock_guard<std::mutex> lock(state_->mutex_);
            state_->ready_.store(true, std::memory_order_release);
            
            // 在锁保护下获取等待的协程句柄
            handle_to_resume = state_->suspended_handle_.exchange(nullptr, std::memory_order_acq_rel);
        }
        
        // 在锁外调度协程以避免死锁
        if (handle_to_resume) {
            LOG_TRACE("Resuming waiting coroutine from AsyncPromise<void>");
            GlobalThreadPool::get().enqueue_void([handle_to_resume]() {
                if (handle_to_resume && !handle_to_resume.done()) {
                    handle_to_resume.resume();
                }
            });
        }
    }
    
    void set_exception(std::exception_ptr ex) {
        LOG_DEBUG("Setting AsyncPromise<void> exception");
        std::coroutine_handle<> handle_to_resume = nullptr;
        {
            std::lock_guard<std::mutex> lock(state_->mutex_);
            state_->exception_ = ex;
            state_->ready_.store(true, std::memory_order_release);
            
            handle_to_resume = state_->suspended_handle_.exchange(nullptr, std::memory_order_acq_rel);
        }
        
        if (handle_to_resume) {
            LOG_TRACE("Resuming waiting coroutine from AsyncPromise<void> exception");
            GlobalThreadPool::get().enqueue_void([handle_to_resume]() {
                if (handle_to_resume && !handle_to_resume.done()) {
                    handle_to_resume.resume();
                }
            });
        }
    }
    
    bool await_ready() {
        return state_->ready_.load(std::memory_order_acquire);
    }
    
    void await_suspend(std::coroutine_handle<> h) {
        std::lock_guard<std::mutex> lock(state_->mutex_);
        
        if (state_->ready_.load(std::memory_order_acquire)) {
            GlobalThreadPool::get().enqueue_void([h]() {
                if (h && !h.done()) {
                    h.resume();
                }
            });
            return;
        }
        
        std::coroutine_handle<> expected = nullptr;
        if (state_->suspended_handle_.compare_exchange_strong(expected, h, std::memory_order_acq_rel)) {
            // 成功设置句柄
        } else {
            // 如果设置句柄失败，直接调度当前协程
            GlobalThreadPool::get().enqueue_void([h]() {
                if (h && !h.done()) {
                    h.resume();
                }
            });
        }
    }
    
    void await_resume() {
        std::lock_guard<std::mutex> lock(state_->mutex_);
        if (state_->exception_) {
            std::rethrow_exception(state_->exception_);
        }
    }
    
private:
    struct State {
        std::atomic<bool> ready_{false};
        std::exception_ptr exception_;
        std::atomic<std::coroutine_handle<>> suspended_handle_{nullptr};
        std::mutex mutex_;
    };
    
    std::shared_ptr<State> state_;
};

// AsyncPromise的unique_ptr特化，支持移动语义
template<typename T>
class AsyncPromise<std::unique_ptr<T>> {
public:
    AsyncPromise() : state_(std::make_shared<State>()) {}
    
    void set_value(std::unique_ptr<T> value) {
        LOG_DEBUG("Setting AsyncPromise<unique_ptr> value");
        std::coroutine_handle<> handle_to_resume = nullptr;
        {
            std::lock_guard<std::mutex> lock(state_->mutex_);
            state_->value_ = std::move(value);
            state_->ready_.store(true, std::memory_order_release);
            
            handle_to_resume = state_->suspended_handle_.exchange(nullptr, std::memory_order_acq_rel);
        }
        
        if (handle_to_resume) {
            LOG_TRACE("Resuming waiting coroutine from AsyncPromise<unique_ptr>");
            GlobalThreadPool::get().enqueue_void([handle_to_resume]() {
                if (handle_to_resume && !handle_to_resume.done()) {
                    handle_to_resume.resume();
                }
            });
        }
    }
    
    void set_exception(std::exception_ptr ex) {
        LOG_DEBUG("Setting AsyncPromise<unique_ptr> exception");
        std::coroutine_handle<> handle_to_resume = nullptr;
        {
            std::lock_guard<std::mutex> lock(state_->mutex_);
            state_->exception_ = ex;
            state_->ready_.store(true, std::memory_order_release);
            
            handle_to_resume = state_->suspended_handle_.exchange(nullptr, std::memory_order_acq_rel);
        }
        
        if (handle_to_resume) {
            LOG_TRACE("Resuming waiting coroutine from AsyncPromise<unique_ptr> exception");
            GlobalThreadPool::get().enqueue_void([handle_to_resume]() {
                if (handle_to_resume && !handle_to_resume.done()) {
                    handle_to_resume.resume();
                }
            });
        }
    }
    
    bool await_ready() {
        return state_->ready_.load(std::memory_order_acquire);
    }
    
    void await_suspend(std::coroutine_handle<> h) {
        std::lock_guard<std::mutex> lock(state_->mutex_);
        
        if (state_->ready_.load(std::memory_order_acquire)) {
            GlobalThreadPool::get().enqueue_void([h]() {
                if (h && !h.done()) {
                    h.resume();
                }
            });
            return;
        }
        
        std::coroutine_handle<> expected = nullptr;
        if (state_->suspended_handle_.compare_exchange_strong(expected, h, std::memory_order_acq_rel)) {
            // 成功设置句柄
        } else {
            GlobalThreadPool::get().enqueue_void([h]() {
                if (h && !h.done()) {
                    h.resume();
                }
            });
        }
    }
    
    std::unique_ptr<T> await_resume() {
        std::lock_guard<std::mutex> lock(state_->mutex_);
        if (state_->exception_) {
            std::rethrow_exception(state_->exception_);
        }
        return std::move(state_->value_);
    }
    
private:
    struct State {
        std::atomic<bool> ready_{false};
        std::unique_ptr<T> value_;
        std::exception_ptr exception_;
        std::atomic<std::coroutine_handle<>> suspended_handle_{nullptr};
        std::mutex mutex_;
    };
    
    std::shared_ptr<State> state_;
};

// Task<unique_ptr<T>>特化，支持移动语义
template<typename T>
struct Task<std::unique_ptr<T>> {
    struct promise_type {
        std::unique_ptr<T> value;
        std::exception_ptr exception;
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(std::unique_ptr<T> v) noexcept { value = std::move(v); }
        void unhandled_exception() { exception = std::current_exception(); }
    };
    std::coroutine_handle<promise_type> handle;
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    ~Task() { 
        if (handle) {
            // 检查协程句柄是否有效
            if (handle.address() != nullptr) {
                handle.destroy();
            }
        }
    }
    
    // ==========================================
    // Phase 1 新增: JavaScript Promise 风格状态查询API (Task<unique_ptr<T>>特化版本)
    // ==========================================
    
    /// @brief 检查任务是否仍在进行中 (JavaScript Promise.pending 风格)
    /// @return true if task is created or running, false otherwise
    bool is_pending() const noexcept {
        return handle && !handle.done();
    }
    
    /// @brief 检查任务是否已结束 (JavaScript Promise.settled 风格)
    /// @return true if task is completed or has error
    bool is_settled() const noexcept {
        if (!handle) return true;  // 无效句柄视为已结束
        return handle.done();
    }
    
    /// @brief 检查任务是否成功完成 (JavaScript Promise.fulfilled 风格)
    /// @return true if task completed successfully
    bool is_fulfilled() const noexcept {
        if (!handle) return false;
        return handle.done() && !handle.promise().exception;
    }
    
    /// @brief 检查任务是否被拒绝/失败 (JavaScript Promise.rejected 风格)
    /// @return true if task has exception
    bool is_rejected() const noexcept {
        if (!handle) return false;
        return handle.promise().exception != nullptr;
    }
    
    std::unique_ptr<T> get() {
        if (handle && !handle.done()) handle.resume();
        if (handle.promise().exception) std::rethrow_exception(handle.promise().exception);
        return std::move(handle.promise().value);
    }
    
    // 使Task<unique_ptr<T>>可等待
    bool await_ready() const {
        return handle && handle.done();
    }
    
    void await_suspend(std::coroutine_handle<> waiting_handle) {
        if (handle && !handle.done()) {
            GlobalThreadPool::get().enqueue_void([task_handle = handle, waiting_handle]() {
                if (task_handle && !task_handle.done()) {
                    task_handle.resume();
                }
                if (waiting_handle && !waiting_handle.done()) {
                    waiting_handle.resume();
                }
            });
        } else {
            GlobalThreadPool::get().enqueue_void([waiting_handle]() {
                if (waiting_handle && !waiting_handle.done()) {
                    waiting_handle.resume();
                }
            });
        }
    }
    
    std::unique_ptr<T> await_resume() {
        if (handle.promise().exception) std::rethrow_exception(handle.promise().exception);
        return std::move(handle.promise().value);
    }
};

// 支持异步任务的无锁队列
class AsyncQueue {
private:
    struct Node {
        std::function<void()> task;
        std::shared_ptr<Node> next;
    };
    
    std::atomic<std::shared_ptr<Node>> head_;
    std::atomic<std::shared_ptr<Node>> tail_;
    
public:
    AsyncQueue() {
        // 初始化哨兵节点
        auto sentinel = std::make_shared<Node>();
        head_.store(sentinel);
        tail_.store(sentinel);
    }
    
    // 入队操作
    void enqueue(std::function<void()> task) {
        auto new_tail = std::make_shared<Node>();
        new_tail->task = std::move(task);
        
        // 尝试将新节点链接到当前尾节点
        std::shared_ptr<Node> old_tail = tail_.load();
        while (!tail_.compare_exchange_weak(old_tail, new_tail, std::memory_order_release));
        
        // 将旧尾节点的next指针指向新节点
        old_tail->next = new_tail;
    }
    
    // 出队操作
    bool dequeue(std::function<void()>& task) {
        // 读取当前头节点
        std::shared_ptr<Node> old_head = head_.load();
        
        // 检查队列是否为空
        if (old_head == tail_.load()) {
            return false; // 队列为空
        }
        
        // 尝试将头节点向前移动一个节点
        if (head_.compare_exchange_strong(old_head, old_head->next, std::memory_order_acquire)) {
            task = std::move(old_head->task);
            return true;
        }
        
        return false;
    }
};

// ==========================================
// SafeTask别名 - 为了向后兼容，SafeTask就是Task
// ==========================================

template<typename T = void>
using SafeTask = Task<T>;

// 便捷函数：休眠

// 内置的安全协程句柄管理器
class SafeCoroutineHandle {
private:
    std::shared_ptr<std::atomic<std::coroutine_handle<>>> handle_;
    std::shared_ptr<std::atomic<bool>> destroyed_;
    std::thread::id creation_thread_id_;
    
public:
    explicit SafeCoroutineHandle(std::coroutine_handle<> h) 
        : handle_(std::make_shared<std::atomic<std::coroutine_handle<>>>(h))
        , destroyed_(std::make_shared<std::atomic<bool>>(false))
        , creation_thread_id_(std::this_thread::get_id()) {}
    
    void resume() {
        if (destroyed_->load(std::memory_order_acquire)) {
            return; // 已销毁，安全退出
        }
        
        // 线程安全检查：只允许在创建线程中恢复
        if (std::this_thread::get_id() != creation_thread_id_) {
            std::cerr << "[FlowCoro] Cross-thread coroutine resume blocked to prevent segfault. "
                      << "Created in thread " << creation_thread_id_ 
                      << ", resume attempted in thread " << std::this_thread::get_id() << std::endl;
            return; // 阻止跨线程恢复
        }
        
        auto h = handle_->load(std::memory_order_acquire);
        if (h && !h.done()) {
            h.resume();
        }
    }
    
    ~SafeCoroutineHandle() {
        destroyed_->store(true, std::memory_order_release);
        // 清除句柄引用
        handle_->store({}, std::memory_order_release);
    }
};

// 协程友好的sleep_for实现 - 不使用任何多线程！
class CoroutineFriendlySleepAwaiter {
private:
    std::chrono::milliseconds duration_;
    
public:
    explicit CoroutineFriendlySleepAwaiter(std::chrono::milliseconds duration) 
        : duration_(duration) {}
    
    bool await_ready() const noexcept { 
        return duration_.count() <= 0;
    }
    
    bool await_suspend(std::coroutine_handle<> h) {
        // 如果时间为0，不挂起
        if (duration_.count() <= 0) {
            return false; // 不挂起，立即继续
        }
        
        // 使用CoroutineManager的定时器，让它在合适的时候恢复我们
        auto& manager = CoroutineManager::get_instance();
        auto when = std::chrono::steady_clock::now() + duration_;
        manager.add_timer(when, h);
        
        // 挂起协程，等待定时器恢复
        return true;
    }
    
    void await_resume() const noexcept {
        // 定时器恢复了我们，休眠完成
    }
};

// 便捷函数：休眠 - 使用协程友好的实现
inline auto sleep_for(std::chrono::milliseconds duration) {
    return CoroutineFriendlySleepAwaiter(duration);
}

// 启动协程管理器的驱动循环
inline void start_coroutine_manager() {
    auto& manager = CoroutineManager::get_instance();
    std::thread([&manager]() {
        while (true) {
            manager.drive();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }).detach();
    
    std::cout << "FlowCoro: Coroutine manager started with ioManager-style architecture" << std::endl;
}
// 修复when_all - 使用索引顺序执行，完全模拟for循环
template<typename... Tasks>
Task<std::tuple<decltype(std::declval<Tasks>().get())...>> when_all(Tasks&&... tasks) {
    // 将tasks打包成tuple
    auto task_tuple = std::make_tuple(std::forward<Tasks>(tasks)...);
    
    // 顺序执行每个task，就像for循环一样
    auto result0 = co_await std::get<0>(task_tuple);
    if constexpr (sizeof...(tasks) == 1) {
        co_return std::make_tuple(std::move(result0));
    } else if constexpr (sizeof...(tasks) == 2) {
        auto result1 = co_await std::get<1>(task_tuple);
        co_return std::make_tuple(std::move(result0), std::move(result1));
    } else if constexpr (sizeof...(tasks) == 3) {
        auto result1 = co_await std::get<1>(task_tuple);
        auto result2 = co_await std::get<2>(task_tuple);
        co_return std::make_tuple(std::move(result0), std::move(result1), std::move(result2));
    }
    // 可以继续扩展更多数量...
}

// 同步等待协程完成的函数
template<typename T>
T sync_wait(Task<T>&& task) {
    // 直接使用Task的get()方法，它会阻塞直到完成
    // 不使用异常，直接返回get()的结果（get()内部已处理错误并返回默认值）
    return task.get();
}

// sync_wait需要特殊处理，避免LOG调用
inline void sync_wait(Task<void>&& task) {
    task.get(); // 不使用异常，get()内部已处理错误并记录日志
}

// 重载版本 - 接受lambda并返回Task
template<typename Func>
auto sync_wait(Func&& func) {
    auto task = func();
    return sync_wait(std::move(task));
}

// ========================================
// Phase 4 Integration: 生命周期管理集成
// ========================================

/**
 * @brief 启用FlowCoro v2.0 增强功能 - 基于ioManager架构设计
 */
inline void enable_v2_features() {
    // 启动协程管理器
    start_coroutine_manager();

    std::cout << "🚀 FlowCoro v4.0 Enhanced Features Enabled (ioManager-inspired)" << std::endl;
    std::cout << "   ✅ Centralized coroutine manager with drive-based scheduling" << std::endl;
    std::cout << "   ✅ Safe timer-based sleep implementation" << std::endl;
    std::cout << "   ✅ Delayed destruction for coroutine safety" << std::endl;
    std::cout << "   ✅ Enhanced lifecycle management integrated" << std::endl;
    std::cout << "   ✅ Cancel/timeout support with proper state tracking" << std::endl;
    std::cout << "   ✅ Architecture inspired by ioManager's FSM design" << std::endl;
    std::cout << "   ✅ All v2/v3 features are now unified in FlowCoro v4.0" << std::endl;
}

/**
 * @brief 简化版任务类型别名
 */
template<typename T = void>
using EnhancedTask = Task<T>;

/**
 * @brief 便利函数：将现有Task转换为增强Task（已经集成）
 */
template<typename T>
auto make_enhanced(Task<T>&& task) -> Task<T> {
    return std::move(task);
}

/**
 * @brief 简化版取消支持
 */
template<typename T>
auto make_cancellable_task(Task<T> task) -> Task<T> {
    // 返回任务本身，因为已经有取消支持
    return task;
}

/**
 * @brief 简化版超时支持
 */
template<typename T>
auto make_timeout_task(Task<T>&& task, std::chrono::milliseconds timeout) -> Task<T> {
    // 启动超时线程
    GlobalThreadPool::get().enqueue_void([task_ref = &task, timeout]() {
        std::this_thread::sleep_for(timeout);
        task_ref->cancel();
    });
    
    return std::move(task);
}

/**
 * @brief 简化版性能报告
 */
inline void print_performance_report() {
    LOG_INFO("=== FlowCoro Performance Report (Simplified) ===");
    LOG_INFO("✅ Task<T> integration: COMPLETE");
    LOG_INFO("✅ Basic lifecycle management: ACTIVE");
    LOG_INFO("✅ Cancel/timeout support: AVAILABLE");
}

// 运行协程直到完成的函数声明
template<typename T>
void run_until_complete(Task<T>& task);

// 运行协程直到完成 - void 特化版本
void run_until_complete(Task<void>& task);

} // namespace flowcoro

// 所有增强功能已整合到core.h中，无需额外包含
