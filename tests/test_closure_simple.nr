make_counter(start):
  count = start
  return {
    increment: fn():
      count = count + 1
      return count
    get: fn():
      return count
  }

main:
  print("=== Minimal Closure Test ===")
  c = make_counter(10)
  print("Counter created")
  val = c.get()
  print("Initial value: " + toString(val))
  c.increment()
  print("After increment: " + toString(c.get()))
  
  if c.get() == 11:
    print("RESULT: PASSED")
  else:
    print("RESULT: FAILED (expected 11, got " + toString(c.get()) + ")")
