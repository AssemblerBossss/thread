#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <vector>
#include <chrono>

constexpr int OPS_PER_THREAD = 500000;
constexpr int NUM_RUNS = 5;

struct ILock {
    virtual void lock() = 0;
    virtual void unlock() = 0;
    virtual const char* name() const = 0;
    virtual ~ILock() = default;
};

struct StdMutexLock : ILock {
    std::mutex m;
    void lock() override { m.lock(); }
    void unlock() override { m.unlock(); }
    const char* name() const override { return "std::mutex"; }
};

struct SpinlockFlag : ILock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

    void lock() override {
        // гарантирует, что все операции после этой точки "увидят" изменения, сделанные до release
        while (flag.test_and_set(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    // Атомарно сбрасывает флаг в false
    // Освобождает блокировку. memory_order_release гарантирует, что все операции до этой точки будут видны другим потокам, которые сделают acquire
    void unlock() override {
        flag.clear(std::memory_order_release);
    }

    const char* name() const override { return "atomic_flag spinlock"; }
};

struct SpinlockBool : ILock {
    std::atomic<bool> locked{ false };

    void lock() override {
        bool expected = false;
        // Если locked == expected (т.е. false) - устанавливает locked = true, возвращает true
        // Если locked != expected → записывает в expected текущее значение locked, возвращает false
        while (!locked.compare_exchange_weak(
            expected, true,
            std::memory_order_acquire)) {
            expected = false; // Потому что при неудачном compare_exchange, expected был перезаписан на true. Возвращаем его к false для следующей попытки
            std::this_thread::yield();
        }
    }

    //  Атомарно записывает false в locked
    void unlock() override {
        locked.store(false, std::memory_order_release);
    }

    const char* name() const override { return "atomic<bool> spinlock"; }
};

long long run_test(ILock& lock, unsigned int threads, std::atomic<bool>& start_flag) {
    std::queue<int> q;

    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&, thread_id = t] {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                lock.lock();
                q.push(thread_id * OPS_PER_THREAD + i);
                if (!q.empty()) {
                    q.pop();
                }
                lock.unlock();
            }
        });
    }

    auto start = std::chrono::high_resolution_clock::now();
    // RELEASE: гарантирует, что time_point записан в память  ПЕРЕД тем, как потоки увидят флаг
    start_flag.store(true, std::memory_order_release);

    for (auto& th : workers) {
        th.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

void benchmark_lock(ILock& lock, int unsigned threads) {
    std::cout << "Testing " << lock.name() << "...\n";

    for (int run = 0; run < NUM_RUNS; ++run) {
        std::atomic<bool> start_flag{ false };
        auto time = run_test(lock, threads, start_flag);
        std::cout << "  Run " << (run + 1) << ": " << time << " ms\n";
    }
    std::cout << "\n";
}

int main() {
    unsigned int threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 4;

    std::cout << "==============================================\n";
    std::cout << "   SYNCHRONIZATION MECHANISMS BENCHMARK\n";
    std::cout << "==============================================\n\n";
    std::cout << "Threads:         " << threads << "\n";
    std::cout << "Ops per thread:  " << OPS_PER_THREAD << "\n";
    std::cout << "Test runs:       " << NUM_RUNS << "\n\n";

    StdMutexLock mutex_lock;
    SpinlockFlag flag_lock;
    SpinlockBool bool_lock;

    benchmark_lock(mutex_lock, threads);
    benchmark_lock(flag_lock, threads);
    benchmark_lock(bool_lock, threads);

    return 0;
}