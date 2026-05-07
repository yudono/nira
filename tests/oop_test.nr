# OOP / Method Sharing Test for Nira

# Hoisted functions (Method templates)
getName:
  return self.name

greet msg:
  return msg + " " + self.name

# Factory function with closure
createUser name, age:
  return {
    name: name
    age: age
    # Method references (Shared from global)
    getName: getName
    greet: greet
    # Inline closure (Unique per instance)
    getInfo: fn:
      return self.name + " is " + self.age
  }

main:
  print("--- Testing Method Sharing ---")
  u1 = createUser("Budi", 25)
  u2 = createUser("Ani", 22)

  print(u1.getName())    # Budi
  print(u2.getName())    # Ani

  print(u1.greet("Hi"))  # Hi Budi
  print(u2.greet("Hello")) # Hello Ani

  print("--- Testing Closure / getInfo ---")
  print(u1.getInfo())    # Budi is 25
  print(u2.getInfo())    # Ani is 22

  print("--- Testing Mutation ---")
  u1.name = "Budi Santoso"
  print(u1.getName())    # Budi Santoso
  print(u2.getName())    # Ani (should remain Ani)

  print("--- OOP tests completed ---")
