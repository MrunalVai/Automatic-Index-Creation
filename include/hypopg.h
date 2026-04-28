#ifndef PGAI_HYPOPG_H
#define PGAI_HYPOPG_H

#include "db.h"

/* Returns 1 if the hypopg extension is loaded in the connected DB. */
int    hypopg_available(DB *db);

/* Drops all hypothetical indexes in the current session.            */
int    hypopg_reset(DB *db);

/* Creates a hypothetical index from a CREATE INDEX statement.
   Returns 0 on success and writes the assigned hypo-index name into
   `name_out` (caller-owned buffer of `name_out_size`).               */
int    hypopg_create(DB *db, const char *create_index_sql,
                     char *name_out, size_t name_out_size);

/* Estimates the planner cost of `query` using the current session's
   hypothetical indexes (if any). Writes total cost into *out.
   Returns 0 on success.                                              */
int    explain_total_cost(DB *db, const char *query, double *out);

/* Estimates size of a hypothetical index in bytes via
   hypopg_relation_size(oid). Pass the hypo-index NAME; the function
   resolves the oid internally.                                       */
int    hypopg_estimated_size(DB *db, const char *hypo_index_name,
                             long *bytes_out);

#endif
