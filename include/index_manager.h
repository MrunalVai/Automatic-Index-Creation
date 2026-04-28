#ifndef INDEX_MANAGER_H
#define INDEX_MANAGER_H

#include "db.h"

int index_create_concurrent(DB *db, const char *create_index_sql);
int index_drop_concurrent(DB *db, const char *schema, const char *index_name);

#endif
