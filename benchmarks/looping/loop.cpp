#include <iostream>
#include <chrono>

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    long long total = 0;
    for (int i = 0; i < 10000000; i++) {
        total += i;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "C++: " << duration.count() << " ms (Result: " << total << ")" << std::endl;
    return 0;
}
