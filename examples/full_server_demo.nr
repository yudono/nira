import http
import file
import json

_tasks = [
  { id: 1, title: "Learn Nira Language", done: false },
  { id: 2, title: "Master C Backend", done: true },
  { id: 3, title: "Build Premium Web App", done: false }
]
_next_id = 4

handle_index req:
  index_content = file.read("examples/index.html")
  return { status: 200, body: index_content }

handle_get_tasks req:
  return { status: 200, body: __builtin_json_encode(_tasks), content_type: "application/json" }

handle_create_task req:
  data = __builtin_json_parse(req.body)
  new_task = { 
    id: _next_id, 
    title: data.title, 
    done: false 
  }
  _tasks.push(new_task)
  _next_id = _next_id + 1
  return { status: 200, body: __builtin_json_encode(new_task), content_type: "application/json" }

handle_delete_task req:
  id_str = req.params.id
  target_id = toInt(id_str)
  new_list = []
  for t in _tasks:
    if t.id != target_id:
      new_list.push(t)
  _tasks = new_list
  return { status: 200, body: __builtin_json_encode({ ok: true }), content_type: "application/json" }

main:
  println("🚀 Nira Task Manager starting...")
  app = http.app()
  
  app.get("/", handle_index)
  app.get("/api/tasks", handle_get_tasks)
  app.post("/api/tasks", handle_create_task)
  app.delete("/api/tasks/:id", handle_delete_task)

  println("✨ Server ready at http://localhost:3000")
  app.listen(3000)
