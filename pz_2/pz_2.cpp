#include <iostream>
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <functional>

using namespace std;


// функция, которая будет использоваться в вызываемом объекте
void func_dummy(int N)
{
    for (int i = 0; i < N; i++) {
        cout << "Thread 1 :: callable => function pointer\n";
    }
}

// Вызываемый объект
class thread_obj {
public:
    void operator()(int n) {
        for (int i = 0; i < n; i++)
            cout << "Thread 2 :: callable => function object\n";
    }
};

// ======================== parallel_accumulate =============================

template<typename Iterator, typename T>
struct accumulate_block {
    void operator()(Iterator first, Iterator last, T& result)
    {
        result = std::accumulate(first, last, result);
    }
};

template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init)
{
    unsigned long const length = std::distance(first, last);
    if (!length)
        return init;

    unsigned long const min_per_thread = 25;
    unsigned long const max_threads =
        (length + min_per_thread - 1) / min_per_thread;

    unsigned long const hardware_threads =
        std::thread::hardware_concurrency();

    unsigned long const num_threads =
        std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);

    unsigned long const block_size = length / num_threads;

    std::vector<T> results(num_threads);
    std::vector<std::thread> threads(num_threads - 1);

    Iterator block_start = first;
    for (unsigned long i = 0; i < (num_threads - 1); ++i)
    {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        threads[i] = std::thread(
            accumulate_block<Iterator, T>(),
            block_start, block_end, std::ref(results[i])
        );
        block_start = block_end;
    }

    accumulate_block<Iterator, T>()(block_start, last, results[num_threads - 1]);

    std::for_each(threads.begin(), threads.end(),
        std::mem_fn(&std::thread::join));

    return std::accumulate(results.begin(), results.end(), init);
}

// ======================== main() =============================

int main()
{
    cout << "=== Определение количества потоков (ядер) ===\n";
    unsigned int cores = std::thread::hardware_concurrency();
    cout << "Количество доступных аппаратных потоков: " << cores << "\n\n";

    cout << "=== Пример запуска потоков ===\n";

    // Лямбда-выражение
    auto f = [](int n) {
        for (int i = 0; i < n; i++)
            cout << "Thread 3 :: callable => lambda expression\n";
    };

    // запуск потоков
    thread th1(func_dummy, 2);
    thread th2(thread_obj(), 2);
    thread th3(f, 2);

    th1.join();
    th2.join();
    th3.join();

    cout << "\n=== Пример parallel_accumulate ===\n";

    vector<int> numbers(1000);
    iota(numbers.begin(), numbers.end(), 1);

    int result = parallel_accumulate(numbers.begin(), numbers.end(), 0);
    cout << "Сумма элементов (parallel_accumulate): " << result << "\n";

    cout << "\nПрограмма завершена.\n";
    return 0;
}
