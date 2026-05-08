# Mini JSON-like Formatter
# Tests complex object traversal and string manipulation.

format_val val, indent:
  t = typeof(val)
  # print("DEBUG Type: " + t)
  
  if val == null:
    return "null"
  
  if t == "string":
    return "\"" + val + "\""
  
  if t == "int":
    return toString(val)
  
  if t == "bool":
    return toString(val)
  
  if t == "array":
    return format_array(val, indent)
  
  if t == "object":
    return format_object(val, indent)
  
  return "unknown: " + t

format_array arr, indent:
  if len(arr) == 0:
    return "[]"
  
  res = "[\n"
  new_indent = indent + "  "
  i = 0
  while i < len(arr):
    res = res + new_indent + format_val(arr[i], new_indent)
    if i < len(arr) - 1:
      res = res + ","
    res = res + "\n"
    i = i + 1
  
  return res + indent + "]"

format_object obj, indent:
  keys = object.keys(obj)
  if len(keys) == 0:
    return "{}"
  
  res = "{\n"
  new_indent = indent + "  "
  i = 0
  while i < len(keys):
    k = keys[i]
    v = obj[k]
    res = res + new_indent + "\"" + k + "\": " + format_val(v, new_indent)
    if i < len(keys) - 1:
      res = res + ","
    res = res + "\n"
    i = i + 1
  
  return res + indent + "}"

main:
  print("=== MINI JSON ENGINE TEST ===")
  
  complex_data = {
    id: 101,
    title: "Test Suite",
    active: true,
    tags: ["nira", "test", "complex"],
    config: {
      timeout: 5000,
      retries: 3,
      endpoints: [
        { url: "/api/v1", method: "GET" },
        { url: "/api/v2", method: "POST" }
      ]
    },
    metadata: null
  }
  
  formatted = format_val(complex_data, "")
  print("Formatted Output:")
  print(formatted)
  
  print("=== JSON ENGINE TEST PASSED ===")
