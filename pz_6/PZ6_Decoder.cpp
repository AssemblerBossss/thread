#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <string>
#include <algorithm>

using namespace std;

// Глобальные данные
vector<uint8_t> binaryData;
map<int, vector<string>> codeToCharLeft;  // Левый столбец кодов (может быть несколько символов)
map<int, vector<string>> codeToCharRight; // Правый столбец кодов (может быть несколько символов)
vector<int> matchedCodes;
string decodedText;
int bestModeGlobal = 0;  // Сохраняем выбранный режим для потока 4
bool bestUseLeftGlobal = false;  // Сохраняем выбранный столбец для потока 4

// Синхронизация
mutex mtx;
condition_variable cv_data, cv_match, cv_decode;
bool data_ready = false, match_done = false, decode_done = false, codes_ready = false;

// Вспомогательная функция: объединение вариантов символов через "/"
string joinVariants(const vector<string>& variants) {
    if (variants.empty()) return "?";
    if (variants.size() == 1) return variants[0];

    string result = "[";
    for (size_t i = 0; i < variants.size(); i++) {
        result += variants[i];
        if (i < variants.size() - 1) result += "/";
    }
    result += "]";
    return result;
}

// Вспомогательная функция: декодирование кода из выбранного столбца
string decodeSymbol(int code, bool useLeft) {
    auto& codeMap = useLeft ? codeToCharLeft : codeToCharRight;
    if (codeMap.count(code)) {
        return joinVariants(codeMap[code]);
    }
    return "?";
}

// Вспомогательная функция: добавление символа в таблицу кодов (без дубликатов)
void addSymbolToMap(map<int, vector<string>>& codeMap, int code, char symbol) {
    string sym(1, symbol);
    for (const string& s : codeMap[code]) {
        if (s == sym) return; // Уже есть
    }
    codeMap[code].push_back(sym);
}

// Поток 1: Чтение бинарного файла
void readBinaryFile(const string& filename) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) return;

    file.seekg(0, ios::end);
    size_t size = file.tellg();
    file.seekg(0, ios::beg);

    vector<uint8_t> buffer(size);
    file.read((char*)buffer.data(), size);
    file.close();

    lock_guard<mutex> lock(mtx);
    binaryData = buffer;
    data_ready = true;
    cv_data.notify_all();
}

// Поток 2: Чтение 7-битных кодов
void read7BitCodes(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) return;

    string line;
    while (getline(file, line)) {
        // Пропускаем заголовки и служебные строки
        if (line.empty() || line.find("коды") != string::npos ||
            line.find("ITA") != string::npos || line.find("CCIR") != string::npos ||
            line.find("Запрос") != string::npos || line.find("Idle") != string::npos) {
            continue;
        }

        // Разбиваем строку на токены (слова)
        vector<string> tokens;
        string token;
        for (char c : line) {
            if (c == ' ' || c == '\t') { // Если нашли пробел или табуляцию
                if (!token.empty()) { // И если накопили какое-то слово
                    tokens.push_back(token); // Сохраняем слово в список
                    token.clear(); // Очищаем буфер для следующего слова
                }
            } else { // Если это обычный символ
                token += c; // Добавляем символ к текущему слову
            }
        }
        // После цикла может остаться последнее слово, если строка не заканчивается пробелом
        if (!token.empty()) tokens.push_back(token);

        // Обрабатываем токены: ищем паттерн "номер код1 код2 символ"
        for (size_t i = 0; i < tokens.size(); ) {
            // Ищем номер (начинается с цифры)
            if (i >= tokens.size() || tokens[i].empty() || !(tokens[i][0] >= '0' && tokens[i][0] <= '9')) {
                i++;
                continue;
            }

            // Нашли номер, теперь ищем 2 кода по 7 бит и символ (всего 4 токена)
            if (i + 4 > tokens.size()) break;

            string code1 = tokens[i + 1];
            string code2 = tokens[i + 2];
            string symbolStr = tokens[i + 3];

            // Проверяем что это действительно 7-битные коды
            if (code1.length() == 7 && code2.length() == 7 &&
                code1.find_first_not_of("01") == string::npos && // проверка, что содержатся только 0 и 1, если не найдены дргуие, условие неверно
                code2.find_first_not_of("01") == string::npos) {

                // Берём первый ASCII символ
                char symbol = '?';
                for (char c : symbolStr) {
                    if ((unsigned char)c < 128) {
                        symbol = c;
                        break;
                    }
                }

                // Сохраняем в левый и правый столбцы отдельно
                // Если код уже существует, добавляем символ в список вариантов
                try {
                    int val1 = stoi(code1, nullptr, 2);
                    int val2 = stoi(code2, nullptr, 2);
                    //std::cout << "val1 and val2 "<< val1 << " " << val2 << std::endl;
                    addSymbolToMap(codeToCharLeft, val1, symbol);
                    addSymbolToMap(codeToCharRight, val2, symbol);
                } catch (...) {
                    // Игнорируем ошибки парсинга
                }

                i += 4; // Пропускаем обработанные токены
            } else {
                i++;
            }
        }
    }
    file.close();

    lock_guard<mutex> lock(mtx);
    codes_ready = true;
    cv_data.notify_all();
}

