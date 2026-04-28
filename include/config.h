#ifndef CONFIG_H
#define CONFIG_H

typedef struct{
    char*conninfo;             
    int top_n_queries;        
    long min_query_calls;      
    int max_creates_per_run;  
    int max_drops_per_run;    
    double min_benefit_ratio;    
    double drop_safety_ratio;    
    double base_index_write_cost;
    int analysis_interval_s;  
    int monitor_interval_s;   
    int dry_run;              
    int use_pg_qualstats;     
}Config;

Config*config_default(const char *conninfo);
void config_free(Config*cfg);

#endif
