# http library for Nira

import "string"

_routes = []
_serve = null

app:
  _serve = serve
  return {
    routes: _routes,
    get: (path, handler) -> _routes.push({ method: "GET", path: path, handler: handler }),
    post: (path, handler) -> _routes.push({ method: "POST", path: path, handler: handler }),
    put: (path, handler) -> _routes.push({ method: "PUT", path: path, handler: handler }),
    delete: (path, handler) -> _routes.push({ method: "DELETE", path: path, handler: handler }),
    listen: (port) -> __builtin_http_serve(port, _serve)
  }

serve req:
  for r in _routes:
    if r.method == req.method and match_path(r.path, req.path):
      ctx = create_ctx(req, r.path)
      return r.handler(ctx)
  
  return { status: 404, body: "Not Found" }

match_path pattern actual_path:
  if pattern == actual_path:
    return true
    
  colon_idx = string.indexOf(pattern, ":")
  if colon_idx > 0:
    pattern_base = pattern.substring(0, colon_idx)
    if len(actual_path) >= colon_idx:
      path_base = actual_path.substring(0, colon_idx)
      if pattern_base == path_base:
        return true
        
  return false

create_ctx req pattern:
  params = {}
  actual_path = req.path
  
  colon_idx = string.indexOf(pattern, ":")
  if colon_idx > 0:
    # Extract value after the colon position
    params.id = actual_path.substring(colon_idx, 10)
    
  return {
    method: req.method,
    path: req.path,
    params: params,
    body: req.body,
    
    json: (data) -> {
      status: 200,
      headers: { "Content-Type": "application/json" },
      body: __builtin_json_encode(data)
    },
    
    text: (data) -> {
      status: 200,
      headers: { "Content-Type": "text/plain" },
      body: data
    }
  }
