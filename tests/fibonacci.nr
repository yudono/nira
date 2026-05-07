fib val:
  if val == 0:
    return 0
  if val == 1:
    return 1
  return fib(val - 1) + fib(val - 2)

main:
  print(fib(10))
