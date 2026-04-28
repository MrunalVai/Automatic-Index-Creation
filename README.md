# pg_autoindex
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

