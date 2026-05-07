#!/bin/bash

echo "🚀 Starting Performance Benchmark (Fibonacci 35)"
echo "------------------------------------------------"

# 1. Python
python3 benchmarks/fib.py

# 2. Node.js
node benchmarks/fib.js

# 3. C++
g++ -O3 benchmarks/fib.cpp -o benchmarks/fib_cpp
./benchmarks/fib_cpp

# 4. Nira (Interpreted)
echo -n "Nira (Interpreted): "
./nira run benchmarks/fib.nr | grep "Nira:" | cut -d' ' -f2-

# 5. Nira (Transpiled & Compiled)
./nira build benchmarks/fib.nr > /dev/null 2>&1
./build/fib

echo "------------------------------------------------"
echo "🏁 Benchmark Finished"
