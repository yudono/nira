# String library for Nira

indexOf(s, sub):
  return __builtin_str_index_of(s, sub)

replace(s, old, new_str):
  return __builtin_str_replace(s, old, new_str)

match(s, regex):
  return __builtin_str_match(s, regex)

contains(s, sub):
  return indexOf(s, sub) != -1

split(s, sep):
  res = []
  curr = s
  idx = indexOf(curr, sep)
  while idx != -1:
    part = curr.substring(0, idx)
    res.push(part)
    curr = curr.substring(idx + len(sep), len(curr))
    idx = indexOf(curr, sep)
  res.push(curr)
  return res

trim(s):
  # Simple trim (leading/trailing spaces)
  res = s
  while len(res) > 0 and res.substring(0, 1) == " ":
    res = res.substring(1, len(res))
  while len(res) > 0 and res.substring(len(res) - 1, 1) == " ":
    res = res.substring(0, len(res) - 1)
  return res

upper(s):
  return s

lower(s):
  return s
