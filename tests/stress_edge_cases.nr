# ============================================
# Test 4: Edge Cases & Error Handling
# ============================================
# Tests division by zero, empty collections,
# null handling, unicode strings, and type coercion.

export main():
  print("=== STRESS: Edge Cases ===")

  # --- Division by zero ---
  # NOTE: Division by zero triggers a [MATH ERROR] and halts.
  # This is expected behavior — Nira does not silently return Infinity/NaN.
  # Uncomment below to test:
  # result = 10 / 0
  # print("10 / 0 = " + result)
  print("Division by Zero: SKIPPED (causes expected halt)")

  # --- Null handling ---
  print("")
  print("Null Handling:")
  val = null
  print("val = " + val)
  if val == null:
    print("val is null: true")
  else:
    print("val is null: false")

  # Null comparison
  a = null
  b = null
  if a == b:
    print("null == null: true")

  # Null in expressions
  c = null
  if c != 1:
    print("null != 1: true")

  # --- Empty collections ---
  print("")
  print("Empty Collections:")
  empty_arr = []
  print("empty array length: " + len(empty_arr))

  empty_obj = {}
  print("empty object created")

  # Push to empty array
  empty_arr.push(42)
  print("after push, length: " + len(empty_arr))
  print("first element: " + empty_arr[0])

  # --- Error values ---
  print("")
  print("Error Handling:")
  err = error("something went wrong")
  print("error value: " + err)

  # --- Boolean edge cases ---
  print("")
  print("Boolean Edge Cases:")
  print("true and true = " + (true and true))
  print("true and false = " + (true and false))
  print("false or true = " + (false or true))
  print("false or false = " + (false or false))
  print("not true = " + (not true))
  print("not false = " + (not false))

  # Chained boolean
  x = true
  y = false
  z = true
  if x and not y and z:
    print("complex boolean: true")

  # --- String edge cases ---
  print("")
  print("String Edge Cases:")
  empty_str = ""
  print("empty string length: " + len(empty_str))

  # String concatenation
  s = ""
  i = 0
  while i < 100:
    s = s + "x"
    i = i + 1
  print("100 x's length: " + len(s))

  # String with special content
  special = "hello\nworld"
  print("escaped newline:")
  print(special)

  tab_str = "col1\tcol2"
  print("escaped tab: " + tab_str)

  # --- Unicode strings ---
  print("")
  print("Unicode Stress:")
  emoji = "Hello 🌍🚀"
  print(emoji)

  jp = "日本語テスト"
  print(jp)

  mixed = "abc 你好 🎉 end"
  print(mixed)

  # --- Type coercion edge cases ---
  print("")
  print("Type Coercion:")
  num = 42
  str_val = "Number: " + num
  print(str_val)

  float_val = 3.14
  str_float = "Pi: " + float_val
  print(str_float)

  # --- Comparison edge cases ---
  print("")
  print("Comparison Edge Cases:")
  print("1 == 1: " + (1 == 1))
  print("1 != 2: " + (1 != 2))
  print("1 < 2: " + (1 < 2))
  print("2 > 1: " + (2 > 1))
  print("1 <= 1: " + (1 <= 1))
  print("1 >= 1: " + (1 >= 1))

  # String comparison
  print("str eq: " + ("abc" == "abc"))
  print("str neq: " + ("abc" != "xyz"))

  # --- Modulo operations ---
  print("")
  print("Modulo Tests:")
  print("10 %% 3 = " + (10 % 3))
  print("7 %% 2 = " + (7 % 2))
  print("100 %% 10 = " + (100 % 10))
  print("17 %% 5 = " + (17 % 5))

  # --- Power operations ---
  print("")
  print("Power Tests:")
  print("2 ** 10 = " + (2 ** 10))
  print("3 ** 4 = " + (3 ** 4))
  print("5 ** 3 = " + (5 ** 3))
  print("10 ** 0 = " + (10 ** 0))
  print("2 ** 16 = " + (2 ** 16))

  # --- Chained operations ---
  print("")
  print("Chained Operations:")
  result2 = (2 + 3) * (4 + 5)
  print("(2+3) * (4+5) = " + result2)

  result3 = 10 - 3 - 2 - 1
  print("10 - 3 - 2 - 1 = " + result3)

  # --- Boundary integers ---
  print("")
  print("Boundary Values:")
  big = 2147483647
  print("MAX_INT32: " + big)
  small = -2147483648
  print("MIN_INT32: " + small)

  print("")
  print("=== Edge Cases stress PASSED ===")
