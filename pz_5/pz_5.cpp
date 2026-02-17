#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <fstream>
#include <atomic>
#include <chrono>
#include <condition_variable>

std::mutex queueMutex;
std::queue<std::string> sharedQueue;
std::atomic<int> counter{0};

std::mutex fibMutex;                    // отдельный мьютекс для Fibonacci
std::condition_variable cv;
bool fibReady = false;
long long resultFib = 0;

// ======================= ПОТОКИ ЗАПИСИ В ОЧЕРЕДЬ ==================================

void writerThread(int id, long long& waitTime)
{
    using namespace std::chrono;

    for (int i = 0; i < 5; i++)
    {
        auto start = high_resolution_clock::now();

        std::unique_lock<std::mutex> lock(queueMutex);

        auto end = high_resolution_clock::now();
        waitTime += duration_cast<microseconds>(end - start).count();

        std::string text = "Thread " + std::to_string(id) +
                          " -> value " + std::to_string(counter++);
        sharedQueue.push(text);

        lock.unlock(); // освобождаем раньше для минимизации времени блокировки

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ======================== ПОТОК ЗАПИСИ ОЧЕРЕДИ В ФАЙЛ ============================

void saveToFile()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::ofstream file("output.txt");
    std::unique_lock<std::mutex> lock(queueMutex);

    while (!sharedQueue.empty())
    {
        file << sharedQueue.front() << "\n";
        sharedQueue.pop();
    }

    lock.unlock();
    file.close();

    std::cout << "Queue saved -> output.txt\n";
}

// ========================== FIBONACCI + condition_variable ========================

long long fibonacci(int n)
{
    if (n < 2) return n;

    long long a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        long long temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

void fibProducer()
{
    std::this_thread::sleep_for(std::chrono::seconds(1));

    long long result = fibonacci(20);

    {
        std::unique_lock<std::mutex> lock(fibMutex);
        resultFib = result;
        fibReady = true;
    }
    cv.notify_one();
}

void fibConsumer()
{
    std::unique_lock<std::mutex> lock(fibMutex);
    cv.wait(lock, [] { return fibReady; });

    std::cout << "Fibonacci(20) = " << resultFib << "\n";
}

// ==================================== MAIN ========================================

int main()
{
    long long wait1 = 0, wait2 = 0, wait3 = 0;

    std::thread t1(writerThread, 1, std::ref(wait1));
    std::thread t2(writerThread, 2, std::ref(wait2));
    std::thread t3(writerThread, 3, std::ref(wait3));
    std::thread saver(saveToFile);

    std::thread fibProd(fibProducer);
    std::thread fibCons(fibConsumer);

    t1.join();
    t2.join();
    t3.join();
    saver.join();

    fibProd.join();
    fibCons.join();

    std::cout << "\nВремя ожидания потоков:\n";
    std::cout << "Thread 1: " << wait1 << " мкс\n";
    std::cout << "Thread 2: " << wait2 << " мкс\n";
    std::cout << "Thread 3: " << wait3 << " мкс\n";

    return 0;
}