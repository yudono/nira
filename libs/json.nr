# Object Serialization
# Konversi antara objek internal dengan format JSON.

export stringify(obj):
  return __builtin_json_stringify(obj)

export parse(json_str):
  return __builtin_json_parse(json_str)
