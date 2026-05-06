# db library for Nira (JSON-file persistence)

_handle = null

open path:
  _handle = __builtin_db_open(path)
  
  return {
    handle: _handle,
    exec: (sql, params) -> __builtin_db_exec(_handle, sql, params or []),
    query: (sql, params) -> __builtin_db_query(_handle, sql, params or [])
  }
