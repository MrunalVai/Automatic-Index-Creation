# pg_autoindex

Automatic Postgres index advisor + applier in C, using HypoPG for
hypothetical-index cost estimation and a write-overhead model so we don't
just create indexes that read-improve at the cost of write-degrade.

## The decision rule

For each candidate index `I` on table `T`:

```
benefit(I)  = Σ over queries q  of  (cost_baseline(q) − cost_with_I(q)) × calls(q)
overhead(I) = index_writes(T) × per_write_cost(I)

index_writes(T)   = n_tup_ins + (n_tup_upd − n_tup_hot_upd) + n_tup_del
per_write_cost(I) = base_cost × (1 + log2(pages(I)))   ← btree descend approx

CREATE iff benefit(I) / overhead(I) ≥ min_benefit_ratio   (default 1.5)
```

For an *existing* index `I` we run the same comparison on real stats:

```
benefit(I)  = idx_scan × per_scan_benefit_units    (heuristic; tunable)
overhead(I) = index_writes(T) × per_write_cost(I)

DROP iff   not unique/PK
       AND ( idx_scan == 0 with writes > 0
              OR  benefit/overhead ≤ drop_safety_ratio (default 0.5) )
```

Both numbers are in **planner-cost units** — the same currency EXPLAIN uses,
which is why the comparison is meaningful.

## Modules

```
include/
  config.h           tunables
  logger.h           leveled logging
  db.h               libpq wrapper (connect, exec, scalars, has_extension)
  query_stats.h      pg_stat_statements / pg_stat_user_tables / pg_stat_user_indexes
  candidates.h       index candidates from pg_qualstats
  hypopg.h           hypopg_create / hypopg_reset / EXPLAIN cost extraction
  write_overhead.h   per_write_cost & total overhead model
  decision.h         create- and drop-side cost-benefit
  index_manager.h    CREATE/DROP INDEX CONCURRENTLY
src/
  ... matching .c files
  main.c             two interleaved timed loops (create / monitor)
```

Module dependencies:

```
main → config, logger, db, query_stats, candidates, hypopg,
       decision, index_manager
decision → hypopg, write_overhead, query_stats, candidates
hypopg, candidates, query_stats, index_manager → db, logger
write_overhead → query_stats
```

No module reaches across boundaries; you can swap any of:

- `candidates.c` (e.g. parse `pg_stat_statements` SQL with libpg_query
  instead of using pg_qualstats),
- `write_overhead.c` (more sophisticated cost model — WAL volume,
  fillfactor effects, partial-index selectivity),
- `decision.c` (different acceptance policy)

without touching anything else.

## Required Postgres extensions

```sql
CREATE EXTENSION pg_stat_statements;  -- query statistics (mandatory)
CREATE EXTENSION hypopg;              -- hypothetical indexes (mandatory)
CREATE EXTENSION pg_qualstats;        -- candidate generation (recommended)
```

`shared_preload_libraries` must include `pg_stat_statements` and
`pg_qualstats`. HypoPG is per-session, no preload required.

## Build

```sh
make                       # uses pg_config from PATH
PG_CFG=/usr/pgsql-16/bin/pg_config make
```

## Run

```sh
./pg_autoindex "host=localhost dbname=app user=ai password=..."
# defaults: dry-run, analysis every 1h, monitor every 6h, top 50 queries

./pg_autoindex "host=... dbname=app user=ai" --no-dry-run
```

The user must have at minimum `pg_read_all_stats` and, for `--no-dry-run`,
`CREATE` on the target schemas.

## Notes / gotchas

1. **HypoPG is a session-local construct.** `decision_evaluate_creates`
   issues `hypopg_reset()` between candidates so they're scored independently.
   Don't run multiple workers on the same connection.
2. **Parameterised queries.** `explain_total_cost()` detects `$1..$N` and
   `PREPARE`s the query, then `EXPLAIN EXECUTE`s it with `NULL`s under
   `force_generic_plan`. This gives a stable, parameter-independent cost.
3. **HOT updates** are excluded from `index_writes` because they don't
   touch indexes Postgres can prove are unaffected. This understates
   overhead slightly (HOT may not apply to the *new* index), so the model
   is mildly biased toward keeping indexes — adjust if needed.
4. **The `per_scan_benefit_units` constant** in `decision.c` is the only
   hand-tuned magic in the drop path. You can replace it with a
   per-table average computed from `pg_stat_statements` total time / calls.
5. **`CREATE INDEX CONCURRENTLY`** can fail mid-build leaving an `INVALID`
   index. The orchestrator does not currently clean those up — add a
   pre-step that drops indexes with `pg_index.indisvalid = false` if
   you want full self-healing.

## Where to extend first

- Real candidate generation from `pg_stat_statements` (use libpg_query to
  parse SQL into an AST and pull WHERE-clause columns) — drop the
  pg_qualstats dependency.
- Persist proposals + decisions to a small history table so the monitor
  loop can detect "we created this 3 days ago, is it earning its keep?"
- Replace the constant `per_scan_benefit_units` with the actual
  cost-saved-per-scan, computable as
  `(cost_without_idx − cost_with_idx)` measured at create-time and stored.
