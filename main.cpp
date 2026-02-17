#include <iostream>
#include <fstream>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <random>

// Константы
const unsigned char TARGET_BYTE1 = 0x0a;
const unsigned char TARGET_BYTE2 = 0x0d;
const unsigned char TARGET_BYTE3 = 0x20;
const char* FILENAME = "input.bin";
const size_t FILE_SIZE = 30 * 1024 * 1024;  // 30 МБ

// Общая очередь
std::queue<unsigned char> byte_queue;
std::mutex queue_mtx;

// Структура результатов
struct CountResult {
    int count_0a;
    int count_0d;
    int count_20;
    int count_group;
    size_t queue_size;
};

// Генерация тестового файла
void generate_test_file(const std::string& filename, size_t size_bytes) {
    std::cout << "Генерация файла " << filename
              << " размером " << (size_bytes / 1024.0 / 1024.0) << " МБ...\n";

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Ошибка создания файла\n";
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned char> dist(0, 255);

    const size_t BUFFER_SIZE = 1024 * 1024;
    std::vector<unsigned char> buffer(BUFFER_SIZE);

    size_t bytes_written = 0;
    while (bytes_written < size_bytes) {
        size_t chunk_size = std::min(BUFFER_SIZE, size_bytes - bytes_written);

        for (size_t i = 0; i < chunk_size; ++i) {
            buffer[i] = dist(gen);
        }

        file.write(reinterpret_cast<char*>(buffer.data()), chunk_size);
        bytes_written += chunk_size;
    }

    file.close();
    std::cout << "Файл создан успешно!\n\n";
}

// Функция обработки части файла (для packaged_task)
CountResult process_file_chunk(const std::string& filename, size_t start_pos, size_t end_pos) {
    CountResult result = {0, 0, 0, 0, 0};

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Ошибка открытия файла\n";
        return result;
    }

    file.seekg(start_pos);

    unsigned char prev_byte = 0;
    char byte;
    size_t current_pos = start_pos;

    while (file.get(byte) && current_pos < end_pos) {
        unsigned char ub = static_cast<unsigned char>(byte);

        if (ub == TARGET_BYTE1) {
            result.count_0a++;
            std::lock_guard<std::mutex> lock(queue_mtx);
            byte_queue.push(ub);
        }
        else if (ub == TARGET_BYTE2) {
            result.count_0d++;
            std::lock_guard<std::mutex> lock(queue_mtx);
            byte_queue.push(ub);
        }
        else if (ub == TARGET_BYTE3) {
            result.count_20++;
            std::lock_guard<std::mutex> lock(queue_mtx);
            byte_queue.push(ub);
        }

        // Проверка группы 0x0d0a
        if (ub == 0x0a && prev_byte == 0x0d) {
            result.count_group++;
        }

        prev_byte = ub;
        current_pos++;
    }

    file.close();

    {
        std::lock_guard<std::mutex> lock(queue_mtx);
        result.queue_size = byte_queue.size();
    }

    return result;
}

