main():
  items = [5, 2, 9, 1, 5, 6]
  n = len(items)
  
  # Bubble sort
  i = 0
  for _ in items:
    j = 0
    for __ in items:
      if j < n - 1:
        if items[j + 1] < items[j]:
          # Swap
          temp = items[j]
          items[j] = items[j + 1]
          items[j + 1] = temp
      j = j + 1
    i = i + 1

  # Print sorted
  for x in items:
    print(x)
