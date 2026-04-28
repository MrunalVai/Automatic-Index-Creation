#ifndef WRITE_OVERHEAD_H
#define WRITE_OVERHEAD_H

#include "query_stats.h"

double estimate_per_write_cost(long index_size_bytes, double base_cost);
double estimate_total_overhead(const TableStat *t, long index_size_bytes, double base_cost);

#endif
