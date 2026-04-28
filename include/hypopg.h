#ifndef HYPOPG_H
#define HYPOPG_H

#include "db.h"

int hypopg_available(DB *db);
int    hypopg_reset(DB *db);
int    hypopg_create(DB *db, const char *create_index_sql,
                     char *name_out, size_t name_out_size);
int    explain_total_cost(DB *db, const char *query, double *out);
int    hypopg_estimated_size(DB *db, const char *hypo_index_name,
                             long *bytes_out);

#endif
