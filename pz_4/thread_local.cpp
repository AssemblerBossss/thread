#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// Объявляем переменную, уникальную для каждого потока
thread_local int thread_specific_counter = 0;
std::mutex mtx;

void increment_counter() {
    std::unique_lock<std::mutex> lock(mtx);
    std::cout << "Поток " << std::this_thread::get_id() << " начинает с " << thread_specific_counter << std::endl;
    thread_specific_counter++; // Каждый поток увеличивает свою копию
    std::cout << "Поток " << std::this_thread::get_id() << " закончил с " << thread_specific_counter << std::endl;
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back(increment_counter);
    }

    for (auto& t : threads) {
        t.join();
    }

    // В главном потоке тоже своя копия, которая осталась 0
    std::cout << "Главный поток: " << thread_specific_counter << std::endl; // Выведет 0

    return 0;
}
