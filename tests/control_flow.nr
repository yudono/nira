import math as m

test_logic:
  print("Logic Test:")
  a = true
  b = false
  
  if a and not b:
    print("a and not b is true")
  else:
    print("logic failed")
    
  if b or a:
    print("b or a is true")

test_while:
  print("While Test:")
  i = 0
  while i < 10:
    i = i + 1
    if i == 5:
      continue
    if i == 8:
      break
    print(i)
  print("done while")

test_null_error:
  print("Null/Error Test:")
  val = null
  if val == null:
    print("val is null")
    
  # Simulate error handling
  res = error("not found")
  print(res)

export main:
  test_logic()
  test_while()
  test_null_error()
  
  # pass keyword
  if true:
    pass
  else:
    print("never")
