#ifndef PGAI_QUERY_STATS_H
#define PGAI_QUERY_STATS_H

#include "db.h"

/* ---- Top SELECT-ish queries from pg_stat_statements ------------------- */
typedef struct {
    char  *queryid;
    char  *query_text;     /* normalised text with $1, $2, ...             */
    long   calls;
    double total_exec_time;
    double mean_exec_time;
} QueryStat;

typedef struct { QueryStat *items; int n; } QueryStatList;

QueryStatList *query_stats_top(DB *db, int top_n, long min_calls);
void           query_stats_free(QueryStatList *list);

/* ---- Per-table write/read stats from pg_stat_user_tables -------------- */
typedef struct {
    char *schemaname;
    char *relname;
    long  n_tup_ins;
    long  n_tup_upd;
    long  n_tup_del;
    long  n_tup_hot_upd;
    long  n_live_tup;
    long  seq_scan;
    long  idx_scan;
} TableStat;

typedef struct { TableStat *items; int n; } TableStatList;

TableStatList *table_stats_all(DB *db);
TableStat     *table_stats_find(TableStatList *list,
                                const char *schema, const char *rel);
void           table_stats_free(TableStatList *list);

/* fraction of writes that bypass index update (HOT). 0..1                 */
double         table_hot_ratio(const TableStat *t);

/* total non-HOT writes that DO touch indexes                              */
long           table_index_writes(const TableStat *t);

/* ---- Per-index stats from pg_stat_user_indexes ------------------------ */
typedef struct {
    char *schemaname;
    char *relname;
    char *indexrelname;
    long  idx_scan;
    long  idx_tup_read;
    long  idx_tup_fetch;
    long  index_size_bytes;
    int   is_unique;
    int   is_primary;
    char *index_def;       /* output of pg_get_indexdef                    */
} IndexStat;

typedef struct { IndexStat *items; int n; } IndexStatList;

IndexStatList *index_stats_all(DB *db);
void           index_stats_free(IndexStatList *list);

#endif
