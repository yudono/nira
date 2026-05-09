open(path):
  conn = __builtin_sqlite3_open(path)
  return {
    _conn: conn
    exec: fn(sql, params):
      return __builtin_sqlite3_exec(conn, sql, params or [])
    query: fn(sql, params):
      return __builtin_sqlite3_query(conn, sql, params or [])
    close: fn():
      return __builtin_sqlite3_close(conn)
  }

exec(conn, sql, params):
  return __builtin_sqlite3_exec(conn, sql, params or [])

query(conn, sql, params):
  return __builtin_sqlite3_query(conn, sql, params or [])

close(conn):
  return __builtin_sqlite3_close(conn)
