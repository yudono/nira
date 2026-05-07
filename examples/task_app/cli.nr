import "task"
import "sys"

run args:
  cmd = ""
  arg_offset = 0
  
  # Determine if we are running via nira interpreter or compiled binary
  if args[0] == "./nira" or args[0] == "nira":
    cmd = args[3]
    arg_offset = 3
  else:
    cmd = args[1]
    arg_offset = 1
  
  if cmd == "add":
    title = args[arg_offset + 1]
    task.create(title)
    print("Task added: " + title)
  
  if cmd == "list":
    items = task.all()
    print("--- Tasks ---")
    for t in items:
      status = "[ ]"
      if t.done:
        status = "[x]"
      print(status + " " + toString(t.id) + ": " + t.title)
  
  if cmd == "done":
    id = args[arg_offset + 1]
    # Update needs title, so we need to fetch the task first or just pass empty string if the update query handles it
    t = task.find(id)
    if len(t) > 0:
      task.update(id, t[0].title, 1)
      print("Task " + id + " marked as done")
  
  if cmd == "delete":
    id = args[arg_offset + 1]
    task.delete(id)
    print("Task " + id + " deleted")
