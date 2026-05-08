# ============================================
# Test 1: High-Precision Arithmetic (Stress Test)
# ============================================
# Tests integer overflow, large number handling,
# and floating-point precision boundaries.

# --- Integer Factorial Test ---
factorial(n):
  if n <= 1:
    return 1
  return n * factorial(n - 1)

# --- Fibonacci Stress (Large Index) ---
fib_iter(n):
  if n <= 0:
    return 0
  if n == 1:
    return 1
  a = 0
  b = 1
  i = 2
  while i <= n:
    temp = a + b
    a = b
    b = temp
    i = i + 1
  return b

# --- Power function ---
power(base, exp):
  result = 1
  i = 0
  while i < exp:
    result = result * base
    i = i + 1
  return result

# --- Sum of series (precision test) ---
harmonic_sum(n):
  total = 0.0
  i = 1
  while i <= n:
    total = total + 1.0 / i
    i = i + 1
  return total

export main():
  print("=== STRESS: Arithmetic ===")

  # Small factorials (sanity check)
  print("Factorial Tests:")
  print("5! = " + factorial(5))
  print("10! = " + factorial(10))
  print("15! = " + factorial(15))
  print("20! = " + factorial(20))

  # Large fibonacci
  print("")
  print("Fibonacci Stress:")
  print("fib(10) = " + fib_iter(10))
  print("fib(20) = " + fib_iter(20))
  print("fib(30) = " + fib_iter(30))
  print("fib(40) = " + fib_iter(40))

  # Power (overflow test)
  print("")
  print("Power Tests:")
  print("2^10 = " + power(2, 10))
  print("2^20 = " + power(2, 20))
  print("2^30 = " + power(2, 30))
  print("3^15 = " + power(3, 15))

  # Large multiplication chain
  print("")
  print("Large Multiply Chain:")
  val = 1
  i = 1
  while i <= 25:
    val = val * i
    i = i + 1
  print("25! = " + val)

  # Floating point precision
  print("")
  print("Float Precision Tests:")
  a = 0.1 + 0.2
  print("0.1 + 0.2 = " + a)

  b = 1.0 / 3.0
  print("1/3 = " + b)

  c = harmonic_sum(100)
  print("Harmonic(100) = " + c)

  # Negative arithmetic
  print("")
  print("Negative Arithmetic:")
  print("-5 * -3 = " + (-5 * -3))
  print("-100 + 50 = " + (-100 + 50))
  print("10 - 100 = " + (10 - 100))

  print("")
  print("=== Arithmetic stress PASSED ===")