// Поток 3: Побитовое смещение и поиск совпадений
void findMatches() {
    unique_lock<mutex> lock(mtx);
    cv_data.wait(lock, [] { return data_ready && codes_ready; });

    // Тестируем 3 режима × 7 смещений × 2 столбца = 42 варианта
    string modeNames[] = {"", "Inverted", "Reversed", "Inv+Rev"};

    // Сохраняем все результаты для вывода в консоль
    vector<string> allResults;
    vector<vector<int>> allCodes;
    int bestMode = 0, bestOffset = 0;
    bool bestUseLeft = false;
    int bestSeq = 0;

    for (int mode = 1; mode <= 3; mode++) {
        // Применяем преобразования
        vector<uint8_t> transformedData;

        // Режим 1: Инверсия (побитовое НЕ)
        // Режим 2: Разворот (без инверсии)
        // Режим 3: Инверсия + Разворот (сначала инверсия, потом разворот)

        if (mode == 1 || mode == 3) {
            // Режимы 1, 3: Инверсия байтов
            for (uint8_t byte : binaryData) {
                transformedData.push_back(~byte);
            }
        } else {
            // Режим 2: Обычные байты
            transformedData = binaryData;
        }

        // Преобразуем в биты
        vector<bool> allBits;
        for (uint8_t byte : transformedData) {
            for (int i = 7; i >= 0; i--)
                allBits.push_back((byte >> i) & 1);
        }

        // Для режимов 2, 3: разворачиваем биты задом наперёд
        if (mode == 2 || mode == 3) {
            reverse(allBits.begin(), allBits.end());
        }

        // Пробуем все смещения 0-6 и оба столбца
        for (int offset = 0; offset < 7; offset++) {
         for (int colIdx = 0; colIdx < 2; colIdx++) {
            bool forceUseLeft = (colIdx == 0);
            // Ищем меандры RSRS на текущем смещении
            // Для режимов 2 и 3 (с разворотом) меандры в конце, не пропускаем
            size_t startPos = offset;

            // Только для режима 1 (Inverted без разворота) пропускаем меандры в начале
            if (mode == 1 && offset + 28 <= allBits.size()) {
                // Декодируем 4 кода по 7 бит
                vector<int> codes;
                for (int codeIdx = 0; codeIdx < 4; codeIdx++) {
                    int val = 0;
                    for (int i = 0; i < 7; i++) {
                        val = (val << 1) | (allBits[offset + codeIdx * 7 + i] ? 1 : 0);
                    }
                    codes.push_back(val);
                }

                // Проверяем, что это RSRS (меандры: 0101010=42 и 1010101=85)
                // R может быть 85 (1010101), S может быть 42 (0101010)
                bool isValidMeander = false;

                // Вариант 1: R=85, S=42, R=85, S=42
                if ((codes[0] == 85 || codes[0] == 42) &&
                    (codes[1] == 85 || codes[1] == 42) &&
                    (codes[2] == 85 || codes[2] == 42) &&
                    (codes[3] == 85 || codes[3] == 42)) {
                    // Дополнительно проверим, что они чередуются
                    //if (codes[0] != codes[1] && codes[0] == codes[2] && codes[1] == codes[3]) {
                        isValidMeander = true;
                    //}
                }

                // Если нашли меандры, пропускаем их
                if (isValidMeander) {
                    startPos = offset + 28;
                }
            }

            // Декодируем с текущего смещения, используя принудительно выбранный столбец
            string result = "";
            vector<int> currentCodes;
            int matched = 0, total = 0;

            // Идём по битовому потоку с шагом 7 бит начиная с startPos
            for (size_t pos = startPos; pos + 6 < allBits.size(); pos += 7) {
                total++;
                int val = 0;

                // Извлекаем 7 бит
                for (int i = 0; i < 7; i++) {
                    val = (val << 1) | (allBits[pos + i] ? 1 : 0);
                }

                currentCodes.push_back(val);

                // Декодируем символ из выбранного столбца
                string symbol = decodeSymbol(val, forceUseLeft);
                result += symbol;
                if (symbol != "?") matched++;
            }

            // Для режимов с разворотом (2, 3) переворачиваем результат обратно
            // но сохраняя правильный порядок символов внутри [.../...]
            if (mode == 2 || mode == 3) {
                // Переворачиваем строку, но сохраняя скобки
                string reversed = "";
                for (int i = result.length() - 1; i >= 0; i--) {
                    if (result[i] == ']') {
                        // Нашли закрывающую скобку, ищем открывающую
                        int openPos = i;
                        while (openPos >= 0 && result[openPos] != '[') {
                            openPos--;
                        }
                        if (openPos >= 0) {
                            // Копируем [.../...] как есть
                            for (int j = openPos; j <= i; j++) {
                                reversed += result[j];
                            }
                            i = openPos; // Пропускаем скопированную часть
                        } else {
                            reversed += result[i];
                        }
                    } else if (result[i] != '[') {
                        reversed += result[i];
                    }
                }
                result = reversed;
                reverse(currentCodes.begin(), currentCodes.end());
            }

            // Вычисляем максимальную последовательность совпадений
            // Игнорируем '[', ']', '/' при подсчете
            int maxSequence = 0, currentSequence = 0;
            for (char c : result) {
                if (c != '?' && c != '[' && c != ']' && c != '/') {
                    currentSequence++;
                    if (currentSequence > maxSequence) maxSequence = currentSequence;
                } else if (c == '?') {
                    currentSequence = 0;
                }
                // '[', ']', '/' не прерывают последовательность
            }

            // Сохраняем результат
            allResults.push_back(result);
            allCodes.push_back(currentCodes);

            // Обновляем лучший вариант
            if (maxSequence > bestSeq) {
                bestSeq = maxSequence;
                bestMode = mode;
                bestOffset = offset;
                bestUseLeft = forceUseLeft;
            }
          }  // конец цикла по столбцам
        }
    }

    // Выводим все 42 варианта в консоль
    cout << "\nAll 42 variants:\n";
    int idx = 0;
    for (int mode = 1; mode <= 3; mode++) {
        cout << "\n" << modeNames[mode] << ":\n";
        for (int offset = 0; offset < 7; offset++) {
            for (int col = 0; col < 2; col++) {
                cout << "  offset=" << offset << " ["
                     << (col == 0 ? "LEFT" : "RIGHT") << "]: "
                     << allResults[idx++] << "\n";
            }
        }
    }

    // Сохраняем коды лучшего варианта для потока 4
    idx = 0;
    for (int mode = 1; mode <= 3; mode++) {
        for (int offset = 0; offset < 7; offset++) {
            for (int col = 0; col < 2; col++) {
                if (mode == bestMode && offset == bestOffset &&
                    (col == 0) == bestUseLeft) {
                    matchedCodes = allCodes[idx];
                    decodedText = allResults[idx];
                    bestModeGlobal = mode;
                    bestUseLeftGlobal = bestUseLeft;
                }
                idx++;
            }
        }
    }

    cout << "\nBest variant: " << modeNames[bestMode] << ", offset=" << bestOffset
         << " [" << (bestUseLeft ? "LEFT" : "RIGHT") << "], maxSeq=" << bestSeq << "\n";

    match_done = true;
    cv_match.notify_one();
}

