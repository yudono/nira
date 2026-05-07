#include <iostream>
#include <vector>
#include <chrono>

int main() {
    std::vector<int> arr;
    for (int i = 0; i < 1000; i++) {
        arr.push_back(1000 - i);
    }

    auto start = std::chrono::high_resolution_clock::now();
    int n = arr.size();
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "C++: " << duration.count() << " ms (First: " << arr[0] << ", Last: " << arr[999] << ")" << std::endl;
    return 0;
}
