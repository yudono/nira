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
Modules are imported using `import "name"`.
```nira
import "http"
import "json"
```

---

## 📦 Standard Library

### HTTP
The `http` module provides a zero-dependency web server.
```nira
import "http"

main:
  app = http.app()
  app.get("/", ctx -> ctx.json({ message: "Hello World" }))
  app.listen(3000)
```

### Database (SQLite)
The `task` library (in examples) demonstrates SQLite usage.
```nira
_db = db.open("data.db")
_db.exec("INSERT INTO users (name) VALUES (?)", ["Budi"])
results = _db.query("SELECT * FROM users")
```

### JSON
```nira
data = { name: "Budi" }
encoded = json.encode(data)
decoded = json.decode(encoded)
```

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
