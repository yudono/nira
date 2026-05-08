# Minimal Binary Search Tree (BST) Implementation
# Tests recursion with nested objects.

create_node(val):
  return {
    val: val,
    left: null,
    right: null
  }

insert(root, val):
  if root == null:
    return create_node(val)
  
  if val < root.val:
    root.left = insert(root.left, val)
  else:
    root.right = insert(root.right, val)
  
  return root

search(root, val):
  if root == null:
    return false
  if root.val == val:
    return true
  
  if val < root.val:
    return search(root.left, val)
  else:
    return search(root.right, val)

inorder(root):
  if root == null:
    return
  
  inorder(root.left)
  print("Node: " + toString(root.val))
  inorder(root.right)

main():
  print("=== MIN BST TEST ===")
  
  root = null
  values = [50, 30, 20, 40, 70, 60, 80]
  
  print("Inserting values...")
  for v in values:
    root = insert(root, v)
  
  print("Inorder traversal:")
  inorder(root)
  
  print("Search Tests:")
  print("Search 40: " + toString(search(root, 40)))
  print("Search 90: " + toString(search(root, 90)))
  print("Search 20: " + toString(search(root, 20)))
  
  print("=== MIN BST TEST PASSED ===")
