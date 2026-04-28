#ifndef PGAI_WRITE_OVERHEAD_H
#define PGAI_WRITE_OVERHEAD_H

#include "query_stats.h"

/*
 * Write-overhead model
 * --------------------
 * Each non-HOT write to a table touches every index on that table.
 * The pganalyze indexing-engine model (referenced in your prompt) treats
 * the per-write cost of an index as roughly proportional to a small,
 * size-dependent factor (the cost of descending the btree + a leaf
 * insert), and aggregates the cost over the projected number of writes
 * in the analysis window.
 *
 * We model:
 *
 *   per_write_cost(idx) = base * (1 + log2(pages(idx)))
 *
 *      pages(idx) = max(1, index_size_bytes / 8192)
 *
 *   total_overhead(idx) = per_write_cost(idx) * index_writes(table)
 *
 *      index_writes(table) = n_ins + (n_upd - n_hot_upd) + n_del
 *
 * Both numbers are in planner-cost units, the same currency EXPLAIN's
 * total cost is in -- so they're directly comparable to the benefit
 * computed from EXPLAIN diffs.
 */

double estimate_per_write_cost(long index_size_bytes, double base_cost);
double estimate_total_overhead(const TableStat *t,
                               long index_size_bytes,
                               double base_cost);

#endif
