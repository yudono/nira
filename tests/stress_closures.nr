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
  print("Creating counter...")
  counter = make_counter(0)
  print("Counter created.")
  # Nira toString conversion test (now that I fixed builtins)
  print("initial: " + toString(counter.get()))
  counter.increment()
  counter.increment()
  counter.increment()
  print("after 3 increments: " + toString(counter.get()))
  counter.reset()
  print("after reset: " + toString(counter.get()))

  # Independent counters
  c1 = make_counter(10)
  c2 = make_counter(100)
  c1.increment()
  c2.increment()
  c2.increment()
  print("c1: " + toString(c1.get()))
  print("c2: " + toString(c2.get()))

  print("=== Closures & Scope stress PASSED ===")
