# 📦 Nira Standard Library

This directory contains the core implementation of the Nira Standard Library.

## 📚 Available Modules

### **1. nira.io & nira.fs**
- **nira.io**: `print`, `println`, `printf`, `input`, `read`, `write`.
- **nira.fs**: File operations including `open`, `readFile`, `writeFile`, `mkdir`, `remove`, and `stats`.

### **2. nira.core**
- **nira.string**: String manipulation (`length`, `split`, `join`, `replace`, etc.).
- **nira.array**: Array operations (`push`, `pop`, `map`, `filter`, `sort`, etc.).
- **nira.map**: Key-value store management.
- **nira.set**: Unique collection operations.
- **nira.buffer**: Memory buffer allocation and conversion.

### **3. nira.math**
- Constants: `PI`, `E`, `INF`, `NAN`.
- Functions: `abs`, `sqrt`, `pow`, `sin`, `cos`, `floor`, `random`, etc.

### **4. nira.time**
- Clock: `now()`, `unix()`.
- Formatting and manipulation of time and duration.
- `sleep(ms)` for execution control.

### **5. nira.os & nira.path**
- **nira.os**: Environment variables, process exit, platform detection, and system stats.
- **nira.path**: Path joining, resolution, and manipulation.

### **6. nira.net & nira.http**
- **nira.http**: High-performance HTTP client and server.
- **nira.net**: Low-level networking (TCP/UDP) and DNS lookups.
- **nira.url**: URL parsing and encoding.

### **7. nira.encoding**
- Support for **JSON**, **Base64**, **Hex**, and **CSV**.

### **8. nira.crypto**
- Hashing: `md5`, `sha1`, `sha256`, `sha512`.
- `hmac` and `SecureRandom` (UUID, bytes).

### **9. nira.sync & nira.async**
- **nira.async**: `spawn`, `await`, `all`.
- **nira.sync**: `Mutex`, `WaitGroup`, `Channel` for safe concurrency.

### **10. nira.reflect & nira.debug**
- **nira.reflect**: Runtime type inspection.
- **nira.debug**: `assert`, `trace`, `dump`.
- **nira.test**: Built-in testing framework (`describe`, `it`, `expect`).

---
*For detailed API reference, see [STANDARD_LIBRARY.md](../STANDARD_LIBRARY.md).*
