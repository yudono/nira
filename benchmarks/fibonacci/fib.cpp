#include <iostream>
#include <chrono>

long long fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    long long result = fib(40);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "C++: " << duration.count() << " ms (Result: " << result << ")" << std::endl;
    return 0;
}
