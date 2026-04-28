#include "write_overhead.h"
#include <math.h>

#define PG_PAGE 8192
double estimate_per_write_cost(long index_size_bytes, double base_cost) {
    if (index_size_bytes < PG_PAGE) index_size_bytes = PG_PAGE;
    double pages = (double)index_size_bytes / (double)PG_PAGE;
    return base_cost * (1.0 + log2(pages));
}
double estimate_total_overhead(const TableStat *t,long index_size_bytes,double base_cost) {
    if (!t) return 0.0;
    long writes = table_index_writes(t);
    if (writes <= 0) return 0.0;
    return (double)writes * estimate_per_write_cost(index_size_bytes, base_cost);
}
