# HTTP and trace contract (schema 1)

All routes use same-origin JSON. Non-GET requests require `revision` (nonnegative safe integer) and `request_id` (1–80-character string). Responses echo both. Errors are `{error:{code,message}}` with 400, 404, 409, 413, 429, or 500. Client revisions identify editor state; they are not global server counters. Mutations additionally require the current `dictionary_version` and fail with 409 when stale.

## Requests

| Route | Fields beyond revision and request_id |
|---|---|
| `GET /health` | None |
| `GET /api/dictionary` | None; returns the bounded full word list and scope |
| `POST /api/analyze` | `text`, UTF-16 `start`, `end`, integer `threshold` (1–3) |
| `POST /api/accept` | `text`, UTF-16 `start`, `end`, `word`, `dictionary_version` |
| `POST /api/search` | `text`, `pattern` |
| `POST /api/trace` (trie) | `mode:"trie"`, `prefix` |
| `POST /api/trace` (DP) | `mode:"dp"`, `source`, `target` |
| `POST /api/trace` (KMP) | `mode:"kmp"`, `text`, `pattern` |
| `PUT /api/dictionary` | `words` array of strings, `dictionary_version` |

`analyze` returns `token:{word,normalized,start,end}`, `status` (`empty`, `exact`, `prefix`, `misspelled`, `unknown`, or `too_long`), ranked `completions` and `corrections` (`{word,distance,usage}`), and `metrics`. Corrections run only after exact membership and prefix lookup fail. English tokens contain ASCII letters, with internal apostrophes/hyphens; other characters separate tokens. Non-ASCII text remains intact. A selection spanning multiple tokens has no active token. Tokens longer than the word limit yield `too_long` and no suggestions or search work. `workload` gives `dictionary_words`, `trie_nodes`, `query_points`, and `word_limit` from the same snapshot. Exact membership uses the terminal marker reached by the measured prefix traversal, avoiding another uncounted traversal.

`accept` returns the full replacement `text`, UTF-16 `caret`, and a new dictionary version. It changes only the active token, preserving punctuation and initial/all-uppercase capitalization. The browser applies this result only if its editor revision still matches.

`search` returns `matches:[{start,end,utf16_start,utf16_end}]`, `lps`, and `metrics`. `start/end` are zero-based Unicode-code-point ranges; all ends are exclusive. An empty pattern has zero matches. KMP is exact and case-sensitive, including overlaps.

Search responses and KMP trace results also return `comparison:{kmp_total,naive_total,difference}`. The server computes `difference = kmp_total - naive_total`; positive means KMP executed more character comparisons. These are complete computations on identical inputs, including LPS preprocessing. A partially played trace must not use this difference as a claim about its partial counters.

`dictionary` returns `words`, `count`, `dictionary_version`, `scope`, and `limits`. Replacement validates the entire list before publication. Duplicates collapse; accepted-word counts reset. Usage/ranking updates also advance the version. Snapshots remain immutable for readers.

## Traces

`schema_version`, `algorithm`, `request_id`, `revision`, `dictionary_version`, `indexing`, `inputs`, `initial`, `events`, `flow`, `result`, `metrics`, and `metric_definitions` form the envelope. `initial` contains code-point arrays and dimensions, not a prefilled matrix or LPS array. Trie topology uses stable dictionary node IDs and real edges. A bounded selection of nodes explicitly reports `total_nodes` and `hidden_nodes`. Each trie node additionally carries `focus` (boolean): it belongs to an actual traced/cached-word path, rather than neighboring context. The browser can fold context without finding trie paths itself and reports the combined number of hidden nodes. Existing routes and fields are unchanged.

Each event has `seq`, `kind`, `flow`, `i`, `j`, `node`, `other`, `value`, `diagonal`, `up`, `left`, optional boolean `condition`, `text`, and cumulative `counters`. Interpretation:

- Trie: `trie_edge_lookup` identifies source/destination and whether an edge exists; `cache_read` reports actual cached output. No suffix-visit events are invented.
- DP: `dp_initialize`/`dp_cell` write a cell. `dp_cell` carries predecessor costs and the selected direction in `text`. `dp_backtrack` visits an optimal path cell. `edit_apply.text` is the actual transformed source after one edit.
- KMP: `lps_compare`/`kmp_compare` carry comparison pointers and equality; `lps_write` writes `lps[i]=value`; `lps_fallback`/`kmp_fallback` change the pattern pointer from `j` to `value` while retaining `i`. `kmp_advance` reports new pointers; `match_found` reports `[i,j)` in code points.
- `initialize` and `complete` bracket each computation. Recording the trace does not alter computation or metrics.

`flow` is an executed control-flow graph: only observed operations and branches are shown, with stable operation-kind IDs. Outgoing True/False labels correspond to the recorded condition on the source event. It is not presented as all possible branches. The client builds display state from empty initial state plus events `[0,cursor)` on every scrub; it never starts from the final result.

Trace `workload` contains snapshot `dictionary_words`/`trie_nodes`, plus `prefix_points`, DP `source_points`/`target_points`, or KMP `text_points`/`pattern_points`. These additive schema-1 fields describe the actual computation. Baseline metrics are complete, separate computations outside the event stream; event counters cover the selected algorithm only.

## Metrics and limits

`lookups`: trie child-map lookups; `copied`: cached suggestions returned; `scan_checks`: actual prefix character comparisons in a full dictionary scan. These are separate units, not speedup percentages. `visited`/`cells`: fuzzy visited nodes/DP cell computations. `lps` and `search`: KMP preprocessing/search character comparisons; `naive`: baseline character comparisons, including failures; `edits`: non-match edit operations applied. No wall-clock or animation timing is called algorithm runtime.

`scan_words` counts words visited by the full prefix scan, even when zero character comparisons are needed. `pruned` counts fuzzy rows whose minimum exceeds the threshold, including leaf rows; it does not claim to count hidden descendants. `candidates` counts qualifying dictionary words before the top-eight cap. `cells` excludes initialized border cells. `lps_fallbacks`/`search_fallbacks` count recorded pointer fallbacks, including search resets after a match. `matches` counts recorded exact occurrences, including overlaps. New counters are present in every metrics object and cumulative event counter object; unused counters are zero.

Default limits match `AGENTS.md`; body limit is 1 MiB, serialized trace limit is 8 MiB, with at most two simultaneous API computations. Exceeding a trace limit rejects the whole trace. Ordinary endpoints do not record events. Limits live in C++ `Limits`; resource limits can be configured at application startup as documented in the README. Responses never contain partial traces.
