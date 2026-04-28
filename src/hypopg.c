#include "hypopg.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int hypopg_available(DB *db) {
    return db_has_extension(db, "hypopg");
}

int hypopg_reset(DB *db) {
    return db_exec_cmd(db, "SELECT hypopg_reset()");
}

int hypopg_create(DB *db, const char *create_index_sql,char *name_out, size_t name_out_size) {
    char *escaped = PQescapeLiteral(db->conn, create_index_sql,strlen(create_index_sql));
    if (!escaped) return -1;

    size_t buflen = strlen(escaped) + 64;
    char *sql = malloc(buflen);
    snprintf(sql, buflen,
             "SELECT indexname FROM hypopg_create_index(%s)", escaped);
    PQfreemem(escaped);

    PGresult *res = db_exec(db, sql);
    free(sql);
    if (!res) return -1;

    int rc = -1;
    if (PQntuples(res) > 0 && !PQgetisnull(res, 0, 0)) {
        snprintf(name_out, name_out_size, "%s", PQgetvalue(res, 0, 0));
        rc = 0;
    }
    PQclear(res);
    return rc;
}

/* Find "(cost=A..B rows=...)" in the first plan line; return B. */
static int parse_total_cost(const char *plan_line, double *out) {
    //print plan line
    
    const char *p = strstr(plan_line, "cost=");
    if (!p) return -1;
    p += 5;
    LOG_I("plan line: %s", p);
    /* skip start cost */
    while (*p && *(p+1) && (*p != '.' || *(p+1) != '.')) p++;
    if (*p != '.' || *(p+1) != '.') return -1;
    p += 2;
    char *end;
    double v = strtod(p, &end);
    if (end == p) return -1;
    *out = v;
    return 0;
}

int explain_total_cost(DB *db, const char *query, double *out) {

    int has_params = strchr(query, '$') != NULL;

    PGresult *res = NULL;
    if (has_params) {
      
        db_exec_cmd(db, "BEGIN");                                             
        if (db_exec_cmd(db, "SET LOCAL plan_cache_mode = force_generic_plan") != 0){
           
            return -1;
        }
            
        if (db_exec_cmd(db, "DEALLOCATE ALL") != 0) { /* tolerate failure */ }

        size_t qlen = strlen(query);
        char *prep = malloc(qlen + 64);
        snprintf(prep, qlen + 64, "PREPARE pgai_q AS %s", query);
        if (db_exec_cmd(db, prep) != 0) { free(prep);db_exec_cmd(db, "ROLLBACK"); return -1; }
        free(prep);

        /* Build EXECUTE pgai_q (NULL, NULL, ...). Count parameters first.  */
        int max_param = 0;
        for (const char *p = query; *p; p++) {
            if (*p == '$') {
                char *e;
                long n = strtol(p + 1, &e, 10);
                if (e != p + 1 && n > max_param) max_param = (int)n;
                p = e ? e - 1 : p;
            }
        }
        char nulls[2048] = {0};
        int off = 0;
        for (int i = 0; i < max_param; i++) {
            off += snprintf(nulls + off, sizeof(nulls) - off,
                            "%sNULL", i ? "," : "");
        }
        char exec_sql[4096];
        if (max_param > 0)
            snprintf(exec_sql, sizeof(exec_sql),
                     "EXPLAIN EXECUTE pgai_q(%s)", nulls);
        else
            snprintf(exec_sql, sizeof(exec_sql), "EXPLAIN EXECUTE pgai_q");

        res = db_exec(db, exec_sql);
        db_exec_cmd(db, "DEALLOCATE pgai_q");
        db_exec_cmd(db, "COMMIT");
    } else {
        size_t len = strlen(query) + 32;
        char *sql = malloc(len);
        snprintf(sql, len, "EXPLAIN %s", query);
        res = db_exec(db, sql);
        free(sql);
    }

    
    if (!res){
         return -1;
    } 
    int rc = -1;
    if (PQntuples(res) > 0)
        rc = parse_total_cost(PQgetvalue(res, 0, 0), out);
    PQclear(res);
    return rc;
}

int hypopg_estimated_size(DB *db, const char *hypo_index_name, long *bytes_out) {
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT hypopg_relation_size(indexrelid) "
             "FROM   hypopg() "
             "WHERE  hypopg.indexname = '%s'", hypo_index_name);
    return db_scalar_long(db, sql, bytes_out);
}
