#ifndef PGAI_INDEX_MANAGER_H
#define PGAI_INDEX_MANAGER_H

#include "db.h"

/* Convert "CREATE INDEX ON ..." -> "CREATE INDEX CONCURRENTLY <name> ON ..."
   so live traffic isn't blocked. Generates a deterministic name based on
   schema+rel+columns. */
int index_create_concurrent(DB *db, const char *create_index_sql);

int index_drop_concurrent(DB *db, const char *schema, const char *index_name);

#endif
