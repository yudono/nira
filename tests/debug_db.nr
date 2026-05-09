import sqlite3 as db
_db = db.open("tasks.db")
print("DB instance:")
print(_db)
items = _db.query("SELECT * FROM tasks")
for t in items:
  print("Keys for task: ")
  ks = __builtin_keys(t)
  for k in ks:
    print("key: " + k)
    print(k + ": " + t[k])
