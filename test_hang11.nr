import "task"
import "sys"

run args:
  if len(t) > 0:
    task.update(id, t[0].title, 1)

