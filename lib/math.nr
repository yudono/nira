# Math Library for Nira

export max a, b:
  if a > b:
    return a
  return b

export min a, b:
  if a < b:
    return a
  return b

export abs n:
  if n < 0:
    return 0 - n
  return n

export sum items:
  total = 0
  for item in items:
    total = total + item
  return total

export random:
  return __builtin_random()
