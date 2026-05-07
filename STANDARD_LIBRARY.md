# 📚 Nira Standard Library (STDLIB)

Nira comes with a robust standard library designed for performance and readability. Below are the currently implemented modules and their APIs.

---

## 📦 Core Modules

### 1. `conv` (Core Type Conversions)
The primary module for explicit type casting.
- `toInt(any, [base])`: Converts `any` to an integer. Optional `base` for string conversion.
- `toFloat(any)`: Converts `any` to a floating-point number.
- `toStr(any)`: Returns a string representation of any data type.
- `toBool(any)`: Converts `any` to boolean following truthy/falsy rules.

### 2. `parse` (String Parsing)
Safely extract data from raw text.
- `parseInt(str)`: Parses an integer from a string.
- `parseFloat(str)`: Parses a float from a string.
- `parseBool(str)`: Converts "true"/"1" to true, everything else to false.

### 3. `encoding` (Data Representation)
- `toBase64(data)`: Encodes string data to Base64 format.
- `fromBase64(str)`: Decodes Base64 string back to original data.
- `toHex(data)`: Converts numbers or strings to hexadecimal representation.

### 4. `collection` (Data Structures)
- `toList(iter)`: Converts any iterable (Map, Set, String) to a List.
- `toSet(list)`: Returns a unique list of elements from the input list.
- `toMap(list_of_pairs)`: Constructs a Map from a list of `[key, value]` pairs.
- `entries(map)`: Returns a list of `[key, value]` pairs from a Map.

### 5. `json` (Serialization)
- `stringify(obj)`: Serializes a Nira object/array to a JSON string.
- `parse(json_str)`: Deserializes a JSON string into Nira objects.

### 6. `time` (Time & Control)
- `toUnix(time_obj)`: Converts a date object `{year, month, day}` to a Unix timestamp.
- `fromDate(year, month, day)`: Helper to create a date object.
- `sleep(ms)`: Suspends execution for the specified milliseconds.
- `millis()`: Returns the current system time in milliseconds.

### 7. `math` (Mathematical Operations)
- `sqrt(n)`: Square root.
- `pow(base, exp)`: Exponentiation.
- `sin(n)` / `cos(n)`: Trigonometric functions.
- `random()`: Returns a pseudo-random integer.

### 8. `file` (I/O Operations)
- `read(path)`: Reads entire file content.
- `write(path, content)`: Writes content to file.
- `delete(path)`: Removes a file from the system.
- `exists(path)`: Checks if a file or directory exists.

---

## 🚀 Usage Example

```nira
import json
import conv

main:
  data = { id: "123", active: true }
  json_str = json.stringify(data)
  print "Serialized: " + json_str
  
  id_num = conv.toInt(data.id)
  print "ID as number: " + toString(id_num)
```
