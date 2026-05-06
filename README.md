# Nira Programming Language

Nira is a modern, indentation-based programming language designed for readability and performance. It transpiles to C for high-speed execution.

## Features

- **Clean Syntax**: Indentation-based blocks (Python-like).
- **Modern Objects**: Simple object literals and field access.
- **Fast**: Transpiles to optimized C code.
- **Modular**: Support for `import`, `export`, and `import ... from ...`.
- **System Built-ins**: `print`, `exec`, `file_read`, `file_write`, `delay`.

## CLI Usage

### Running a script

```bash
./nira run tests/fibonacci.nr
```

### Building a standalone binary

```bash
./nira build tests/fibonacci.nr
```

The binary will be available in the `build/` directory.

### Running all tests

```bash
./nira test
```

### Showing help

```bash
./nira help
```

## Standard Library (`lib/`)

- `lib/math`: `max`, `min`, `abs`
- `lib/file`: `read`, `write`
- `lib/sys`: `run`, `print_v`
- `lib/time`: `sleep`
- `lib/string`: `concat`, `wrap`

### Example Import

```nira
import max, min from "lib/math"

main:
  print (max 10 20)
```

## Development

### Building the compiler

```bash
make
```

### Building the standard library

```bash
./build_lib.sh
```

# Building the system

make clean && make
./nira build examples/task_app/app.nr
cc -w build/examples/task_app/app.nr.c -o build/task_app

# Testing persistence

./build/task_app add "Zero Dependency Success"
./build/task_app list
