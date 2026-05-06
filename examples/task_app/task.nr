import "db"
import "sys"

_db = null

prepare:
  if _db == null:
    _db = db.open("tasks.db")
    _db.exec("CREATE TABLE IF NOT EXISTS tasks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, done BOOLEAN, created_at INTEGER)")

all:
  prepare()
  return _db.query("SELECT * FROM tasks")

find id:
  prepare()
  return _db.query("SELECT * FROM tasks WHERE id = ?", [toInt(id)])

create title:
  prepare()
  _db.exec("INSERT INTO tasks (title, done, created_at) VALUES (?, 0, ?)", [title, __builtin_time_now()])

update id, title, done:
  prepare()
  _db.exec("UPDATE tasks SET title = ?, done = ? WHERE id = ?", [title, done, toInt(id)])

delete id:
  prepare()
  _db.exec("DELETE FROM tasks WHERE id = ?", [toInt(id)])
