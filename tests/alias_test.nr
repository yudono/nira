export main():
  import math as m

  print("Namespace Test:")
  print(m.max(5, 1))

  # Destructuring Alias Test
  user = {
    id: 1,
    name: "Budi"
  }

  { name as userName, id } = user
  print("Destructuring Test:")
  print(userName)
  print(id)

  # Loop Alias Test
  users = [
    { name: "Siti" },
    { name: "Agus" }
  ]

  print("Loop Alias Test:")
  for item as u in users:
    print(u.name)

  # Method Call Sugar Test
  getName u:
    return u.name

  print("Method Sugar Test:")
  print(user.getName())

  # Standalone Alias Test
  net = m
  print("Standalone Alias Test:")
  print(net.max(100, 200))
