#ifndef PGAI_CANDIDATES_H
#define PGAI_CANDIDATES_H

#include "db.h"

/* A proposed (not yet hypothetical) index, in CREATE INDEX form.       */
typedef struct {
    char *schemaname;
    char *relname;
    char *create_sql;     /* full CREATE INDEX ... ON ... USING btree...*/
    long  qual_occurrences; /* how many times the underlying qual fired */
} IndexCandidate;

typedef struct { IndexCandidate *items; int n; } IndexCandidateList;

/* Sources qual predicates from pg_qualstats and emits btree CREATE INDEX
   statements grouped by (table, column-set). Requires the pg_qualstats
   extension; returns an empty list otherwise.                           */
IndexCandidateList *candidates_from_qualstats(DB *db, long min_occurrences);

void candidates_free(IndexCandidateList *list);

#endif
