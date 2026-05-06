import "http"
import "task"

main:
  app = http.app()

  app.get("/tasks", get_tasks)
  app.get("/tasks/:id", get_task)
  app.post("/tasks", create_task)
  app.put("/tasks/:id", update_task)
  app.delete("/tasks/:id", delete_task)

  print("Server running on http://localhost:3000")
  app.listen(3000)

get_tasks ctx:
  return ctx.json(task.all())

get_task ctx:
  id = ctx.params.id
  t = task.find(id)
  if t != null:
    return ctx.json(t)
  return { status: 404, body: "Task not found" }

create_task ctx:
  data = json_decode(ctx.body)
  task.create(data.title)
  return ctx.json({ ok: true })

update_task ctx:
  id = ctx.params.id
  data = json_decode(ctx.body)
  task.update(id, data.title, data.done)
  return ctx.json({ ok: true })

delete_task ctx:
  id = ctx.params.id
  task.delete(id)
  return ctx.json({ ok: true })
