#ifndef PGAI_CONFIG_H
#define PGAI_CONFIG_H

typedef struct {
    char  *conninfo;             /* libpq conninfo string                    */
    int    top_n_queries;        /* how many queries to consider             */
    long   min_query_calls;      /* skip queries called fewer than this      */
    int    max_creates_per_run;  /* safety cap on CREATE INDEX per cycle     */
    int    max_drops_per_run;    /* safety cap on DROP INDEX per cycle       */
    double min_benefit_ratio;    /* create iff benefit/overhead >= this      */
    double drop_safety_ratio;    /* drop existing iff benefit/overhead <=    */
    double base_index_write_cost;/* planner-cost units per index touch base  */
    int    analysis_interval_s;  /* seconds between create-analysis runs     */
    int    monitor_interval_s;   /* seconds between drop-review runs         */
    int    dry_run;              /* 1 = log only, no DDL                     */
    int    use_pg_qualstats;     /* 1 = use pg_qualstats for candidates      */
} Config;

Config *config_default(const char *conninfo);
void    config_free(Config *cfg);

#endif
