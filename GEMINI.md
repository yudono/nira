Berikut versi **GEMINI.md lengkap & siap pakai** dari prompt kamu—dirapikan, diperjelas, dan dioptimalkan agar bisa langsung dipakai sebagai **blueprint engineering + AI instruction doc**.

---

# 📘 GEMINI.md — Nira Programming Language

## 🧠 Overview

Nira adalah bahasa pemrograman modern yang dirancang untuk:

- **Sangat mudah dibaca (read once, understand)**
- **Konsisten (tidak banyak gaya penulisan)**
- **Performa tinggi (compiled, efisien memori)**
- **Tanpa kompleksitas OOP klasik**
- **Cocok untuk sistem skala besar & jangka panjang**

Nira menggabungkan filosofi:

- Python → simplicity
- Rust → performance mindset
- Go → consistency

---

# 🎯 Core Principles

1. **Readability First**
2. **Consistency Over Flexibility**
3. **Explicit Over Magic**
4. **Composition Over Inheritance**
5. **Simple Surface, Powerful Compiler**

---

# ❌ What Nira Avoids

- Class & inheritance (`extends`, `super`)
- Hidden behavior / implicit magic
- Multiple ways to do the same thing
- Overly clever syntax
- Runtime dynamic chaos

---

# 🧩 Language Syntax

## 🔹 Function Definition

```nira
createUser data:
  return {
    id: data.id
    name: data.name
  }
```

---

## 🔹 Object Literal

```nira
user = {
  id: 1
  name: "Budi"
}
```

---

## 🔹 Named Parameters

```nira
createUser {
  id: 1
  name: "Budi"
}
```

---

## 🔹 Control Flow

```nira
if user.id == 1:
  print "found"
```

---

## 🔹 Loop

```nira
for item in items:
  print item.name
```

---

## 🔹 Allowed Shortcuts

```nira
ids = raw.split(",").map(toInt)

total = sum(items, i -> i.price)

user = getUser(id) or stop
```

---

# ⚙️ Type System

## 🧠 Model: Contained Dynamic Typing

- Type tidak ditulis oleh user
- Compiler melakukan inference
- Tidak ada dynamic runtime liar

---

## 🔹 Primitive Types

- int
- float
- string
- bool

---

## 🔹 Object Shape Typing

```nira
user = {
  id: 1
  name: "Budi"
}
```

👉 type:

```
{ id: int, name: string }
```

---

## 🔹 Union Type (Error Handling)

```nira
getUser id:
  if id <= 0:
    return error "not found"
```

👉 inferred:

```
User | Error
```

---

# 🧠 Memory Model

## 🎯 Goals

- No heavy GC
- High performance
- Automatic (no manual free)

---

## 🔹 Strategy

### 1. Stack-first allocation

- Default object → stack

### 2. Escape Analysis

Jika object:

- di-return
- disimpan global

👉 dipindah ke heap

---

## 🔹 Example

```nira
makeUser:
  return { id: 1 }
```

👉 heap allocation

---

```nira
printUser:
  u = { id: 1 }
  print u
```

👉 stack allocation

---

## 🔹 Arena Allocation

```nira
arena request:
  users = []
  for i in 1..1000:
    users.push({ id: i })
```

👉 free sekaligus setelah scope selesai

---

## 🔹 Reference Model

- Object passing = reference
- Tidak copy kecuali eksplisit

---

# 🧱 Compiler Architecture (Native Path)

## 🔄 Pipeline (Target)

```
Source (.nr)
  ↓
Lexer
  ↓
Parser → AST
  ↓
Interpreter (Evaluator) → for `nira run`
  ↓
IR (Intermediate Representation)
  ↓
Codegen (x86_64 / ARM64) → for `nira build`
  ↓
Executable (ELF / Mach-O / PE)
```

---

# 🧠 Evaluator (Interpreter)

Untuk `nira run`, kita tidak perlu C compiler. Kita akan melakukan tree-walk pada AST:

