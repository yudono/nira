# ============================================
# Test 3: Stress Test Struktur Data
# ============================================
# Tests nested collections, large array sorting,
# and complex data manipulation.

import "object"

# --- Bubble Sort ---
bubble_sort(items):
  n = len(items)
  i = 0
  while i < n:
    j = 0
    while j < n - 1:
      if items[j + 1] < items[j]:
        temp = items[j]
        items[j] = items[j + 1]
        items[j + 1] = temp
      j = j + 1
    i = i + 1
  return items

# --- Linear search ---
linear_search(items, target):
  i = 0
  while i < len(items):
    if items[i] == target:
      return i
    i = i + 1
  return -1

# --- Reverse array ---
reverse_arr(items):
  result = []
  i = len(items) - 1
  while i >= 0:
    result.push(items[i])
    i = i - 1
  return result

# --- Array contains ---
contains(items, target):
  i = 0
  while i < len(items):
    if items[i] == target:
      return true
    i = i + 1
  return false

export main():
  print("=== STRESS: Data Structures ===")

  # --- Sorting stress ---
  print("Sort Stress (100 elements):")
  nums = []
  i = 100
  while i > 0:
    nums.push(i)
    i = i - 1
  # nums is now [100, 99, 98, ..., 1]
  sorted = bubble_sort(nums)
  print("sorted[0] = " + sorted[0])
  print("sorted[49] = " + sorted[49])
  print("sorted[99] = " + sorted[99])

  # --- Search test ---
  print("")
  print("Search Tests:")
  idx = linear_search(sorted, 42)
  print("Found 42 at index: " + idx)
  idx2 = linear_search(sorted, 999)
  print("Found 999 at index: " + idx2)

  # --- Reverse test ---
  print("")
  print("Reverse Test:")
  small = [1, 2, 3, 4, 5]
  rev = reverse_arr(small)
  print("Reversed: ")
  for x in rev:
    print(x)

  # --- Nested object collections ---
  print("")
  print("Nested Collections:")
  users = []
  j = 0
  while j < 50:
    user = {
      id: j,
      name: "user_" + j,
      profile: {
        email: "user" + j + "@test.com",
        settings: {
          theme: "dark",
          level: j
        }
      },
      scores: [j * 10, j * 20, j * 30]
    }
    users.push(user)
    j = j + 1

  # Access deeply nested data
  print("users[0].profile.settings.theme = " + users[0].profile.settings.theme)
  print("users[25].name = " + users[25].name)
  print("users[49].profile.settings.level = " + users[49].profile.settings.level)
  print("users[10].scores[2] = " + users[10].scores[2])

  # --- Object key manipulation ---
  print("")
  print("Object Key Tests:")
  data = { a: 1, b: 2, c: 3, d: 4, e: 5 }
  keys = object.keys(data)
  print("Keys count: " + len(keys))
  for k in keys:
    print(k + " = " + data[k])

  # --- Dynamic field update ---
  print("")
  print("Dynamic Field Update:")
  record = { name: "test", count: 0 }
  m = 0
  while m < 100:
    record.count = record.count + 1
    m = m + 1
  print("record.count = " + record.count)

  # --- Array of arrays ---
  print("")
  print("Array of Arrays:")
  matrix = []
  row = 0
  while row < 5:
    r = []
    col = 0
    while col < 5:
      r.push(row * 5 + col)
      col = col + 1
    matrix.push(r)
    row = row + 1
  print("matrix[0][0] = " + matrix[0][0])
  print("matrix[2][3] = " + matrix[2][3])
  print("matrix[4][4] = " + matrix[4][4])

  # --- Contains test ---
  print("")
  print("Contains Tests:")
  haystack = [10, 20, 30, 40, 50]
  print("contains 30: " + contains(haystack, 30))
  print("contains 99: " + contains(haystack, 99))

  print("")
  print("=== Data Structures stress PASSED ===")
