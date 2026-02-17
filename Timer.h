#ifndef THREAD_TIMER_H
#define THREAD_TIMER_H

#include <chrono>
#include <string>
#include <iostream>

class Timer {
    std::string comment;
    std::chrono::time_point<std::chrono::high_resolution_clock> start;

public:
    explicit Timer(const std::string& comment) {
        this->comment = comment;
        this->start = std::chrono::high_resolution_clock::now();

        std::cout << "Start function: " << comment << std::endl;
    }

    ~Timer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto work_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "End function: " << comment << " - Time taken: "
                  << work_time.count() << " milliseconds" << std::endl;

    }

};


#endif //THREAD_TIMER_H