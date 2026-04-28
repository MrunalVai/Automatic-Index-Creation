#include "index_manager.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long simple_hash(const char *s) {
    unsigned long h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)(*s++);
    return h;
}

int index_create_concurrent(DB *db, const char *create_index_sql) {
    if (strncasecmp(create_index_sql, "CREATE INDEX", 12) != 0) {
        LOG_E("not a CREATE INDEX statement: %s", create_index_sql);
        return -1;
    }
    char name[64];
    snprintf(name, sizeof(name), "pgai_%lx",simple_hash(create_index_sql) & 0xFFFFFFFFul);
    const char *rest = create_index_sql + 12;       
    size_t blen = strlen(create_index_sql) + sizeof(name) + 64;
    char *sql = malloc(blen);
    snprintf(sql, blen, "CREATE INDEX CONCURRENTLY %s%s", name, rest);
    LOG_I("executing: %s", sql);
    int rc = db_exec_cmd(db, sql);
    free(sql);
    return rc;
}

int index_drop_concurrent(DB *db, const char *schema, const char *index_name) {
    char sql[512];
    snprintf(sql, sizeof(sql),"DROP INDEX CONCURRENTLY IF EXISTS \"%s\".\"%s\"",schema, index_name);
    LOG_I("executing: %s", sql);
    return db_exec_cmd(db, sql);
}
