native:
  link "-lsqlite3"
  header "<sqlite3.h>"
  code: "
    static void bind_val(sqlite3_stmt* stmt, int idx, Value v) {
      if (v.type == VAL_INT || v.type == VAL_BOOL) sqlite3_bind_int(stmt, idx, v.data.i);
      else if (v.type == VAL_STR) sqlite3_bind_text(stmt, idx, v.data.s, -1, SQLITE_TRANSIENT);
      else if (v.type == VAL_NIL) sqlite3_bind_null(stmt, idx);
    }
    Value __native_sqlite3_open(Value path) { return path; }
    Value __native_sqlite3_exec(Value db, Value sql, Value params) {
      if (sql.type != VAL_STR || db.type != VAL_STR) return val_int(0);
      sqlite3* h; if (sqlite3_open(db.data.s, &h) != SQLITE_OK) return val_int(0);
      sqlite3_stmt* stmt; if (sqlite3_prepare_v2(h, sql.data.s, -1, &stmt, 0) == SQLITE_OK) {
        if (params.type == VAL_ARR) { for (int i=0; i<params.data.arr->count; i++) bind_val(stmt, i+1, *params.data.arr->elements[i]); }
        sqlite3_step(stmt); sqlite3_finalize(stmt);
      }
      sqlite3_close(h); return val_int(1);
    }
    Value __native_sqlite3_query(Value db, Value sql, Value params) {
      Value a = val_arr(); if (sql.type != VAL_STR || db.type != VAL_STR) return a;
      sqlite3* h; if (sqlite3_open(db.data.s, &h) != SQLITE_OK) return a;
      sqlite3_stmt* stmt; if (sqlite3_prepare_v2(h, sql.data.s, -1, &stmt, 0) == SQLITE_OK) {
        if (params.type == VAL_ARR) { for (int i=0; i<params.data.arr->count; i++) bind_val(stmt, i+1, *params.data.arr->elements[i]); }
        int cols = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
          Value o = val_obj();
          for (int i=0; i<cols; i++) {
            const char* name = sqlite3_column_name(stmt, i); int type = sqlite3_column_type(stmt, i);
            Value v = val_nil();
            if (type == SQLITE_INTEGER) v = val_int(sqlite3_column_int(stmt, i));
            else if (type == SQLITE_NULL) v = val_nil();
            else v = val_str((char*)sqlite3_column_text(stmt, i));
            set_field(o, name, v);
          }
          nr_rt_push(a, o);
        }
        sqlite3_finalize(stmt);
      }
      sqlite3_close(h); return a;
    }
  "

open path:
  return __native_sqlite3_open(path)

exec db, sql, params:
  return __native_sqlite3_exec(db, sql, params or [])

query db, sql, params:
  return __native_sqlite3_query(db, sql, params or [])