// Поток 4: Очистка меандров из лучшего результата
void decodeMatches() {
    unique_lock<mutex> lock(mtx);
    cv_match.wait(lock, [] { return match_done; });

    // СНАЧАЛА удаляем ВСЕ символы '?' из всей строки
    decodedText.erase(remove(decodedText.begin(), decodedText.end(), '?'), decodedText.end());

    // ПОТОМ убираем меандры с начала (чередующиеся R и S)
    while (decodedText.length() >= 2) {
        if ((decodedText[0] == 'R' && decodedText[1] == 'S') ||
            (decodedText[0] == 'S' && decodedText[1] == 'R') ||
            (decodedText[0] == 'R' && decodedText[1] == 'R') ||
            (decodedText[0] == 'S' && decodedText[1] == 'S')) {
            decodedText.erase(0, 1);
        } else {
            break;
        }
    }

    // Убираем меандры с конца (чередующиеся R и S)
    while (decodedText.length() >= 2) {
        size_t len = decodedText.length();
        if ((decodedText[len-1] == 'R' && decodedText[len-2] == 'S') ||
            (decodedText[len-1] == 'S' && decodedText[len-2] == 'R') ||
            (decodedText[len-1] == 'R' && decodedText[len-2] == 'R') ||
            (decodedText[len-1] == 'S' && decodedText[len-2] == 'S')) {
            decodedText.pop_back();
        } else {
            break;
        }
    }

    cout << "\nFinal decoded text: " << decodedText << "\n";

    decode_done = true;
    cv_decode.notify_one();
}

// Поток 5: Запись в файл
void writeOutput(const string& filename) {
    unique_lock<mutex> lock(mtx);
    cv_decode.wait(lock, [] { return decode_done; });

    ofstream file(filename);
    if (file.is_open()) {
        file << decodedText << endl;
        file.close();
    }
}

int main() {
    setlocale(LC_ALL, "ru");

    cout << "=== 7-bit Code Decoder ===" << endl;

    string inputFile = "codeRWT.dat";

    ofstream("output.txt", ios::trunc).close();

    thread t1(readBinaryFile, inputFile);
    thread t2(read7BitCodes, "7-bit-codes.txt");
    thread t3(findMatches);
    thread t4(decodeMatches);
    thread t5(writeOutput, "output.txt");

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();

    cout << "\nAll variants saved to output.txt" << endl;

    return 0;
}
