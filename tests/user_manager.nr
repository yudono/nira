# User Management Test
# Specifically testing function calls with object literals using parentheses.

create_user data:
  return {
    id: data.id,
    name: data.name,
    role: data.role or "user",
    status: "active"
  }

update_status user, new_status:
  user.status = new_status
  return user

print_user user:
  print("User [" + toString(user.id) + "]: " + toString(user.name) + " (" + toString(user.role) + ") - " + toString(user.status))

main:
  print("=== USER MANAGER TEST ===")
  
  # Call with object literal using parentheses
  user1 = create_user({
    id: 1,
    name: "Budi",
    role: "admin"
  })
  
  user2 = create_user({
    id: 2,
    name: "Ani"
  })
  
  print_user(user1)
  print_user(user2)
  
  print("Updating Ani's status...")
  update_status(user2, "suspended")
  print_user(user2)
  
  # List of objects
  users = [user1, user2]
  print("Total users: " + toString(len(users)))
  
  for u in users:
    if u.role == "admin":
      print("Admin found: " + toString(u.name))
  
  print("=== USER MANAGER TEST PASSED ===")
