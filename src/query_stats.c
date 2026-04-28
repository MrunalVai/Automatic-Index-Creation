#include "query_stats.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== top queries ====================================================== */

QueryStatList *query_stats_top(DB *db, int top_n, long min_calls) {
    char sql[2048];
    snprintf(sql, sizeof(sql),
        "SELECT queryid::text, "
        "       query, "
        "       calls, "
        "       total_exec_time, "
        "       mean_exec_time "
        "FROM   pg_stat_statements "
        "WHERE  calls >= %ld "
        "  AND  query ILIKE 'select%%' "
        "  AND  query NOT ILIKE '%%pg_stat_statements%%' "
        "  AND  query NOT ILIKE '%%hypopg%%' "
        "ORDER BY total_exec_time DESC "
        "LIMIT %d",
        min_calls, top_n);

    PGresult *res = db_exec(db, sql);
    if (!res) return NULL;

    int n = PQntuples(res);
    QueryStatList *list = calloc(1, sizeof(*list));
    list->items = calloc(n, sizeof(QueryStat));
    list->n = n;

    for (int i = 0; i < n; i++) {
        list->items[i].queryid        = strdup(PQgetvalue(res, i, 0));
        list->items[i].query_text     = strdup(PQgetvalue(res, i, 1));
        list->items[i].calls          = atol  (PQgetvalue(res, i, 2));
        list->items[i].total_exec_time= atof  (PQgetvalue(res, i, 3));
        list->items[i].mean_exec_time = atof  (PQgetvalue(res, i, 4));
    }
    PQclear(res);
    return list;
}

void query_stats_free(QueryStatList *list) {
    if (!list) return;
    for (int i = 0; i < list->n; i++) {
        free(list->items[i].queryid);
        free(list->items[i].query_text);
    }
    free(list->items);
    free(list);
}

/* ===== table stats ====================================================== */

TableStatList *table_stats_all(DB *db) {
    const char *sql =
        "SELECT schemaname, relname, "
        "       n_tup_ins, n_tup_upd, n_tup_del, n_tup_hot_upd, "
        "       n_live_tup, seq_scan, idx_scan "
        "FROM   pg_stat_user_tables";
    PGresult *res = db_exec(db, sql);
    if (!res) return NULL;

    int n = PQntuples(res);
    TableStatList *list = calloc(1, sizeof(*list));
    list->items = calloc(n, sizeof(TableStat));
    list->n = n;

    for (int i = 0; i < n; i++) {
        TableStat *t = &list->items[i];
        t->schemaname    = strdup(PQgetvalue(res, i, 0));
        t->relname       = strdup(PQgetvalue(res, i, 1));
        t->n_tup_ins     = atol  (PQgetvalue(res, i, 2));
        t->n_tup_upd     = atol  (PQgetvalue(res, i, 3));
        t->n_tup_del     = atol  (PQgetvalue(res, i, 4));
        t->n_tup_hot_upd = atol  (PQgetvalue(res, i, 5));
        t->n_live_tup    = atol  (PQgetvalue(res, i, 6));
        t->seq_scan      = atol  (PQgetvalue(res, i, 7));
        t->idx_scan      = atol  (PQgetvalue(res, i, 8));
    }
    PQclear(res);
    return list;
}

TableStat *table_stats_find(TableStatList *list,
                            const char *schema, const char *rel) {
    if (!list) return NULL;
    for (int i = 0; i < list->n; i++)
        if (!strcmp(list->items[i].schemaname, schema) &&
            !strcmp(list->items[i].relname,    rel))
            return &list->items[i];
    return NULL;
}

void table_stats_free(TableStatList *list) {
    if (!list) return;
    for (int i = 0; i < list->n; i++) {
        free(list->items[i].schemaname);
        free(list->items[i].relname);
    }
    free(list->items);
    free(list);
}

double table_hot_ratio(const TableStat *t) {
    if (!t || t->n_tup_upd == 0) return 0.0;
    double r = (double)t->n_tup_hot_upd / (double)t->n_tup_upd;
    return r < 0 ? 0 : (r > 1 ? 1 : r);
}

long table_index_writes(const TableStat *t) {
    if (!t) return 0;
    /* HOT updates skip index maintenance for indexes whose columns weren't
       changed. Without column-level info, treat HOT as "no index work" --
       Postgres already proved it. Non-HOT updates and inserts/deletes touch
       the indexes. */
    long non_hot_upd = t->n_tup_upd - t->n_tup_hot_upd;
    if (non_hot_upd < 0) non_hot_upd = 0;
    return t->n_tup_ins + non_hot_upd + t->n_tup_del;
}

/* ===== index stats ====================================================== */

IndexStatList *index_stats_all(DB *db) {
    const char *sql =
        "SELECT s.schemaname, s.relname, s.indexrelname, "
        "       s.idx_scan, s.idx_tup_read, s.idx_tup_fetch, "
        "       pg_relation_size(s.indexrelid) AS sz, "
        "       i.indisunique, i.indisprimary, "
        "       pg_get_indexdef(s.indexrelid) "
        "FROM   pg_stat_user_indexes s "
        "JOIN   pg_index i ON i.indexrelid = s.indexrelid "
        "WHERE  s.schemaname NOT IN ('pg_catalog','information_schema')";
    PGresult *res = db_exec(db, sql);
    if (!res) return NULL;

    int n = PQntuples(res);
    IndexStatList *list = calloc(1, sizeof(*list));
    list->items = calloc(n, sizeof(IndexStat));
    list->n = n;

    for (int i = 0; i < n; i++) {
        IndexStat *x = &list->items[i];
        x->schemaname       = strdup(PQgetvalue(res, i, 0));
        x->relname          = strdup(PQgetvalue(res, i, 1));
        x->indexrelname     = strdup(PQgetvalue(res, i, 2));
        x->idx_scan         = atol  (PQgetvalue(res, i, 3));
        x->idx_tup_read     = atol  (PQgetvalue(res, i, 4));
        x->idx_tup_fetch    = atol  (PQgetvalue(res, i, 5));
        x->index_size_bytes = atol  (PQgetvalue(res, i, 6));
        x->is_unique        = (PQgetvalue(res, i, 7)[0] == 't');
        x->is_primary       = (PQgetvalue(res, i, 8)[0] == 't');
        x->index_def        = strdup(PQgetvalue(res, i, 9));
    }
    PQclear(res);
    return list;
}

void index_stats_free(IndexStatList *list) {
    if (!list) return;
    for (int i = 0; i < list->n; i++) {
        free(list->items[i].schemaname);
        free(list->items[i].relname);
        free(list->items[i].indexrelname);
        free(list->items[i].index_def);
    }
    free(list->items);
    free(list);
}
