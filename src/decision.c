#include "decision.h"
#include "hypopg.h"
#include "write_overhead.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void create_proposals_free(CreateProposalList *list) {
    if (!list) return;
    free(list->items);
    free(list);
}

static int parse_target(const char *create_sql,char *schema, size_t slen,char *rel,size_t rlen) {
    const char *on = strstr(create_sql, " ON ");
    if (!on) return -1;
    on += 4;
    while (*on == ' ') on++;
    char tmp[512];
    int  ti = 0;
    int  in_quote = 0;
    int  saw_dot  = 0;
    for (; *on && ti < (int)sizeof(tmp) - 1; on++) {
        if (*on == '"') { in_quote = !in_quote; continue; }
        if (!in_quote && (*on == ' ' || *on == '(')) break;
        tmp[ti++] = *on;
        if (*on == '.') saw_dot = 1;
    }
    tmp[ti] = '\0';
    if (saw_dot) {
        const char *dot = strchr(tmp, '.');
        size_t sl = (size_t)(dot - tmp);
        if (sl >= slen) sl = slen - 1;
        memcpy(schema, tmp, sl); schema[sl] = '\0';
        snprintf(rel, rlen, "%s", dot + 1);
    } else {
        snprintf(schema, slen, "public");
        snprintf(rel,    rlen, "%s", tmp);
    }
    return 0;
}
static double *precompute_baselines(DB *db, QueryStatList *queries) {
    if (hypopg_reset(db) != 0) return NULL;
    double *bases = calloc(queries->n, sizeof(double));
    for (int i = 0; i < queries->n; i++) {
        if (explain_total_cost(db, queries->items[i].query_text, &bases[i]) != 0)
            bases[i] = -1.0; 
        LOG_I("baseline cost for query %d/%d: %.1f", i + 1, queries->n, bases[i]);
    }
    return bases;
}

CreateProposalList *decision_evaluate_creates(DB *db,const Config *cfg,IndexCandidateList *cands,QueryStatList *queries,TableStatList *tables) {
    CreateProposalList *out = calloc(1, sizeof(*out));
    if (!cands || cands->n == 0) return out;

    if (!hypopg_available(db)) {
        LOG_E("hypopg extension is required but not present");
        return out;
    }

    out->items = calloc(cands->n, sizeof(CreateProposal));

    double *baselines = precompute_baselines(db, queries);
    if (!baselines) { LOG_E("baseline pass failed"); return out; }

    for (int i = 0; i < cands->n; i++) {
        IndexCandidate *c = &cands->items[i];
        CreateProposal *p = &out->items[out->n];
        p->cand = c;
        if (hypopg_reset(db) != 0) {
            snprintf(p->reason, sizeof(p->reason), "hypopg_reset failed");
            out->n++;
            continue;
        }
        char hypo_name[256] = {0};
        if (hypopg_create(db, c->create_sql, hypo_name, sizeof(hypo_name)) != 0) {
            snprintf(p->reason, sizeof(p->reason),
                     "hypopg_create_index failed for: %s", c->create_sql);
            out->n++;
            continue;
        }
        long est_size = 0;
        if (hypopg_estimated_size(db, hypo_name, &est_size) != 0)
            est_size = 0;
        p->est_size_bytes = est_size;

        double benefit = 0.0;
        for (int q = 0; q < queries->n; q++) {
            if (baselines[q] < 0) continue;
            LOG_I("evaluating candidate %d/%d on query %d/%d", i + 1, cands->n, q + 1, queries->n);
            double with_hyp = 0;
            if (explain_total_cost(db, queries->items[q].query_text,
                                   &with_hyp) != 0) continue;
            double delta = baselines[q] - with_hyp;
            if (delta <= 0) continue; 
            benefit += delta * (double)queries->items[q].calls;
        }
        p->benefit = benefit;

        char schema[128], rel[128];
        if (parse_target(c->create_sql, schema, sizeof(schema),rel, sizeof(rel))    == 0) {
            TableStat *t = table_stats_find(tables, schema, rel);
            p->overhead = estimate_total_overhead(t,est_size,cfg->base_index_write_cost);
        } else {
            p->overhead = 0.0;
        }
        double ratio = (p->overhead > 0)? (p->benefit / p->overhead): (p->benefit > 0 ? 1e18 : 0);
        if (p->benefit <= 0) {
            p->accepted = 0;
            snprintf(p->reason, sizeof(p->reason),"no planner benefit detected");
        } else if (ratio < cfg->min_benefit_ratio) {
            p->accepted = 0;
            snprintf(p->reason, sizeof(p->reason),"benefit/overhead = %.2f < threshold %.2f",ratio, cfg->min_benefit_ratio);
        } else {
            p->accepted = 1;
            snprintf(p->reason, sizeof(p->reason),"benefit/overhead = %.2f >= %.2f",ratio, cfg->min_benefit_ratio);
        }
        out->n++;
    }
    hypopg_reset(db);
    free(baselines);
    return out;
}


void drop_proposals_free(DropProposalList *list) {
    if (!list) return;
    for (int i = 0; i < list->n; i++) {
        free(list->items[i].schemaname);
        free(list->items[i].relname);
        free(list->items[i].indexrelname);
    }
    free(list->items);
    free(list);
}

DropProposalList *decision_evaluate_drops(
        DB *db,
        const Config *cfg,
        IndexStatList *indexes,
        TableStatList *tables) {

    (void)db;
    DropProposalList *out = calloc(1, sizeof(*out));
    if (!indexes || indexes->n == 0) return out;
    out->items = calloc(indexes->n, sizeof(DropProposal));
    const double per_scan_benefit_units = 100.0;
    for (int i = 0; i < indexes->n; i++) {
        IndexStat *x = &indexes->items[i];
        DropProposal *p = &out->items[out->n];
        p->schemaname       = strdup(x->schemaname);
        p->relname          = strdup(x->relname);
        p->indexrelname     = strdup(x->indexrelname);
        p->index_size_bytes = x->index_size_bytes;
        p->idx_scan         = x->idx_scan;
        TableStat *t = table_stats_find(tables, x->schemaname, x->relname);
        p->index_writes = table_index_writes(t);
        p->benefit      = (double)x->idx_scan * per_scan_benefit_units;
        p->overhead     = estimate_total_overhead(t, x->index_size_bytes,
                                                  cfg->base_index_write_cost);
        if (x->is_primary || x->is_unique) {
            p->recommend_drop = 0;
            out->n++;
            continue;
        }
        double ratio = (p->overhead > 0)
                       ? (p->benefit / p->overhead)
                       : (p->benefit > 0 ? 1e18 : 0);

        if (p->idx_scan == 0 && p->index_writes > 0) {
            p->recommend_drop = 1;
           
        } else if (ratio <= cfg->drop_safety_ratio) {
            p->recommend_drop = 1;
 \
        } else {
            p->recommend_drop = 0;
        }
        out->n++;
    }
    return out;
}
