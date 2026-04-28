#include "config.h"
#include "logger.h"
#include "db.h"
#include "query_stats.h"
#include "candidates.h"
#include "hypopg.h"
#include "decision.h"
#include "index_manager.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;
static void on_signal(int s) { (void)s; g_stop = 1; }

static void run_create_cycle(DB *db, const Config *cfg) {
    LOG_I("=== create-cycle start ===");

    QueryStatList *queries = query_stats_top(db, cfg->top_n_queries,cfg->min_query_calls);
    if (!queries || queries->n == 0) {
        LOG_W("no candidate queries from pg_stat_statements; skipping");
        if (queries) query_stats_free(queries);
        return;
    }
    LOG_I("loaded %d candidate queries", queries->n);

    TableStatList *tables = table_stats_all(db);
    if (!tables) { query_stats_free(queries); return; }

    IndexCandidateList *cands = NULL;
    if (cfg->use_pg_qualstats)
        cands = candidates_from_qualstats(db,cfg->min_query_calls);
    if (!cands || cands->n == 0) {
        LOG_W("no index candidates this cycle");
        if (cands) candidates_free(cands);
        table_stats_free(tables);
        query_stats_free(queries);
        return;
    }
    CreateProposalList *props=decision_evaluate_creates(db, cfg, cands, queries, tables);
    int created = 0;
    for (int i = 0; i < props->n; i++) {
        CreateProposal *p = &props->items[i];
        LOG_I("candidate: %s | benefit=%.1f overhead=%.1f size~%ld | %s",
              p->cand->create_sql, p->benefit, p->overhead,
              p->est_size_bytes,   p->reason);
        if (!p->accepted) continue;
        if (created >= cfg->max_creates_per_run) {
            LOG_I("reached max_creates_per_run=%d, deferring rest",
                  cfg->max_creates_per_run);
            break;
        }
        if (cfg->dry_run) {
            LOG_I("[dry-run] would CREATE: %s", p->cand->create_sql);
        } else {
            if (index_create_concurrent(db, p->cand->create_sql) == 0) {
                created++;
                LOG_I("created index: %s", p->cand->create_sql);
            } else {
                LOG_E("failed to create: %s", p->cand->create_sql);
            }
        }
    }
    create_proposals_free(props);
    candidates_free(cands);
    table_stats_free(tables);
    query_stats_free(queries);
    LOG_I("=== create-cycle end (created=%d) ===", created);
}
static void run_drop_cycle(DB *db, const Config *cfg) {
    LOG_I("=== drop-cycle start ===");
    IndexStatList *idxs   = index_stats_all(db);
    TableStatList *tables = table_stats_all(db);
    if (!idxs || !tables) {
        if (idxs)   index_stats_free(idxs);
        if (tables) table_stats_free(tables);
        return;
    }
    DropProposalList *props = decision_evaluate_drops(db, cfg, idxs, tables);
    int dropped = 0;
    for (int i = 0; i < props->n; i++) {
        DropProposal *p = &props->items[i];
        LOG_D("%s.%s scans=%ld writes=%ld benefit=%.1f overhead=%.1f | %s",
              p->relname, p->indexrelname, p->idx_scan, p->index_writes,
              p->benefit, p->overhead, p->reason);

        if (!p->recommend_drop) continue;
        if (dropped >= cfg->max_drops_per_run) {
            LOG_I("reached max_drops_per_run=%d, deferring rest",
                  cfg->max_drops_per_run);
            break;
        }
        if (cfg->dry_run) {
            LOG_I("[dry-run] would DROP %s.%s (%s)",
                  p->schemaname, p->indexrelname, p->reason);
        } else {
            if (index_drop_concurrent(db, p->schemaname, p->indexrelname) == 0)
                dropped++;
        }
    }
    drop_proposals_free(props);
    index_stats_free(idxs);
    table_stats_free(tables);
    LOG_I("=== drop-cycle end (dropped=%d) ===", dropped);
}
int main(int argc, char **argv) {
    logger_init(LOG_INFO);
    if (argc < 2) {
        fprintf(stderr,
            "usage: %s <conninfo> [--no-dry-run]\n"
            "  conninfo example: \"host=localhost dbname=app user=ai\"\n",
            argv[0]);
        return 2;
    }
    Config *cfg = config_default(argv[1]);
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--no-dry-run")) cfg->dry_run = 0;
    }
    LOG_I("starting pg_autoindex (dry_run=%d, create_int=%ds, monitor_int=%ds)",
          cfg->dry_run, cfg->analysis_interval_s, cfg->monitor_interval_s);
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    time_t next_create  = 0;
    time_t next_monitor = 0;

    while (!g_stop) {
        time_t now = time(NULL);

        DB *db = db_connect(cfg->conninfo);
        if (!db) { sleep(5); continue; }
        if (!hypopg_available(db)) {
            LOG_W("hypopg not installed; create-cycles will be skipped");
        }
        if (now >= next_create) {
            run_create_cycle(db, cfg);
            next_create = now + cfg->analysis_interval_s;
        }
        sleep(cfg->monitor_interval_s / 2);
        if (now >= next_monitor) {
            run_drop_cycle(db, cfg);
            next_monitor = now + cfg->monitor_interval_s;
        }
        db_close(db);
        time_t soonest = next_create < next_monitor ? next_create : next_monitor;
        long delay = soonest - time(NULL);
        if (delay < 1) delay = 1;
        if (delay > 60) delay = 60;  
        for (long s = 0; s < delay && !g_stop; s++) sleep(1);
    }
    LOG_I("shutting down");
    config_free(cfg);
    return 0;
}
