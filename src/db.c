#include "db.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DB *db_connect(const char *conninfo) {
    PGconn *conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        LOG_E("connection failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }
    PGresult *r = PQexec(conn, "SET application_name = 'pg_autoindex'");
    PQclear(r);
    DB *db = calloc(1, sizeof(DB));
    db->conn = conn;
    return db;
}

void db_close(DB *db) {
    if (!db) return;
    if (db->conn) PQfinish(db->conn);
    free(db);
}

PGresult *db_exec(DB *db, const char *sql) {
    PGresult *res = PQexec(db->conn, sql);
    ExecStatusType st = PQresultStatus(res);
    if (st != PGRES_TUPLES_OK && st != PGRES_COMMAND_OK) {
        LOG_E("query failed: %s | sql: %.200s",
              PQerrorMessage(db->conn), sql);
        PQclear(res);
        return NULL;
    }
    return res;
}

int db_exec_cmd(DB *db, const char *sql) {
    PGresult *r = db_exec(db, sql);
    if (!r) return -1;
    PQclear(r);
    return 0;
}

int db_scalar_double(DB *db, const char *sql, double *out) {
    PGresult *r = db_exec(db, sql);
    if (!r) return -1;
    int rc = -1;
    if (PQntuples(r) > 0 && PQnfields(r) > 0 && !PQgetisnull(r, 0, 0)) {
        *out = strtod(PQgetvalue(r, 0, 0), NULL);
        rc = 0;
    }
    PQclear(r);
    return rc;
}

int db_scalar_long(DB *db, const char *sql, long *out) {
    PGresult *r = db_exec(db, sql);
    if (!r) return -1;
    int rc = -1;
    if (PQntuples(r) > 0 && PQnfields(r) > 0 && !PQgetisnull(r, 0, 0)) {
        *out = strtol(PQgetvalue(r, 0, 0), NULL, 10);
        rc = 0;
    }
    PQclear(r);
    return rc;
}

int db_has_extension(DB *db, const char *ext_name) {
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT 1 FROM pg_extension WHERE extname = '%s'", ext_name);
    PGresult *r = db_exec(db, sql);
    if (!r) return 0;
    int found = PQntuples(r) > 0;
    PQclear(r);
    return found;
}
