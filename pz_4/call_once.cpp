#include <iostream>
#include <mutex>
#include <thread>

struct X {
    X() { std::cout << "X constructed by thread " << std::this_thread::get_id() << "\n"; }
};

X* instance = nullptr;
std::once_flag instance_flag;

void create_x() {
    instance = new X();
}

void thread_func() {
    std::call_once(instance_flag, create_x);
}

int main() {
    std::thread t1(thread_func);
    std::thread t2(thread_func);
    t1.join();
    t2.join();
    // cleanup for demo:
    delete instance;
    return 0;
}
