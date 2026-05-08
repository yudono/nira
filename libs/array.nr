# Array Library for Nira

export map items, callback:
  res = []
  for item in items:
    __builtin_push(res, callback(item))
  return res

export foreach items, callback:
  for item in items:
    callback(item)

export reduce items, initial, callback:
  acc = initial
  for item in items:
    acc = callback(acc, item)
  return acc

export push items, val:
  return __builtin_push(items, val)

export pop items:
  return __builtin_pop(items)

export length items:
  return len(items)
