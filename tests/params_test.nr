add(a, b):
  return a + b

greet(name, greeting="Hello"):
  return greeting + " " + name

sum(...nums):
  total = 0
  for n in nums:
    total = total + n
  return total

main():
  println("Main started")
  # Test normal
  println("add(5, 10) =", add(5, 10))
  
  # Test optional
  println("greet(\"Budi\") =", greet("Budi"))
  println("greet(\"Budi\", \"Hi\") =", greet("Budi", "Hi"))
  
  # Test variadic
  println("sum(1, 2, 3, 4) =", sum(1, 2, 3, 4))
  
  # Test named
  println("add(b=20, a=5) =", add(b=20, a=5))
  println("greet(greeting=\"Halo\", name=\"Agus\") =", greet(greeting="Halo", name="Agus"))
