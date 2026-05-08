# Math Library for Nira

export max(a, b):
  if a > b:
    return a
  return b

export min(a, b):
  if a < b:
    return a
  return b

export abs(n):
  if n < 0:
    return 0 - n
  return n

export sum(items):
  total = 0
  for item in items:
    total = total + item
  return total

export sqrt(n):
  return __builtin_math_sqrt(n)

export pow(b, e):
  return __builtin_math_pow(b, e)

export sin(n):
  return __builtin_math_sin(n)

export cos(n):
  return __builtin_math_cos(n)

export random():
  return __builtin_random()
