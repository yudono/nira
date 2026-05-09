#include "evaluator.h"
#include <sqlite3.h>

    #include <sqlite3.h>
    #include <string.h>

    Value nira_sqlite3_open(Value self, Value path, Value _v2, Value _v3, Value _v4, Value _v5) {
        if (path.type != VAL_STR) return val_nil();
        sqlite3 *db;
        if (sqlite3_open(path.data.s, &db) != SQLITE_OK) return val_nil();
        return val_int((long long)db);
    }

    Value nira_sqlite3_close(Value self, Value conn, Value _v2, Value _v3, Value _v4, Value _v5) {
        if (conn.type != VAL_INT) return val_nil();
        sqlite3_close((sqlite3 *)conn.data.i);
        return val_nil();
    }

    Value nira_sqlite3_exec(Value self, Value conn, Value sql, Value params, Value _v4, Value _v5) {
        if (conn.type != VAL_INT || sql.type != VAL_STR) return val_nil();
        sqlite3 *db = (sqlite3 *)conn.data.i;
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql.data.s, -1, &stmt, NULL) != SQLITE_OK)
            return val_error((char *)sqlite3_errmsg(db));

        if (params.type == VAL_ARR) {
            for (int i = 0; i < params.data.arr->count; i++) {
                Value p = *params.data.arr->elements[i];
                if (p.type == VAL_INT) sqlite3_bind_int64(stmt, i + 1, p.data.i);
                else if (p.type == VAL_FLOAT) sqlite3_bind_double(stmt, i + 1, p.data.f);
                else if (p.type == VAL_STR) sqlite3_bind_text(stmt, i + 1, p.data.s, -1, SQLITE_TRANSIENT);
                else if (p.type == VAL_NIL) sqlite3_bind_null(stmt, i + 1);
            }
        }
        int res = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return val_bool(res == SQLITE_DONE || res == SQLITE_OK);
    }

    Value nira_sqlite3_query(Value self, Value conn, Value sql, Value params, Value _v4, Value _v5) {
        if (conn.type != VAL_INT || sql.type != VAL_STR) return val_nil();
        sqlite3 *db = (sqlite3 *)conn.data.i;
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql.data.s, -1, &stmt, NULL) != SQLITE_OK)
            return val_error((char *)sqlite3_errmsg(db));

        if (params.type == VAL_ARR) {
            for (int i = 0; i < params.data.arr->count; i++) {
                Value p = *params.data.arr->elements[i];
                if (p.type == VAL_INT) sqlite3_bind_int64(stmt, i + 1, p.data.i);
                else if (p.type == VAL_FLOAT) sqlite3_bind_double(stmt, i + 1, p.data.f);
                else if (p.type == VAL_STR) sqlite3_bind_text(stmt, i + 1, p.data.s, -1, SQLITE_TRANSIENT);
                else if (p.type == VAL_NIL) sqlite3_bind_null(stmt, i + 1);
            }
        }

        Value res_arr = val_arr();
        int cols = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Value row = val_obj();
            for (int i = 0; i < cols; i++) {
                const char *name = sqlite3_column_name(stmt, i);
                int type = sqlite3_column_type(stmt, i);
                Value val;
                if (type == SQLITE_INTEGER) val = val_int(sqlite3_column_int64(stmt, i));
                else if (type == SQLITE_FLOAT) val = val_float(sqlite3_column_double(stmt, i));
                else if (type == SQLITE_TEXT) val = val_str(nr_strdup((const char *)sqlite3_column_text(stmt, i)));
                else val = val_nil();
                set_field(row, name, val);
            }
            nr_rt_push(val_nil(), res_arr, row, val_nil(), val_nil(), val_nil());
        }
        sqlite3_finalize(stmt);
        return res_arr;
    }
  
