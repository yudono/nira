import "object"

main:
  user = { name: "Budi", age: 25 }
  print("User keys:")
  keys = object.keys(user)
  for k in keys:
    print(k)
    
  print("\nTesting object.assign:")
  extra = { city: "Jakarta", job: "Developer" }
  object.assign(user, extra)
  
  new_keys = object.keys(user)
  for k in new_keys:
    print(k + ": " + user[k])

  print("\nMemory test (simulating high allocation)...")
  items = []
  for i in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]:
    items.push({ id: i, data: "some large string data to fill arena" })
  
  print("Allocated 10 objects successfully.")
