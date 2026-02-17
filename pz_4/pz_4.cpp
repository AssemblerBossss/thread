#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <future>
#include <optional>
#include <chrono>
#include <functional>

/////////////////////////////////////////////////////////////////
// -------------- ЗАДАЧА 1: Общий массив + mutex ---------------
/////////////////////////////////////////////////////////////////

void task1_run() {
    std::cout << "\n=== TASK 1: Shared vector + mutex ===\n";

    static std::vector<int> data;
    static std::mutex mtx;

    auto writer = [&]() {
        std::lock_guard<std::mutex> lock(mtx);
        data.push_back(42);
        std::cout << "[writer] pushed 42\n";
    };

    auto remover = [&]() {
        std::lock_guard<std::mutex> lock(mtx);
        if(!data.empty()) {
            int v = data.back();
            data.pop_back();
            std::cout << "[remover] removed " << v << "\n";
        } else {
            std::cout << "[remover] vector empty\n";
        }
    };

    std::thread t1(writer);
    std::thread t2(remover);
    t1.join();
    t2.join();

    std::cout << "Final vector size: " << data.size() << "\n";
}

/////////////////////////////////////////////////////////////////
// -------------- ЗАДАЧА 2: Передача по ссылке -----------------
/////////////////////////////////////////////////////////////////

void ref_func(int &a, int b) {
    a += b;
    std::cout << "[thread] a += b → " << a << "\n";
}

void task2_run() {
    std::cout << "\n=== TASK 2: std::ref demonstration ===\n";

    int value = 10;
    std::thread t(ref_func, std::ref(value), 5);
    t.join();

    std::cout << "After join: value = " << value << "\n"; // должно быть 15
}

/////////////////////////////////////////////////////////////////
// ------- ЗАДАЧА 3: future, promise, condition_variable -------
/////////////////////////////////////////////////////////////////

std::mutex cv_mtx;
std::condition_variable cv;
bool ready = false;
int shared_value = 0;

void producer_promise(std::promise<int> p) {
    try {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        p.set_value(777); // Отправляем значение
    } catch(...) {
        p.set_exception(std::current_exception());
    }
}

void waiter_thread() {
    std::unique_lock<std::mutex> lock(cv_mtx);
    cv.wait(lock, []{ return ready; });
    std::cout << "[waiter] received signal. shared_value = " << shared_value << "\n";
}

void notifier_thread() {
    {
        std::lock_guard<std::mutex> lock(cv_mtx);
        shared_value = 123;
        ready = true;
        std::cout << "[notifier] sending signal...\n";
    }
    cv.notify_one();
}

void task3_run() {
    std::cout << "\n=== TASK 3: future + condition_variable ===\n";

    // --- future/promise ---
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();
    std::thread p(producer_promise, std::move(prom));

    std::cout << "[main] waiting future...\n";
    int result = fut.get();
    std::cout << "[main] future value = " << result << "\n";
    p.join();

    // --- condition_variable ---
    std::thread w(waiter_thread);
    std::thread n(notifier_thread);
    w.join();
    n.join();
}




#define RUN_TASK_1
#define RUN_TASK_2
#define RUN_TASK_3
#define RUN_TASK_4

int main() {

#ifdef RUN_TASK_1
    task1_run();
#endif

#ifdef RUN_TASK_2
    task2_run();
#endif

#ifdef RUN_TASK_3
    task3_run();
#endif

    return 0;
}
