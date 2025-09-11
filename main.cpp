#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <atomic>
#include <algorithm>
#include <locale>
#include "Timer/Timer.h"
#include <array>
#include <thread>


// Структура для передачи параметров в потоки
typedef struct {
    const char* filename;
    std::atomic<ULONGLONG>* counts; // атомарные счетчики для каждого символа
    int threadId;
} ThreadParams;



BOOL CreateFileThread(ThreadParams& param) {
    std::string function_name = "CreateFileThread";

    Timer timer(function_name);

    printf("Поток создания файла запущен...\n");

    std::array<unsigned char, 5> symbols{0x0A, 0x0D, 0x0B, 0x20, 0x22};
    const DWORD symbolCount = sizeof(symbols) / sizeof(symbols[0]);

    const ULONGLONG targetSize = 120ULL * 1024ULL * 1024ULL;

    HANDLE hFile = CreateFileA(
            param.filename,
            GENERIC_WRITE | GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Create file error: %lu\n", GetLastError());
        return FALSE;
    }

    LARGE_INTEGER fileSize, zeroOffset;

    fileSize.QuadPart = targetSize;

    if (!SetFilePointerEx(hFile, fileSize, nullptr, FILE_BEGIN)) {
        printf("SetFilePointerEx error: %lu\n", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    if (!SetEndOfFile(hFile)) { // Устанавливает конец файла в текущую позицию указателя.
        printf("SetEndOfFile error: %lu\n", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    zeroOffset.QuadPart = 0;
    if (!SetFilePointerEx(hFile, zeroOffset, nullptr, FILE_BEGIN)) {
        printf("SetFilePointerEx error: %lu\n", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    HANDLE hMap = CreateFileMappingA(
            hFile,
            NULL,
            PAGE_READWRITE,
            fileSize.HighPart,
            fileSize.LowPart,
            NULL);

    if (hMap == NULL) {
        printf("Mapping file error: 123 %lu\n", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    LPVOID fileData = MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, 0);
    if (fileData == nullptr) {
        printf("CreateFileMapping error: %lu\n", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    auto *data = static_cast<unsigned char *>(fileData);

    for (ULONGLONG i = 0; i < targetSize; i++) {
        data[i] = symbols[i % symbolCount];
    }

    if (!FlushViewOfFile(fileData, 0)) {
        printf("FlushViewOfFile error: %lu\n", GetLastError());
    }

    UnmapViewOfFile(fileData);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return TRUE;
}

BOOL CountSymbolsInFile(ThreadParams& param) {
    std::string function_name = "CountSymbolsInFile";
    Timer timer(function_name);

    HANDLE hFile = CreateFileA(
            param.filename,
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Read file error: %lu\n", GetLastError());
    }

    LARGE_INTEGER fileSize;
    if(!GetFileSizeEx(hFile, &fileSize)) {
        std::cerr << "Error getting file size" << std::endl;
        CloseHandle(hFile);
        return FALSE;
    }

    HANDLE hMap = CreateFileMappingA(
            hFile,
            nullptr,
            PAGE_READONLY,
            fileSize.HighPart,
            fileSize.LowPart,
            nullptr);

    if (hMap == INVALID_HANDLE_VALUE) {
        printf("Mapping file error  %lu\n", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    auto* pData = static_cast<const UCHAR*>(
            MapViewOfFile(
                    hMap,
                    FILE_MAP_READ,
                    0, 0,
                    fileSize.QuadPart

            ));

    if (pData == nullptr) {
        printf("Mapping file error: %lu\n", GetLastError());
        CloseHandle(hMap);
        CloseHandle(hFile);
        return FALSE;
    }

    const ULONGLONG chunkSize = 10 * 1024 * 1024; // 10 МБ chunks
    const ULONGLONG totalSize = fileSize.QuadPart;

    for (ULONGLONG offset = 0; offset < totalSize; offset += chunkSize) {
        ULONGLONG chunkEnd = std::min(offset + chunkSize, totalSize);

        for (ULONGLONG i = offset; i < chunkEnd; i++) {
            switch (pData[i]) {
                case 0x0A: param.counts[0]++; break;
                case 0x0D: param.counts[1]++; break;
                case 0x0B: param.counts[2]++; break;
                case 0x20: param.counts[3]++; break;
                case 0x22: param.counts[4]++; break;
            }
        }

        //std::cout << "Processed: " << (chunkEnd / (1024 * 1024)) << " MB" << std::endl;
    }

    UnmapViewOfFile(pData);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return TRUE;
}




// Функция для вывода результатов
DWORD WINAPI OutputResultsThread(LPVOID param) {

    auto* p = static_cast<ThreadParams*>(param);

    // Ждем завершения подсчета символов
    Sleep(1000); // Небольшая задержка для демонстрации

    printf("\n=== РЕЗУЛЬТАТЫ ПОДСЧЕТА СИМВОЛОВ ===\n");
    printf("Символ 0x0A (LF):  %llu\n", p->counts[0].load());
    printf("Символ 0x0D (CR):  %llu\n", p->counts[1].load());
    printf("Символ 0x0B (VT):  %llu\n", p->counts[2].load());
    printf("Символ 0x20 (SP):  %llu\n", p->counts[3].load());
    printf("Символ 0x22 (\");  %llu\n", p->counts[4].load());

    ULONGLONG total = p->counts[0] + p->counts[1] + p->counts[2] + p->counts[3] + p->counts[4];
    printf("Общее количество символов: %llu\n", total);
    printf("Ожидаемый размер файла: 125829120 байт (120 МБ)\n");

    return 0;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    const char* filename = "test_file.bin";

    // Атомарные счетчики для каждого символа
    std::atomic<ULONGLONG> counts[5] = {0, 0, 0, 0, 0};

    // Параметры для потоков
    ThreadParams createParams = {filename, counts, 1};
    ThreadParams countParams = {filename, counts, 2};
    //ThreadParams outputParams = {filename, counts, 3};

    HANDLE threads[3];
    DWORD threadIDs[3];

    printf("=== ЗАПУСК ПРОГРАММЫ ===\n");

    // Создаем поток для создания файла
    std::thread t(CreateFileThread, std::ref(createParams));

//    if (threads[0] == NULL) {
//        printf("Ошибка создания потока для создания файла\n");
//        return 1;
//    }
    t.join();

    // Ждем завершения создания файла
//    WaitForSingleObject(threads[0], INFINITE);
//    CloseHandle(threads[0]);

    std::thread t2(CountSymbolsInFile, std::ref(countParams));
    t2.join();
    // Создаем поток для подсчета символов
//    threads[1] = CreateThread(NULL, 0, CountSymbolsThread, &countParams, 0, &threadIDs[1]);
//    if (threads[1] == NULL) {
//        printf("Ошибка создания потока для подсчета символов\n");
//        return 1;
//    }
//
//    // Создаем поток для вывода результатов
//    threads[2] = CreateThread(NULL, 0, OutputResultsThread, &outputParams, 0, &threadIDs[2]);
//    if (threads[2] == NULL) {
//        printf("Ошибка создания потока для вывода результатов\n");
//        CloseHandle(threads[1]);
//        return 1;
//    }
//
//    // Ждем завершения всех потоков
//    WaitForMultipleObjects(2, &threads[1], TRUE, INFINITE);
//
//    CloseHandle(threads[1]);
//    CloseHandle(threads[2]);

    // Удаляем временный файл
    //DeleteFileA(filename);

    return 0;
}