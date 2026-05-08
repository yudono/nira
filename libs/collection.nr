# Collection Casting
# Mengubah struktur pengelompokan data.

export toList(iter):
  res = []
  for item in iter:
    res.push(item)
  return res

export toSet(list):
  # Simple implementation: unique items in a list
  res = []
  seen = {}
  for item in list:
    if not seen[item]:
      res.push(item)
      seen[item] = true
  return res

export toMap(list_of_pairs):
  res = {}
  for pair in list_of_pairs:
    res[pair[0]] = pair[1]
  return res

export entries(map):
  res = []
  keys = __builtin_obj_keys(map)
  for key in keys:
    res.push([key, map[key]])
  return res
