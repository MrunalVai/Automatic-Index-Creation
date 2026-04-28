#ifndef DB_H
#define DB_H

#include <libpq-fe.h>

typedef struct{
    PGconn *conn;
}DB;

DB *db_connect(const char *conninfo);
void db_close(DB *db);
PGresult*db_exec(DB *db, const char *sql);
int db_exec_cmd(DB *db, const char *sql);
int db_scalar_double(DB *db, const char *sql, double *out);
int db_scalar_long  (DB *db, const char *sql, long   *out);
int db_has_extension(DB *db, const char *ext_name);

#endif
