# json library for Nira

encode data:
  return __builtin_json_encode(data)

decode str:
  # Simplification: return empty object if decode not implemented in C yet
  return {}
