#ifndef CANDIDATES_H
#define CANDIDATES_H

#include "db.h"

typedef struct{
    char *schemaname;
    char *relname;
    char *create_sql;     
    long qual_occurrences;
}IndexCandidate;

typedef struct{IndexCandidate*items; int n;} IndexCandidateList;
IndexCandidateList *candidates_from_qualstats(DB *db, long min_occurrences);
void candidates_free(IndexCandidateList *list);

#endif
