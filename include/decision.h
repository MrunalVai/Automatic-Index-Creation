#ifndef DECISION_H
#define DECISION_H

#include "candidates.h"
#include "query_stats.h"
#include "config.h"

//CreateProposalList
typedef struct{
    IndexCandidate *cand;    
    double benefit; 
    double overhead;
    long est_size_bytes;
    int accepted;
    char reason[256];
}CreateProposal;

typedef struct{CreateProposal *items; int n;} CreateProposalList;

CreateProposalList *decision_evaluate_creates(DB *db, const Config *cfg, IndexCandidateList *cands, QueryStatList *queries, TableStatList *tables);
void create_proposals_free(CreateProposalList *list);

//DropProposalList
typedef struct{
    char *schemaname;
    char *relname;
    char *indexrelname;
    long index_size_bytes;
    long idx_scan;
    long index_writes;
    double benefit;
    double overhead;
    int recommend_drop;
    char reason[256];
}DropProposal;

typedef struct {DropProposal *items; int n;} DropProposalList;

DropProposalList *decision_evaluate_drops(DB *db, const Config *cfg, IndexStatList *indexes, TableStatList *tables);
void drop_proposals_free(DropProposalList *list);

#endif
