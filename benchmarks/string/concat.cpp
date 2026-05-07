#include <chrono>
#include <iostream>
#include <string>

int main() {
  auto start = std::chrono::high_resolution_clock::now();
  std::string s = "";
  for (int i = 0; i < 10000000; i++) {
    s += "a";
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << "C++: " << duration.count() << " ms (Length: " << s.length()
            << ")" << std::endl;
  return 0;
}
