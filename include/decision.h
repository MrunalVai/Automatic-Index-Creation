#ifndef PGAI_DECISION_H
#define PGAI_DECISION_H

#include "candidates.h"
#include "query_stats.h"
#include "config.h"

typedef struct {
    IndexCandidate *cand;          /* not owned                          */
    double          benefit;       /* Σ Δcost * calls (planner units)    */
    double          overhead;      /* writes * per_write_cost            */
    long            est_size_bytes;
    int             accepted;
    char            reason[256];
} CreateProposal;

typedef struct { CreateProposal *items; int n; } CreateProposalList;

/*
 * For each candidate:
 *   1. hypopg_reset()
 *   2. hypopg_create_index(candidate.create_sql)  -> hypo_name
 *   3. for each query in `queries`:
 *        baseline = explain_total_cost(query)        -- without hypo
 *        with_hyp = explain_total_cost(query)        -- with current hypo set
 *        if with_hyp < baseline:
 *           benefit += (baseline - with_hyp) * calls
 *   4. estimate_total_overhead(table, hypo_size)
 *   5. accept iff benefit / overhead >= cfg->min_benefit_ratio
 *
 * Note: baseline must be computed with the SAME session state aside from
 * the hypothetical index, so we precompute baselines once per query at the
 * start (hypopg_reset first, EXPLAIN, then proceed).
 */
CreateProposalList *decision_evaluate_creates(
        DB *db,
        const Config *cfg,
        IndexCandidateList *cands,
        QueryStatList *queries,
        TableStatList *tables);

void create_proposals_free(CreateProposalList *list);

/* ---- Existing-index review ----------------------------------------------
 *
 * For each existing user index:
 *   benefit_estimate  = idx_scan * mean_query_cost_using_idx(approximation)
 *   overhead_estimate = estimate_total_overhead(table, real_size)
 *   recommend_drop iff benefit / overhead <= cfg->drop_safety_ratio
 *                       AND not unique/primary/exclusion
 *
 * Because Postgres doesn't store "cost saved per scan", we approximate
 * benefit as idx_scan * (avg per-query cost reduction observed for queries
 * on this table -- defaults to 100 planner units when unknown).
 */
typedef struct {
    char  *schemaname;
    char  *relname;
    char  *indexrelname;
    long   index_size_bytes;
    long   idx_scan;
    long   index_writes;
    double benefit;
    double overhead;
    int    recommend_drop;
    char   reason[256];
} DropProposal;

typedef struct { DropProposal *items; int n; } DropProposalList;

DropProposalList *decision_evaluate_drops(
        DB *db,
        const Config *cfg,
        IndexStatList *indexes,
        TableStatList *tables);

void drop_proposals_free(DropProposalList *list);

#endif
