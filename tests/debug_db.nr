import "db"
_db = db.open("tasks.db")
items = _db.query("SELECT * FROM tasks")
for t in items:
  print("Keys for task:")
  # We don't have a way to list keys in Nira yet, but we can try common ones
  print("id: " + t.id)
  print("ID: " + t.ID)
  print("title: " + t.title)
  print("done: " + t.done)
