# ============================================
# Test 2: Memory & Recursion (Stability Test)
# ============================================
# Tests deep recursion, stack depth limits,
# and massive object allocation via Arena.

# --- Ackermann Function ---
# Grows extremely fast, stresses the call stack
ackermann m, n:
  if m == 0:
    return n + 1
  if n == 0:
    return ackermann(m - 1, 1)
  return ackermann(m - 1, ackermann(m, n - 1))

# --- Deep linear recursion ---
count_down n:
  if n <= 0:
    return 0
  return count_down(n - 1)

# --- Recursive sum ---
recursive_sum n:
  if n <= 0:
    return 0
  return n + recursive_sum(n - 1)

# --- Mutual recursion ---
is_even n:
  if n == 0:
    return true
  return is_odd(n - 1)

is_odd n:
  if n == 0:
    return false
  return is_even(n - 1)

export main:
  print("=== STRESS: Recursion & Memory ===")

  # Ackermann (small values - it grows FAST)
  print("Ackermann Tests:")
  print("A(0,0) = " + ackermann(0, 0))
  print("A(1,1) = " + ackermann(1, 1))
  print("A(2,2) = " + ackermann(2, 2))
  print("A(3,3) = " + ackermann(3, 3))
  # A(3,4) = 125 - very deep recursion, skipped for interpreter speed

  # Deep linear recursion (stack depth test)
  print("")
  print("Deep Recursion:")
  result = count_down(100)
  print("count_down(100) = " + result)

  result2 = recursive_sum(100)
  print("recursive_sum(100) = " + result2)

  # Mutual recursion
  print("")
  print("Mutual Recursion:")
  print("is_even(10) = " + is_even(10))
  print("is_odd(11) = " + is_odd(11))
  print("is_even(7) = " + is_even(7))
  print("is_odd(0) = " + is_odd(0))

  # Massive object allocation (Arena GC Test)
  print("")
  print("Arena Allocation Stress:")
  items = []
  i = 0
  while i < 1000:
    items.push({ id: i, name: "object_" + i, value: i * 10 })
    i = i + 1
  print("Allocated 1000 objects")
  print("First: id=" + items[0].id)
  print("Last: id=" + items[999].id)
  print("items length = " + len(items))

  # Nested object allocation
  print("")
  print("Nested Object Stress:")
  root = { level: 0, data: "root" }
  current = root
  j = 1
  while j <= 20:
    child = { level: j, data: "level_" + j, parent_level: current.level }
    current = child
    j = j + 1
  print("Deepest level: " + current.level)
  print("Deepest data: " + current.data)

  # Array growth stress
  print("")
  print("Array Growth Stress:")
  big_arr = []
  k = 0
  while k < 2000:
    big_arr.push(k)
    k = k + 1
  print("Array size: " + len(big_arr))
  print("big_arr[0] = " + big_arr[0])
  print("big_arr[1999] = " + big_arr[1999])

  print("")
  print("=== Recursion & Memory stress PASSED ===")
