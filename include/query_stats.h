#ifndef QUERY_STATS_H
#define QUERY_STATS_H

#include "db.h"

typedef struct{
    char*queryid;
    char*query_text;
    long calls;
    double total_exec_time;
    double mean_exec_time;
}QueryStat;

typedef struct{QueryStat *items; int n;}QueryStatList;

QueryStatList *query_stats_top(DB *db, int top_n, long min_calls);
void query_stats_free(QueryStatList *list);

typedef struct{
    char *schemaname;
    char *relname;
    long  n_tup_ins;
    long  n_tup_upd;
    long  n_tup_del;
    long  n_tup_hot_upd;
    long  n_live_tup;
    long  seq_scan;
    long  idx_scan;
}TableStat;

typedef struct {TableStat *items; int n;} TableStatList;

TableStatList *table_stats_all(DB *db);
TableStat *table_stats_find(TableStatList *list, const char *schema, const char *rel);
void table_stats_free(TableStatList *list);
double table_hot_ratio(const TableStat *t);
long table_index_writes(const TableStat *t);

typedef struct{
    char *schemaname;
    char *relname;
    char *indexrelname;
    long idx_scan;
    long idx_tup_read;
    long idx_tup_fetch;
    long index_size_bytes;
    int is_unique;
    int is_primary;
    char *index_def;       
}IndexStat;

typedef struct {IndexStat *items; int n;} IndexStatList;

IndexStatList *index_stats_all(DB *db);
void index_stats_free(IndexStatList *list);

#endif
