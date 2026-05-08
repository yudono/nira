# ============================================
# Test 6: Closures, Scope & Composition
# ============================================
# Tests closure capture, scope isolation,
# function composition, and method patterns.

# --- Counter factory (closure test) ---
make_counter(start):
  count = start
  return {
    increment: fn():
      count = count + 1
      return count
    get: fn():
      return count
    reset: fn():
      count = start
      return count
  }

# --- Adder factory ---
make_adder(n):
  return fn(x):
    return x + n

# --- Multiplier factory ---
make_multiplier(factor):
  return fn(x):
    return x * factor

# --- Compose two functions ---
compose(f, g):
  return fn(x):
    return f(g(x))

# --- Object with methods (composition pattern) ---
createAnimal(name, sound):
  return {
    name: name,
    sound: sound,
    speak: fn():
      return self.name + " says " + self.sound
    rename: fn(new_name):
      self.name = new_name
      return self
  }

# --- Pipeline pattern ---
apply_all(fns, value):
  result = value
  for f in fns:
    result = f(result)
  return result

export main():
  print("=== STRESS: Closures & Scope ===")

  # --- Counter closure ---
  print("Counter Closure:")
  counter = make_counter(0)
  print("initial: " + counter.get())
  counter.increment()
  counter.increment()
  counter.increment()
  print("after 3 increments: " + counter.get())
  counter.reset()
  print("after reset: " + counter.get())

  # Independent counters
  c1 = make_counter(10)
  c2 = make_counter(100)
  c1.increment()
  c2.increment()
  c2.increment()
  print("c1: " + c1.get())
  print("c2: " + c2.get())

  # --- Adder / Multiplier closures ---
  print("")
  print("Factory Closures:")
  add5 = make_adder(5)
  add10 = make_adder(10)
  print("add5(3) = " + add5(3))
  print("add10(3) = " + add10(3))

  double = make_multiplier(2)
  triple = make_multiplier(3)
  print("double(7) = " + double(7))
  print("triple(7) = " + triple(7))

  # --- Composition ---
  print("")
  print("Function Composition:")
  add1 = make_adder(1)
  times2 = make_multiplier(2)
  add1_then_double = compose(times2, add1)
  print("(5 + 1) * 2 = " + add1_then_double(5))

  double_then_add1 = compose(add1, times2)
  print("(5 * 2) + 1 = " + double_then_add1(5))

  # --- Pipeline ---
  print("")
  print("Pipeline Pattern:")
  pipeline = [make_adder(10), make_multiplier(2), make_adder(3)]
  result = apply_all(pipeline, 5)
  print("(5 + 10) * 2 + 3 = " + result)

  # --- Scope isolation ---
  print("")
  print("Scope Isolation:")
  x = "outer"
  if true:
    y = "inner"
    print("inside block: x=" + x)
  print("outside block: x=" + x)

  # --- Object methods (self binding) ---
  print("")
  print("Object Methods (self):")
  cat = createAnimal("Cat", "Meow")
  dog = createAnimal("Dog", "Woof")
  print(cat.speak())
  print(dog.speak())

  cat.rename("Kitty")
  print("After rename: " + cat.speak())

  # --- Many closures ---
  print("")
  print("Many Closures Stress:")
  adders = []
  i = 0
  while i < 50:
    adders.push(make_adder(i))
    i = i + 1
  # NOTE: array[index](args) not supported, assign to var first
  fn0 = adders[0]
  fn25 = adders[25]
  fn49 = adders[49]
  print("adders[0](100) = " + fn0(100))
  print("adders[25](100) = " + fn25(100))
  print("adders[49](100) = " + fn49(100))

  # --- Nested closures ---
  print("")
  print("Nested Closure:")
  outer_val = 10
  make_inner(a):
    return fn(b):
      return fn(c):
        return a + b + c + outer_val
  nested = make_inner(1)
  nested2 = nested(2)
  result2 = nested2(3)
  print("1 + 2 + 3 + 10 = " + result2)

  print("")
  print("=== Closures & Scope stress PASSED ===")
