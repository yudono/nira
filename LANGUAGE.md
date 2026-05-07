# 📘 Nira Language Documentation

Nira is a modern, zero-dependency programming language designed for simplicity, performance, and cross-platform compatibility.

## 🚀 Getting Started

### Installation
Clone the repository and run:
```bash
make
```

### Running Code
```bash
./nira run examples/task_app/app.nr
```

### Building Native Binaries
```bash
./nira build examples/task_app/app.nr
./build/app
```

---

## 🧩 Syntax Reference

### Function Definition
Functions are defined using `name params:`.
```nira
add a b:
  return a + b

main:
  result = add(5, 10)
  print(result)
```

### Variable Assignment
```nira
name = "Nira"
age = 1
```

### Control Flow
```nira
if age > 0:
  print("Alive")
else:
  print("Unknown")

for item in items:
  print(item)

while count > 0:
  count = count - 1
```

### Objects and Arrays
```nira
user = {
  name: "Budi",
  age: 25
}

items = [1, 2, 3]
```

### Imports
Nira distinguishes between workspace files, internal libraries, and external dependencies.

#### 1. Workspace Import
Use quotes to import a local file relative to the project root.
```nira
import "config/config"  # Imports config/config.nr
```

#### 2. Library & Dependency Import
Use identifiers without quotes for internal libraries (built-in) or external dependencies (found in `.nira/libs/*`).
```nira
import time  # Internal library
import os    # Internal library
import requests # External dependency
```

#### 3. Specific Imports (`from`)
You can import specific functions or variables from a module.
```nira
from time import millis
from "config/config" import db
```

---

## 📦 Standard Library

Nira provides a rich set of built-in modules to accelerate development.

### **1. Input/Output & Files (`nira.io`, `nira.fs`)**
```nira
import io
import fs

main:
  print("Enter your name: ")
  name = io.input()
  fs.writeFile("name.txt", name)
  println("Saved!")
```

### **2. Networking (`nira.http`, `nira.net`)**
The `http` module provides a zero-dependency web server and client.
```nira
import http

main:
  # Client
  res = http.get("https://api.example.com")
  
  # Server
  app = http.app()
  app.get("/", ctx -> ctx.json({ status: "ok" }))
  app.listen(3000)
```

### **3. Data Encoding (`nira.json`, `nira.encoding`)**
```nira
import json

main:
  data = { id: 1, tags: ["new", "fast"] }
  str = json.encode(data)
  obj = json.decode(str)
```

### **4. Concurrency (`nira.async`, `nira.sync`)**
```nira
import async

fetchData:
  return http.get("/data")

main:
  p1 = async.spawn(fetchData)
  p2 = async.spawn(fetchData)
  results = async.all([p1, p2])
```

### **5. Other Modules**
- **nira.math**: Constants like `PI` and functions like `sqrt`, `random`.
- **nira.time**: `now()`, `unix()`, and `sleep(ms)`.
- **nira.os**: `getEnv()`, `exit()`, and platform info.
- **nira.crypto**: Hashing (`sha256`) and secure random.
- **nira.test**: Built-in test runner with `describe` and `expect`.

---

## ⚙️ Architecture

- **Compiler**: Transpiles Nira to optimized C code.
- **Runtime**: Zero-dependency C runtime (libc + libsqlite3).
- **Linker**: Clang/GCC for final binary generation.
- **Cross-Platform**: Supports macOS (arm64), Linux, and Windows (via MinGW).

---

## 🎨 Design Philosophy
1. **Readability**: Code should be easy to understand at a glance.
2. **Explicit over Implicit**: No hidden magic.
3. **Zero Dependency**: The generated binaries should be small and portable.
4. **Performance**: Native speed via C transpilation.