int main() {
    // Проверка/создание файла
    std::ifstream check_file(FILENAME);
    if (!check_file.good()) {
        check_file.close();
        generate_test_file(FILENAME, FILE_SIZE);
    } else {
        check_file.close();
        std::cout << "Файл существует, используем его\n\n";
    }

    // Временные метки начала
    auto start_time = std::chrono::steady_clock::now();
    auto start_timestamp = std::chrono::system_clock::now();
    auto start_time_t = std::chrono::system_clock::to_time_t(start_timestamp);

    std::cout << "=== НАЧАЛО РАБОТЫ ПРОГРАММЫ ===\n";
    std::cout << "Время начала: "
              << std::put_time(std::localtime(&start_time_t), "%Y-%m-%d %H:%M:%S")
              << std::endl;

    // Определяем размер файла
    std::ifstream size_file(FILENAME, std::ios::binary | std::ios::ate);
    if (!size_file.is_open()) {
        std::cerr << "Ошибка открытия файла\n";
        return 1;
    }

    size_t file_size = size_file.tellg();
    size_file.close();

    std::cout << "Размер файла: " << (file_size / 1024.0 / 1024.0)
              << " МБ (" << file_size << " байт)\n";

    size_t mid_point = file_size / 2;

    // ===== КЛЮЧЕВАЯ ЧАСТЬ: std::packaged_task =====

    // Создаём packaged_task для каждой задачи
    std::packaged_task<CountResult(const std::string&, size_t, size_t)> task1(process_file_chunk);
    std::packaged_task<CountResult(const std::string&, size_t, size_t)> task2(process_file_chunk);

    // Получаем future из packaged_task
    std::future<CountResult> future1 = task1.get_future();
    std::future<CountResult> future2 = task2.get_future();

    // Запускаем потоки с packaged_task (через std::move, т.к. packaged_task не копируется)
    std::cout << "Запуск обработки в 2 потока...\n";
    std::thread thread1(std::move(task1), FILENAME, 0, mid_point);
    std::thread thread2(std::move(task2), FILENAME, mid_point, file_size);

    // Ждём завершения потоков
    thread1.join();
    thread2.join();

    // Получаем результаты через future
    CountResult result1 = future1.get();
    CountResult result2 = future2.get();

    // Суммируем результаты
    int total_0a = result1.count_0a + result2.count_0a;
    int total_0d = result1.count_0d + result2.count_0d;
    int total_20 = result1.count_20 + result2.count_20;
    int total_group = result1.count_group + result2.count_group;

    // Временные метки окончания
    auto end_time = std::chrono::steady_clock::now();
    auto end_timestamp = std::chrono::system_clock::now();
    auto end_time_t = std::chrono::system_clock::to_time_t(end_timestamp);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "\n=== РЕЗУЛЬТАТЫ РАБОТЫ ПРОГРАММЫ ===\n";
    std::cout << "Время окончания: "
              << std::put_time(std::localtime(&end_time_t), "%Y-%m-%d %H:%M:%S")
              << std::endl;
    std::cout << "Время выполнения: " << duration.count() << " мс\n";
    std::cout << "Скорость обработки: "
              << (file_size / 1024.0 / 1024.0) / (duration.count() / 1000.0)
              << " МБ/с\n";

    std::cout << "\n--- КОЛИЧЕСТВЕННЫЕ ПОКАЗАТЕЛИ ---\n";
    std::cout << "Количество байт 0x0a (LF): " << total_0a << "\n";
    std::cout << "Количество байт 0x0d (CR): " << total_0d << "\n";
    std::cout << "Количество байт 0x20 (пробел): " << total_20 << "\n";
    std::cout << "Количество групп 0x0d0a (CRLF): " << total_group << "\n";

    {
        std::lock_guard<std::mutex> lock(queue_mtx);
        std::cout << "Размер очереди: " << byte_queue.size() << " элементов\n";
    }

    // Пояснения результатов
    std::cout << "\n--- ПОЯСНЕНИЕ РЕЗУЛЬТАТОВ ---\n";
    std::cout << "1. Файл обработан в 2 потока параллельно\n";
    std::cout << "2. Байты 0x0a, 0x0d, 0x20 записаны в общую очередь\n";
    std::cout << "3. Размер очереди = сумма найденных целевых байтов\n";
    std::cout << "4. Группы 0x0d0a - последовательности CR+LF (перевод строки Windows)\n";
    std::cout << "5. Для случайных данных ожидается ~117KB каждого байта (30MB/256≈117KB)\n";

    // Проверка корректности
    int expected_queue_size = total_0a + total_0d + total_20;
    {
        std::lock_guard<std::mutex> lock(queue_mtx);
        if (byte_queue.size() == expected_queue_size) {
            std::cout << "✓ Размер очереди совпадает с суммой найденных байтов\n";
        } else {
            std::cout << "✗ ОШИБКА: несоответствие размера очереди!\n";
        }
    }

    return 0;
}