# Object library for Nira

keys(obj):
  return __builtin_obj_keys(obj)

assign(target, source):
  for key in __builtin_obj_keys(source):
    target[key] = source[key]
  return target
