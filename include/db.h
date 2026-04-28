#ifndef PGAI_DB_H
#define PGAI_DB_H

#include <libpq-fe.h>

typedef struct {
    PGconn *conn;
} DB;

DB        *db_connect(const char *conninfo);
void       db_close(DB *db);

/* Caller must PQclear() the returned PGresult. NULL on failure. */
PGresult  *db_exec(DB *db, const char *sql);

/* Convenience: run a command, discard result. 0 on success, -1 on failure. */
int        db_exec_cmd(DB *db, const char *sql);

/* Single scalar getter; returns 0 if value present and parsed, -1 otherwise.
   Out param `out` is filled with strtod() of the (0,0) cell.                */
int        db_scalar_double(DB *db, const char *sql, double *out);
int        db_scalar_long  (DB *db, const char *sql, long   *out);

/* Returns whether the named extension is installed. */
int        db_has_extension(DB *db, const char *ext_name);

#endif
