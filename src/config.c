#include "config.h"
#include <stdlib.h>
#include <string.h>

Config *config_default(const char *conninfo) {
    Config *c = calloc(1, sizeof(Config));
    if (!c) return NULL;
    c->conninfo             = strdup(conninfo ? conninfo : "");
    c->top_n_queries        = 50;
    c->min_query_calls      = 10;
    c->max_creates_per_run  = 3;
    c->max_drops_per_run    = 3;
    c->min_benefit_ratio    = 1.5;  
    c->drop_safety_ratio    = 0.5;   
    c->base_index_write_cost = 0.01; 
    c->analysis_interval_s  = 100;
    c->monitor_interval_s   = 400;                          
    c->dry_run              = 1;                      
    c->use_pg_qualstats     = 1;
    return c;
}

void config_free(Config *cfg) {
    if (!cfg) return;
    free(cfg->conninfo);
    free(cfg);
}
