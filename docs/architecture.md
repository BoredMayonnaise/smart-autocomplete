# Architecture and measurement notes

## Boundaries

- `smart_core`: standard-library-only C++20. UTF-8 decoding/encoding, Unicode/UTF-16/byte offsets, token replacement, dictionary construction and caches, fuzzy traversal, all algorithms and baselines, metrics, optional traces.
- `smart_api`: Crow JSON serialization and trust-boundary validation. Immutable `shared_ptr<const Dictionary>` snapshots; mutation copies/builds outside the publication mutex, then verifies snapshot identity under a short lock. Racing mutations receive 409. Analysis never holds the publication lock during computation.
- `smartautocomplete`: fixed static-file whitelist, loopback HTTP routes, Host/Origin protection, JSON content-type enforcement, and two Crow threads. API computations additionally use a two-slot semaphore.
- `static/app.js`: request cancellation/revision checks, editor selection, DOM/SVG rendering, positioning, playback. No LPS construction, DP computation, edit reconstruction, trie lookup, fuzzy ranking, or text matching runs in the browser.

## Algorithm details

### Trie

Nodes use hash-map character edges with stable vector indices. Construction sorts unique normalized words, preventing duplicate counts. Every prefix node has at most eight ranked cached word indices. Ranking is usage descending, then normalized word lexicographically. Acceptance changes only the accepted word's prefix caches; it can promote a word absent from the previous top eight. Publishing a new snapshot advances the version.

Lookup costs expected O(L) child-map lookups plus O(k) result copying. This excludes dictionary construction, snapshot copying, usage updates, fuzzy matching, JSON, and rendering. The C++ full dictionary scan uses identical normalization, ranking, and output limit, counting actual character comparisons separately from child lookups. No speedup percentage compares these unlike units.

### Levenshtein

The pair implementation initializes the empty-string borders and computes unit-cost insert/delete/substitute with zero-cost equality. Deterministic backtracking prefers diagonal, then deletion, then insertion. The optimal path is recorded backward; edits are reconstructed and applied in forward order to actual source strings. Pair computation costs O(mn) time and O(mn) matrix space, plus returned traces/edits.

Dictionary correction first checks membership, then completeness as a prefix. Only missing words without prefix completions enter fuzzy matching. DP rows propagate down real trie edges; branches are pruned only when the row minimum exceeds the threshold. Work depends on visited trie nodes times query length, plus candidate ranking; it is not O(L).

Candidate insertion keeps only the best eight seen so far, ordered by distance, usage, and normalized word. Every qualifying word increments the candidate counter even when it cannot enter the retained set. This bounds candidate storage to O(k), with O(k) insertion per qualifying word, instead of storing and sorting all qualifying words. Traversal order and pruning do not depend on ranking. Analysis reports visited nodes, computed cells, pruned rows, and eligible words; exact/prefix inputs skip fuzzy work. The full prefix-scan baseline remains a separate O(dictionary size × prefix length) computation on each nonempty supported token and explicit trie trace, so the whole analysis request is not O(L).

### KMP

LPS preprocessing and search share no browser implementation. Each actual character comparison increments exactly one of `lps` or `search`. Fallback retains the text pointer. After a match, `j` resumes from LPS, preserving overlaps. Empty pattern convention: zero matches.

Complexity is O(N + M + R), including output of R matches; preprocessing space is O(M), plus match and optional event storage. The independent naive baseline counts every comparison it executes, including failures. With short inputs, LPS overhead can exceed naive work. For text `abc` and pattern `abc`, KMP performs two preprocessing plus three search comparisons; naive performs three.

## Trace correctness

The optional trace sink attaches to the same C++ implementation used for ordinary results. Counter increments happen independently of recording. Events contain actual comparisons, writes, fallbacks, matches, cache reads, path visits, and applied edits. Event overflow throws an explicit 413; no partial trace is returned. API serialization adds immutable initial arrays/topology and the final result without changing computation.

Structure rendering starts with null matrix/LPS values, an unvisited trie, no matches, and the original source. Forward playback applies the next recorded event. Scrubbing/restart rebuild state from immutable initial inputs and replay `[0,cursor)`, so previously revealed values do not leak backward. Final candidate/alignment output is revealed only at the corresponding recorded output stage.

Trie drawing receives actual visited/cached-word paths with `focus` metadata plus neighboring branches from the snapshot. The browser initially folds neighboring context so the root and useful path fit vertically; **Show branches** exposes the supplied context. Hidden counts include both server-omitted and client-folded nodes. Gray suffixes are stored structure, not pretended execution. KMP uses indexed windows and reports hidden positions. Matrices and graphs can scroll.

UI maintenance stays in three files: `static/index.html` owns controls and labels, the readable sections in `static/style.css` own visual states/layout, and `static/app.js` owns request handling, tab-scoped draft state, and event rendering. Trie focus metadata is generated in `src/web/api.cpp::trie_structure`; no prefix-search logic belongs in JavaScript. `tools/make_dictionary.cpp` only prepares the bundled 10,000-word asset offline; the runtime core still validates and ranks it.

The Flow view uses operation-kind IDs and actual transitions observed between events. Conditions label outgoing edges True/False only when the source event recorded a real boolean condition. Curved returns and self-loops represent repeated operations. Its scope label explicitly says this is an executed control-flow graph, not all possible branches.

## Text policy

HTTP strings must be valid UTF-8. Algorithms index decoded Unicode scalars. `Text` stores code-point-boundary mappings to byte and UTF-16 positions; selection offsets inside surrogate pairs fail with 400. Match ends are exclusive. English dictionary normalization folds ASCII only. Browser highlights and replacement selections use returned UTF-16 offsets, not byte offsets.

## Measurements and regression fixture

For 36 `a` characters followed by `b`, searching for `aaaaab` yields exactly one match starting at code-point 31. These implementations perform 9 LPS comparisons plus 68 KMP search comparisons, versus 192 naive comparisons. This is an exact regression example for the chosen counting rules, not a universal performance claim. Both baselines use identical inputs and matching semantics. No elapsed-time benchmark or platform-independent speed claim is provided. Animation delay, instrumentation, serialization, and SVG layout are not algorithm runtime.

## Remaining deployment boundaries

Everything is process-local. There are no accounts, persistence, collaboration, cloud services, or public production deployment. Transport body buffering remains a Crow limitation; route-level rejection does not cap bytes already buffered. The application remains bound to loopback. Windows instructions are unverified on hardware. See README for dependency caching and licenses.
