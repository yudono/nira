#include <iostream>
#include <chrono>

long fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    long result = fib(35);
    auto end = std::chrono::high_resolution_clock::now();
    
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "C++: " << (int)elapsed.count() << " ms (Result: " << result << ")" << std::endl;
    return 0;
}
