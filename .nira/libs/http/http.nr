# http library for Nira

_routes = []

app():
  return {
    routes: _routes,
    get: _app_get,
    post: _app_post,
    put: _app_put,
    delete: _app_delete,
    listen: _app_listen
  }

_app_get(path, handler):
  _routes.push({ method: "GET", path: path, handler: handler })

_app_post(path, handler):
  _routes.push({ method: "POST", path: path, handler: handler })

_app_put(path, handler):
  _routes.push({ method: "PUT", path: path, handler: handler })

_app_delete(path, handler):
  _routes.push({ method: "DELETE", path: path, handler: handler })

_app_listen(port):
  __builtin_http_serve(port, _routes)
