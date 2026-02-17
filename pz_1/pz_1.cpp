#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <array>
#include <iomanip>
#include <memory>
#include "../Timer.h"

constexpr size_t BUF_SIZE = 64 * 1024;
constexpr uint32_t PATTERN_SIZE = 64 * 1024;
constexpr long long APPEND_SIZE = 120 * 1024 * 1024;
constexpr size_t SYMBOLS = 256;

std::array<unsigned long long, SYMBOLS> G_COUNTS{};


void hello_thread() {
    std::cout << "Да здравствует параллелизм!" << std::endl;

    long long int count = 0;
    for (long long int i = 0; i < 1'000'000; ++i) {
        count += i;
    }

    std::cout << "Результат сложения: " << count << std::endl;
}


std::vector<unsigned char> createPattern() {
    std::vector<unsigned char> pattern;
    std::array<unsigned char, 5> BYTES = {0x0A, 0x0D, 0x0B, 0x20, 0x22};

    pattern.reserve(PATTERN_SIZE);
    while (pattern.size() + BYTES.size() <= PATTERN_SIZE) {
        pattern.insert(pattern.end(), BYTES.begin(), BYTES.end());
    }
    pattern.insert(pattern.end(), BYTES.begin(), BYTES.begin() + (PATTERN_SIZE - pattern.size()));

    return pattern;
}

void writeFileThread_direct(std::string &file_path) {
    Timer timer("writeFileThread_direct");

    std::ofstream out(file_path, std::ios::binary | std::ios::app);
    if (!out) {
        std::cerr << "Не удалось открыть файл\n";
        return;
    }

    auto pattern = createPattern(); // Динамическое создание

    uint32_t total_written = 0;
    while (total_written < APPEND_SIZE) {
        uint32_t remaining = APPEND_SIZE - total_written;
        uint32_t write_size = std::min(static_cast<uint32_t>(pattern.size()), remaining);

        out.write(reinterpret_cast<const char*>(pattern.data()), write_size);
        total_written += write_size;
    }

    out.flush();
}

std::mutex g_merge_mutex;

void countFilePart(const std::string& file_path, size_t offset, size_t size) {
    std::array<uint64_t, SYMBOLS> local_counts = {0};
    std::vector<unsigned char> buf(BUF_SIZE); // Динамический буфер

    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open file: " << file_path << '\n';
        return;
    }

    in.seekg(offset);
    size_t remaining = size;

    while (remaining > 0 && in) {
        size_t to_read = std::min(remaining, buf.size());
        in.read(reinterpret_cast<char*>(buf.data()), to_read);
        std::streamsize got = in.gcount();
        if (got == 0) break;

        for (std::streamsize i = 0; i < got; ++i) {
            ++local_counts[buf[i]];
        }

        remaining -= got;
    }

    std::lock_guard<std::mutex> lock(g_merge_mutex);
    for (size_t i = 0; i < SYMBOLS; ++i) {
        G_COUNTS[i] += local_counts[i];
    }
}

void countFileMultithreaded(const std::string& file_path, unsigned int num_threads) {
    std::ifstream in(file_path, std::ios::binary | std::ios::ate);
    if (!in) {
        std::cerr << "Cannot open file: " << file_path << '\n';
        return;
    }

    size_t file_size = in.tellg();
    size_t chunk_size = (file_size + num_threads - 1) / num_threads;

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        size_t offset = i * chunk_size;
        if (offset >= file_size) break;

        size_t size = std::min(chunk_size, file_size - offset);
        threads.emplace_back(countFilePart, std::ref(file_path), offset, size);
    }

    for (auto& t : threads)
        t.join();
}


void printCounts() {
    std::cout << "\nСтатистика по байтам (непустые):\n";
    for (int i = 0; i < 256; ++i) {
        unsigned long long c = G_COUNTS[i];
        if (c == 0) continue;
        std::cout << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << i
             << std::nouppercase << std::dec << std::setfill(' ')
             << " : " << c << "\n";
    }
}


int main() {
    Timer timer("main");

    setlocale(LC_ALL, "ru");
    std::string file_path = "test.bin";
    unsigned int numThreads = std::thread::hardware_concurrency();
    std::cout << "Количество аппаратных потоков: " << numThreads << std::endl;

    std::thread t1(hello_thread);
    std::thread t2(writeFileThread_direct, std::ref(file_path));

    t1.join();
    t2.join();
   // t3.join();
    countFileMultithreaded(file_path, std::thread::hardware_concurrency());


    printCounts();
}








