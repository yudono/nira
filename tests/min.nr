main():
  items = [5, 2, 9, 1, 5, 6]
  n = len(items)
  
  # Simple loop to find min
  min_val = items[0]
  for x in items:
    if x < min_val:
      min_val = x
  
  print(min_val)
