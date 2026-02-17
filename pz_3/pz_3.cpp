#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <mutex>
#include <stdexcept>


// Глобальный мутекс для синхронизации вывода
std::mutex cout_mutex;


class ThreadGuard {
private:
    std::thread &t;

public:
    explicit ThreadGuard(std::thread& _t) : t(_t) {}

    ~ThreadGuard() {
        if (t.joinable()) {
            t.join();
        }
    }

    ThreadGuard(ThreadGuard const&) = delete;
    ThreadGuard& operator=(ThreadGuard const&) = delete;

};


void function_with_exception() {
    try {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "\n[Исключение] Поток начал работу. ID: " << std::this_thread::get_id() << std::endl;

        std::vector<int> my_vector;
        int value = my_vector.at(10);
        std::cout << "Значение: " << value << std::endl;
    }
    catch (const std::exception &except) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[Исключение в потоке] Перехвачено: " << except.what() << std::endl;
    }
}

void demonstrate_exception_handling() {
    std::cout << "\n=== ЗАДАНИЕ 2: Обработка исключений в потоках ===" << std::endl;

    std::thread t(function_with_exception);
    ThreadGuard guard(t);

    std::cout << "ThreadGuard обеспечивает безопасное завершение потока даже при исключениях." << std::endl;
}


// ============== ЗАДАНИЕ 3: Получение идентификатора потока ==============
void thread_func_with_id() {
    // Метод 1: Получение ID изнутри потока
    std::thread::id this_id = std::this_thread::get_id();

    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "[ID внутри потока] std::this_thread::get_id(): " << this_id << std::endl;
}


void demonstrate_thread_ids() {
    std::cout << "\n=== ЗАДАНИЕ 3: Получение идентификатора потока ===" << std::endl;

    std::thread th(thread_func_with_id);

    // Метод 2: Получение ID из объекта thread
    std::thread::id th_id = th.get_id();
    std::cout << "[ID из объекта] th.get_id(): " << th_id << std::endl;

    th.join();

    // После join() поток не имеет идентификатора
    std::cout << "[После join] th.get_id(): " << th.get_id() << std::endl;

    // Метод 3: ID главного потока
    std::cout << "[Главный поток] std::this_thread::get_id(): "
              << std::this_thread::get_id() << std::endl;
}

void f(int i, const std::string& s) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "[Поток с параметрами] i=" << i << ", s=" << s << std::endl;
}

void demonstrate_thread_args() {
    std::cout << "\n=== ЗАДАНИЕ: Поток с аргументами ===" << std::endl;

    std::thread t(f, 42, "hello world");
    t.join();
}


void demonstrate_thread_move() {
    std::cout << "\n=== ЗАДАНИЕ: Передача владения потоком ===" << std::endl;

    std::thread t1([] {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[t1] Работаю..." << std::endl;
    });

    std::thread t2 = std::move(t1); // передача владения

    std::cout << "t1.joinable(): " << t1.joinable() << std::endl;
    std::cout << "t2.joinable(): " << t2.joinable() << std::endl;

    t2.join();
}

void demonstrate_detach() {
    std::cout << "\n=== ЗАДАНИЕ: detach() ===" << std::endl;

    std::thread t([] {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[detach] Фоновый поток! ID="
                  << std::this_thread::get_id() << std::endl;
    });

    t.detach();

    std::cout << "Поток отделён и работает фоном." << std::endl;
}

void demonstrate_sleep() {
    std::cout << "\n=== ЗАДАНИЕ: sleep_for, sleep_until, yield ===" << std::endl;

    std::cout << "Засыпаю на 1 секунду..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "yield() — уступаю управление..." << std::endl;
    std::this_thread::yield();

    auto wake_time = std::chrono::system_clock::now() + std::chrono::seconds(2);
    std::cout << "sleep_until (2 секунды)..." << std::endl;
    std::this_thread::sleep_until(wake_time);

    std::cout << "Проснулся!" << std::endl;
}


class Functor {
public:
    void operator()(int x) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[Функтор] x=" << x << std::endl;
    }
};

void demonstrate_lambda_and_functor() {
    std::cout << "\n=== Лямбда и функтор ===" << std::endl;

    std::thread lambda_thread([](int a, int b){
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[Лямбда] a+b=" << (a+b) << std::endl;
    }, 5, 7);

    std::thread functor_thread(Functor(), 99);

    lambda_thread.join();
    functor_thread.join();
}

int main() {
    std::cout << "=== Многопоточность C++: выполнение всех заданий ===\n";

    demonstrate_exception_handling();
    demonstrate_thread_ids();
    demonstrate_thread_args();
    demonstrate_thread_move();
    demonstrate_detach();
    demonstrate_sleep();
    demonstrate_lambda_and_functor();

    std::cout << "\n=== Все задания выполнены ===\n";
    return 0;
}
