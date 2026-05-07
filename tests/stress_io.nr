# ============================================
# Test 5: I/O Stability
# ============================================
# Tests file read/write in loops, large data
# handling, and resource management.

from file import write, read, remove, exists, append

export main:
  print("=== STRESS: I/O Stability ===")

  # --- Basic file round-trip ---
  print("File Round-Trip:")
  write("_test_io.txt", "Hello Nira!")
  content = read("_test_io.txt")
  print("Read: " + content)
  remove("_test_io.txt")
  print("File removed")

  # --- Repeated write/read cycles ---
  print("")
  print("Write/Read Cycles (100x):")
  i = 0
  while i < 100:
    write("_test_cycle.txt", "iteration_" + i)
    data = read("_test_cycle.txt")
    if i == 0:
      print("First read: " + data)
    if i == 99:
      print("Last read: " + data)
    i = i + 1
  remove("_test_cycle.txt")
  print("100 cycles completed")

  # --- Large content write ---
  print("")
  print("Large Content Write:")
  big_content = ""
  j = 0
  while j < 500:
    big_content = big_content + "Line " + j + ": This is test data for stress testing file I/O operations.\n"
    j = j + 1
  write("_test_large.txt", big_content)
  read_back = read("_test_large.txt")
  print("Wrote 500 lines, read back length: " + len(read_back))
  remove("_test_large.txt")

  # --- Append test ---
  print("")
  print("Append Test:")
  write("_test_append.txt", "Line 1\n")
  k = 2
  while k <= 50:
    append("_test_append.txt", "Line " + k + "\n")
    k = k + 1
  appended = read("_test_append.txt")
  print("Appended 50 lines, length: " + len(appended))
  remove("_test_append.txt")

  # --- Multiple file handles ---
  print("")
  print("Multiple Files:")
  m = 0
  while m < 10:
    write("_test_multi_" + m + ".txt", "Content of file " + m)
    m = m + 1

  # Read all back
  n = 0
  while n < 10:
    c = read("_test_multi_" + n + ".txt")
    if n == 0:
      print("File 0: " + c)
    if n == 9:
      print("File 9: " + c)
    n = n + 1

  # Cleanup
  p = 0
  while p < 10:
    remove("_test_multi_" + p + ".txt")
    p = p + 1
  print("10 files created, read, and removed")

  # --- File exists check ---
  print("")
  print("File Exists Test:")
  write("_test_exists.txt", "check")
  ex = exists("_test_exists.txt")
  print("exists (before remove): " + ex)
  remove("_test_exists.txt")
  ex2 = exists("_test_exists.txt")
  print("exists (after remove): " + ex2)

  # --- Overwrite test ---
  print("")
  print("Overwrite Test:")
  write("_test_overwrite.txt", "original")
  print("Before: " + read("_test_overwrite.txt"))
  write("_test_overwrite.txt", "overwritten")
  print("After: " + read("_test_overwrite.txt"))
  remove("_test_overwrite.txt")

  # --- Empty file ---
  print("")
  print("Empty File Test:")
  write("_test_empty.txt", "")
  empty_content = read("_test_empty.txt")
  print("Empty file length: " + len(empty_content))
  remove("_test_empty.txt")

  print("")
  print("=== I/O Stability stress PASSED ===")
