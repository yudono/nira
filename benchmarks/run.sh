#!/bin/bash

# Find all subdirectories in benchmarks/
for DIR in benchmarks/*/ ; do
    # Remove trailing slash
    GROUP=$(basename "$DIR")
    
    # Skip if not a directory
    [ -d "$DIR" ] || continue
    
    echo "🚀 Benchmarking Group: $GROUP"
    echo "------------------------------------------------"
    
    # 1. Python
    PY_FILE=$(find "$DIR" -name "*.py" | head -n 1)
    if [ -n "$PY_FILE" ] && [ -f "$PY_FILE" ]; then
        python3 "$PY_FILE"
    fi
    
    # 2. Node.js
    JS_FILE=$(find "$DIR" -name "*.js" | head -n 1)
    if [ -n "$JS_FILE" ] && [ -f "$JS_FILE" ]; then
        node "$JS_FILE"
    fi
    
    # 3. C++
    CPP_FILE=$(find "$DIR" -name "*.cpp" | head -n 1)
    if [ -n "$CPP_FILE" ] && [ -f "$CPP_FILE" ]; then
        g++ -O3 "$CPP_FILE" -o "$DIR/cpp_bench"
        "$DIR/cpp_bench"
        rm "$DIR/cpp_bench"
    fi
    
    # 4. Nira (Interpreted)
    NR_FILE=$(find "$DIR" -name "*.nr" | head -n 1)
    if [ -n "$NR_FILE" ] && [ -f "$NR_FILE" ]; then
        echo -n "Nira (Interpreted): "
        ./nira run "$NR_FILE" | grep "Nira:" | cut -d' ' -f2-
    fi
    
    # 5. Nira (Compiled)
    if [ -n "$NR_FILE" ] && [ -f "$NR_FILE" ]; then
        echo -n "Nira (Compiled):    "
        ./nira build "$NR_FILE" > /dev/null 2>&1
        BIN_NAME=$(basename "$NR_FILE" .nr)
        if [ -f "./build/$BIN_NAME" ]; then
            ./build/"$BIN_NAME" | grep "Nira:" | cut -d' ' -f2-
        fi
    fi
    
    echo "------------------------------------------------"
    echo ""
done

echo "🏁 All Benchmarks Finished"
