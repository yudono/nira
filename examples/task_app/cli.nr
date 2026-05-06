import "task"

run args:
  cmd = args[1]
  
  if cmd == "add":
    title = args[2]
    task.create(title)
    print("Task added: " + title)
  
  if cmd == "list":
    items = task.all()
    print("--- Tasks ---")
    for t in items:
      status = "[ ]"
      if t.done:
        status = "[x]"
      print(status + " " + t.id + ": " + t.title)
  
  if cmd == "done":
    id = args[2]
    task.markDone(id)
    print("Task " + id + " marked as done")
  
  if cmd == "delete":
    id = args[2]
    task.delete(id)
    print("Task " + id + " deleted")