- **Literal**: Return value
- **Binary**: Eval left, eval right, apply op
- **Function**: Store in environment
- **Call**: Create new scope, bind params, eval body

---

# 🏗️ Native Codegen

Untuk `nira build`, kita akan emit machine code langsung:

- **Registers**: Penggunaan rax/eax untuk return
- **Stack Frame**: Setup rbp/rsp
- **Syscalls**: Langsung panggil kernel (e.g. syscall 0x2000004 untuk write di macOS)

---

## 🔧 Components

### 1. Lexer

- Tokenize indentation-based syntax

### 2. Parser

- Recursive descent
- Bangun AST

### 3. Semantic

- Scope validation
- Variable resolution

### 4. Type Inference

- Infer primitive
- Infer object shape
- Union type

### 5. Codegen

- LLVM IR (C API)

---

# 🔤 Grammar (EBNF)

```
program     = { statement }

statement   = function
            | assignment
            | expression

function    = IDENT { IDENT } ":" block

block       = INDENT { statement } DEDENT

assignment  = IDENT "=" expression

expression  = literal
            | IDENT
            | object
            | call
            | binary

object      = "{" { IDENT ":" expression } "}"

call        = IDENT expression

binary      = expression OP expression
```

---

# 🔧 LLVM Codegen (C API)

## 🔹 Mapping

| Nira     | LLVM         |
| -------- | ------------ |
| int      | i32          |
| object   | struct       |
| function | LLVMFunction |

---

## 🔹 Example

```c
LLVMValueRef val = LLVMConstInt(LLVMInt32Type(), 5, 0);
```

---

## 🔹 Object → Struct

- field di-map ke LLVM struct
- layout di-optimize

---

# 🧪 Debugging

## 🎯 Target

- Source-level debugging
- Mapping line → IR

---

## 🔹 Tools

- LLDB
- GDB

---

## 🔹 Approach

- Generate DWARF
- Attach metadata ke IR
- Support breakpoint

---

# 🧰 Tooling

## CLI

```bash
nira run main.nr
nira build main.nr
```

---

## Formatter

- enforce indent
- enforce style

---

## Linter

- no ambiguity
- no dynamic misuse

---

## Package Manager (optional)

```bash
nira add http
```

---

# 📦 Standard Library

## Modules

- http
- file
- string
- array

---

## Example

```nira
http.get "/api"
```

---

# 🚀 Example Program (Full)

```nira
createUser data:
  return {
    id: data.id
    name: data.name
  }

main:
  user = createUser {
    id: 1
    name: "Budi"
  }

  print user.name
```

---

# 🧠 Design Decisions

## ❌ No OOP

Tidak ada:

- class
- extends
- inheritance

---

## ✅ Replacement

- object
- function
- composition

---

# ⚠️ Constraints

- Tidak boleh banyak style
- Tidak boleh implicit behavior
- Harus bisa dibaca sekali

---

# 🗺️ Roadmap (Refined)

## Phase 1: The Independent Engine
- [ ] Tree-walk Interpreter (Evaluator)
- [ ] `nira run` works WITHOUT Clang/GCC
- [ ] Native Environment (Scope management)

## Phase 2: Binary Genesis
- [ ] Simple IR (Virtual instructions)
- [ ] Mach-O / ELF Header generator
- [ ] Basic Codegen (Int arithmetic & return)

## Phase 3: The Runtime
- [ ] Custom `print` (No printf)
- [ ] Custom `alloc` (mmap/brk)
- [ ] No external dependencies

---

# 🔥 Final Philosophy

> “Code is written once, but read many times.”

---

# ✅ Usage Instruction (for AI / Team)

Gunakan dokumen ini untuk:

- implement compiler
- diskusi arsitektur
- training AI model
- guideline coding

---

Kalau mau next step, gue bisa:

👉 generate **parser C full (real code, bukan pseudo)**
👉 implement **type inference engine detail**
👉 atau bikin **mini compiler yang benar-benar bisa run**

Tinggal pilih: mau langsung ke implementasi atau dalemin engine dulu.
