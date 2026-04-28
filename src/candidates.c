#include "candidates.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void candidates_free(IndexCandidateList *list) {
    if(!list) return;
    for(int i=0; i<list->n; i++){
        free(list->items[i].schemaname);
        free(list->items[i].relname);
        free(list->items[i].create_sql);
    }
    free(list->items);
    free(list);
}

IndexCandidateList *candidates_from_qualstats(DB *db, long min_occurrences){
    IndexCandidateList *out = calloc(1, sizeof(*out));
    if(!db_has_extension(db, "pg_qualstats")){
        LOG_W("pg_qualstats not installed; no candidates generated");
        return out;
    }
    const char *sql =
    "WITH q AS ( "
    "SELECT "
    "    left_schema AS nspname, "
    "    left_table  AS relname, "
    "    array_agg(DISTINCT left_column ORDER BY left_column) AS cols, "
    "    sum(occurences)::bigint AS occ "
    "FROM pg_qualstats_pretty "
    "WHERE left_table IS NOT NULL "
    "GROUP BY left_schema, left_table "
    "HAVING sum(occurences) >= $1::bigint "
    ") "
    "SELECT * "
    "FROM q "
    "ORDER BY occ DESC;";
    char param[32];
    snprintf(param, sizeof(param), "%ld", min_occurrences);
    const char *params[1] = {param};

    PGresult *res=PQexecParams(db->conn, sql, 1, NULL, params, NULL, NULL, 0);
    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        LOG_W("pg_qualstats query failed (likely older version): %s", PQerrorMessage(db->conn));
        PQclear(res);
        return out;
    }

    int n = PQntuples(res);
    LOG_I("pg_qualstats returned %d candidate indexes", n);
    out->items = calloc(n, sizeof(IndexCandidate));

    for(int i = 0; i < n; i++){
        const char *schema  = PQgetvalue(res, i, 0);
        const char *rel = PQgetvalue(res, i, 1);
        const char *cols_pg = PQgetvalue(res, i, 2); 
        long occ = atol(PQgetvalue(res, i, 3));
        char cols[1024] = {0};
        int co = 0;
        
        for (const char *p = cols_pg; *p && co < (int)sizeof(cols) - 2; p++) {
            if (*p=='{' || *p=='}' || *p=='"') continue;
            if (*p==',') {cols[co++]=','; cols[co++]=' ';continue;}
            cols[co++] = *p;
        }

        char create_sql[2048];
        snprintf(create_sql, sizeof(create_sql),"CREATE INDEX ON \"%s\".\"%s\" (%s)", schema, rel, cols);

        IndexCandidate *c = &out->items[out->n++];
        c->schemaname= strdup(schema);
        c->relname= strdup(rel);
        c->create_sql= strdup(create_sql);
        c->qual_occurrences = occ;
    }
    PQclear(res);
    LOG_I("generated %d candidate indexes from pg_qualstats", out->n);
    return out;
}
