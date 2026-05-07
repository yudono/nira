main:
  print("Testing Arena Allocator...")
  a = { x: 10, y: 20 }
  print(a.x)
  print(a.y)

  arr = [1, 2, 3]
  for i in arr:
    print(i)

  print("Arena test passed if no crash!")
