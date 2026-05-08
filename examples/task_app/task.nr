import "sqlite3"
import "sys"

_db = null

prepare():
  if _db == null:
    _db = sqlite3.open("tasks.db")
    sqlite3.exec(_db, "CREATE TABLE IF NOT EXISTS tasks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, done BOOLEAN, created_at INTEGER)")

all():
  prepare()
  return sqlite3.query(_db, "SELECT * FROM tasks")

find(id):
  prepare()
  return sqlite3.query(_db, "SELECT * FROM tasks WHERE id = ?", [toInt(id)])

create(title):
  prepare()
  sqlite3.exec(_db, "INSERT INTO tasks (title, done, created_at) VALUES (?, 0, ?)", [title, __builtin_time_now()])

update(id, title, done):
  prepare()
  sqlite3.exec(_db, "UPDATE tasks SET title = ?, done = ? WHERE id = ?", [title, done, toInt(id)])

delete(id):
  prepare()
  sqlite3.exec(_db, "DELETE FROM tasks WHERE id = ?", [toInt(id)])
