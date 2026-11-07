# AGENTS.md — SmartAutocomplete

## Project goal

Build a working text editor and algorithm visualizer for a school/thesis presentation. Implement autocomplete, spelling correction, and text search with three explicit algorithmic operations:

1. Trie-based prefix search.
2. Levenshtein edit distance using dynamic programming.
3. Knuth–Morris–Pratt (KMP) string matching.

The audience must be able to type an input, obtain a correct result, and play or step through the computation that produced it. All algorithm implementations, dictionary operations, ranking, comparison baselines, counters, and trace generation belong in C++.

This file is an implementation guide. Do not mistake its proposed structure or acceptance criteria for an already implemented application.

## Required stack and boundaries

| Layer | Required technology | Responsibility |
| --- | --- | --- |
| Core | Modern C++20 and the standard library | Algorithms, tokenization, validation, ranking, dictionary storage, traces, metrics |
| Web server | Crow | HTTP routes, JSON, local static assets, page delivery |
| Build | CMake, CTest | Repeatable builds and C++ tests |
| Browser display | Local HTML, CSS, SVG, minimal vanilla JavaScript | Inputs, requests, visual rendering, playback, keyboard controls |
| Runtime data | C++ memory, bundled dictionary text file | Dictionary and accepted-word counts; no database needed |

“Pure C++” means the application engine and server are C++. Crow delivers a browser interface, so HTML/CSS/SVG and a small JavaScript presentation layer are allowed. Do not describe browser markup and playback as C++ execution.

- Do not implement the three algorithms again in JavaScript.
- Do not use Node.js, Python services, React, Vue, Vite, TypeScript compilation, or a frontend package manager.
- Do not call remote spelling, autocomplete, AI, or search services.
- Do not add Qt or another desktop GUI alongside Crow.
- No CDN scripts, remote fonts, paid APIs, or mandatory hosting subscription. Once built, the demo must work without internet access.
- Visual Studio 2022 or VS Code may be used as development tools. CMake remains the portable build definition; the application must not depend on a particular editor.

Use the supplied `SmartAutocomplete.html` as a behavior and visual reference if present. Its JavaScript algorithms are reference implementations to port, not the production engine. Keep it intact while developing the C++ version.

## Scope

Deliver one local application with an editor, autocomplete suggestions, spelling corrections, find controls, editable dictionary, and three visualizer modes. Include Play/Pause, Step, Restart, speed, and progress scrubbing.

Keep the initial deployment single-user and local: bind to `127.0.0.1` by default. Dictionary replacement affects this application's current dictionary, and accepted-word counts are process-local. Display that scope in the dictionary controls. Accounts, collaboration, cloud sync, public hosting, databases, and payment features are outside this version.

## Suggested source organization

Adapt these names to an existing repository without creating duplicate subsystems.

| Path | Contents |
| --- | --- |
| `CMakeLists.txt` | Core library, server executable, tests, dependency configuration |
| `CMakePresets.json` | Documented development/release configurations |
| `include/smartautocomplete/` | Public core types and algorithm interfaces |
| `src/core/trie.cpp` | Trie construction, prefix lookup, top-k caches |
| `src/core/levenshtein.cpp` | DP distance, fuzzy dictionary traversal, edit reconstruction |
| `src/core/kmp.cpp` | LPS construction and exact matching |
| `src/core/baselines.cpp` | Instrumented dictionary scan and naive text search |
| `src/core/text.cpp` | Text validation, token boundaries, encoding/offset conversion |
| `src/core/dictionary.cpp` | Dictionary snapshots, versions, usage ranking |
| `src/core/trace.cpp` | Shared event types and trace limits |
| `src/web/` | Crow handlers and JSON serialization |
| `src/main.cpp` | Configuration and server startup |
| `templates/` | Page templates when needed |
| `static/` | CSS, SVG assets, presentation-only JavaScript |
| `data/words.txt` | Bundled dictionary with documented provenance/license |
| `tests/` | C++ algorithm, trace, validation, and route tests |
| `docs/` | Architecture, API contract, demo script, benchmark notes |

The core library must compile and run tests without starting Crow. Crow requests must call the core; route handlers must not contain copies of algorithm logic.

## Algorithm requirements

### 1. Trie prefix search

- Build the trie in C++ with explicit nodes, character edges, terminal markers, and stable identifiers for visualization.
- Store children using standard-library containers. Use deterministic ordering when serializing graph edges or resolving ties.
- Maintain at most eight cached suggestions at each prefix node. Rank by accepted-word count descending, then lexicographically by normalized word.
- Repeated dictionary entries must not create duplicate words or inflate usage counts.
- A lookup follows the prefix once and retrieves that node's cached suggestions. Report lookup work separately from cache construction, output delivery, and ranking updates.
- Document prefix traversal as O(L) expected with hash-based child lookup, plus O(k) to copy k cached results. Do not claim the whole request, dictionary construction, or fuzzy search is O(L).
- Stop on the first missing edge. Return an empty suggestion list.
- After a word is accepted, update usage and the relevant prefix caches in C++.
- Show incomplete input as a prefix when completions exist, so suggestions do not imply every unfinished word is a spelling error.

### 2. Levenshtein spelling correction

- Implement unit-cost insertion, deletion, and substitution in C++. Exact character matches cost zero.
- Initialize the empty-string row and column correctly.
- Provide an exact dictionary-membership check before fuzzy matching.
- For dictionary corrections, propagate DP rows through the trie and prune a branch only when its row minimum exceeds the allowed edit distance.
- Return up to eight candidates, ordered by edit distance, usage count, then normalized word.
- Support edit thresholds 1, 2, and 3.
- For a selected source/target pair, generate the matrix, a deterministic optimal backtrack, and valid edits applied in forward order.
- Standard Levenshtein does not include a one-cost adjacent transposition. `recieve` to `receive` costs 2; do not silently change the algorithm to Damerau–Levenshtein.
- State O(mn) time for one source/target matrix. Describe dictionary fuzzy search separately in terms of visited trie nodes and query length.

### 3. KMP text search

- Implement LPS preprocessing and matching in C++.
- Find all exact, case-sensitive occurrences, including overlaps.
- After a match, continue using the LPS fallback instead of discarding overlapping possibilities.
- On a fallback, the text pointer must not move backward. Visualize the pattern pointer changing while the current text character is retained.
- Define an empty pattern as zero matches. Document this convention consistently.
- Return match ranges with explicit offset units and use them for editor selection, next/previous navigation, and highlights.
- State O(N + M + R) for preprocessing, search, and returning R matches, with O(M) preprocessing space plus output storage.

Hash maps, vectors, SVG, and Crow are supporting structures or technologies. They are not additional algorithms. Do not claim a suffix tree is implemented.

## Text and offset policy

- Use valid UTF-8 at the HTTP boundary; reject malformed encoding cleanly.
- Use decoded Unicode code points for algorithm indexing. Keep conversion helpers between UTF-8 byte offsets, code-point indices, and the browser's UTF-16 selection offsets.
- Each match includes code-point positions and UTF-16 start/end offsets. Return end-exclusive ranges; UI labels can display one-based positions.
- Case normalization for autocomplete must be explicit. Start with ASCII A–Z folding, preserve other code points, and do not claim full Unicode case folding or NFC equivalence without a suitable implementation and tests.
- Define the initial dictionary as English words, allowing internal apostrophes/hyphens. Document how unsupported word characters are handled rather than using locale-sensitive byte classification accidentally.
- Preserve punctuation and surrounding text when accepting a suggestion. Identify the active token and compute its replacement range in C++ from the editor text and selection offsets.
- Return the replacement text and new UTF-16 caret position. Preserve initial capitalization and all-uppercase typing for the supported English vocabulary.

## Visual direction and the supplied diagram

Use the supplied image as a reference for numbered circular nodes, arrowheads, curved return edges, Start/End markers, and a readable flow from one state to another.

Do not copy its 1–14 topology, or assign meanings to `S`, `tt`, `ff`, `A1`, or `S1` without a specification. Those labels do not define this application's algorithms.

Provide a compact **Structure / Flow** switch:

- **Structure:** the actual trie branches, edit-distance matrix, or KMP text/pattern alignment.
- **Flow:** a control-flow graph for the selected algorithm, with stable node IDs and execution highlighting driven by the same server trace.

Flow graphs must represent real operations: initialization, conditions, actions, loops, output, and termination. True/False edges must correspond to actual conditions. Dashed edges require a visible legend and a defined meaning; omit them when they add no information. A trie is acyclic, so do not add loop-back edges to its data structure merely to resemble the reference image.

Use local SVG for exact visuals. Keep the interface restrained: light editor/results surfaces and a dark visualization panel with cyan/green activity, amber current work, and a distinct missing/mismatch state. Also use labels, shapes, or line treatments so color is not the only signal.

### Structure views

| Mode | Required visual behavior |
| --- | --- |
| Trie | Light up each traversed character edge; mark missing branches; distinguish terminal words; show cached suggestions without pretending to traverse their suffixes |
| Edit distance | Reveal computed cells, highlight predecessor cells and selected minimum, show the optimal alignment, then transform the source through the reconstructed edits |
| KMP | Show indexed text and pattern rows, align the pattern at the active text position, display both pointers, show LPS values and fallback movements, highlight actual matches |

For large inputs, use scrolling/windowing with visible offsets and disclose any hidden branches. Never present a cropped subset as the complete structure.

## Trace contract: one engine, two outputs

Each C++ algorithm accepts an optional trace sink. The same implementation computes the result with or without recording. Do not maintain a separate “animation algorithm.”

Define a versioned trace schema containing:

- Algorithm name, normalized inputs, dictionary version, request ID, indexing convention.
- Immutable initial structures: nodes/edges, matrix dimensions, or text/pattern arrays.
- Ordered events with sequence number, operation kind, stable state/node/edge identifiers, relevant indices, changed values, and cumulative counters.
- Final result and complete metric definitions.

Useful events include `trie_edge_lookup`, `prefix_missing`, `cache_read`, `dp_initialize`, `dp_cell`, `edit_apply`, `lps_compare`, `lps_write`, `kmp_compare`, `kmp_fallback`, `match_found`, and `complete`.

Record only events that occurred. Construct visualization topology from C++ state; browser positioning is allowed, but browser code must not infer algorithm decisions, calculate LPS, fill DP cells, reconstruct edits, or generate missing algorithm events.

Scrubbing must restore the exact state at the chosen event. Use initial state plus deterministic event replay, optionally with rendering checkpoints. Never infer earlier state from a mutable final structure.

Playback uses the trace already returned by Crow; each Step must not issue another computation request. Clear old timers when inputs, mode, dictionary version, or trace change. Pause on page hiding. Playback speed must not change algorithm results or metric totals.

## Suggested HTTP contract

Use same-origin routes with Crow JSON types. Document the exact request and response schema before coupling the UI to it.

| Route | Responsibility |
| --- | --- |
| `GET /` | Deliver the interface |
| `GET /health` | Report that the process is available |
| `POST /api/analyze` | Accept editor text and UTF-16 selection; return active token, completions, corrections, and dictionary version |
| `POST /api/accept` | Validate an accepted word and editor snapshot; update usage and return the replacement/caret |
| `POST /api/search` | Return KMP matches and measured KMP/naive comparison counts |
| `POST /api/trace` | Return an instrumented trie, DP-pair, or KMP computation for visualization |
| `GET /api/dictionary` | Return dictionary metadata and optionally the bounded word list |
| `PUT /api/dictionary` | Validate a whole replacement before atomically publishing it |

Use request IDs/editor revisions to prevent late responses replacing newer suggestions. Debounce editor analysis, cancel obsolete fetches, and verify response revisions on arrival. Empty and error states must clear stale output.

Dictionary replacement must be atomic. Invalid input leaves the previous dictionary intact. Concurrent analysis and trace requests must use a consistent immutable snapshot; version usage updates too, so caches and rankings cannot drift during a replay.

Validate JSON types, field lengths, encoding, thresholds, limits, and unknown modes. Return structured errors with suitable HTTP status codes. Escape user content in HTML/SVG and insert text with text-safe DOM APIs. Static file serving must be restricted to the intended asset directory.

## Limits and resource use

Start with configurable limits: 10,000 unique dictionary words; 32 code points per dictionary word and DP-pair input; 8,000 code points per editor/search text; 1,000 code points per KMP pattern; eight suggestions; and 20,000 trace events. Cap request bodies and serialized trace size as well.

If a requested trace exceeds a limit, return a clear error instead of an apparently complete, truncated animation. Ordinary result endpoints must remain useful without recording traces.

Target modest hardware, including a two-core, 12-GB Linux VM. Bound worker threads, trace memory, and simultaneous computations. Do not hold a global exclusive lock throughout expensive searches. No unbounded per-keystroke rebuild of the dictionary.

## Honest measurements

- Implement a C++ full dictionary prefix scan and a C++ naive substring search as baselines.
- Compare identical inputs, dictionary snapshot, normalization, matching semantics, and output limits/ranking.
- Count KMP character comparisons during both LPS preprocessing and search. Show these components and their total.
- Count naive character comparisons actually executed, including failed comparisons.
- Trie child lookups and scan character checks are different units. Label them separately; do not turn them into a misleading speedup percentage.
- Instrumentation, JSON serialization, browser layout, and animation delay are not algorithm execution time.
- If reporting elapsed time, use `std::chrono::steady_clock`, separate trace-enabled measurements, state the build/input sizes, and avoid conclusions from one tiny timing sample.
- Do not claim KMP always uses fewer comparisons; report inputs where preprocessing makes its total larger as well.

## Build and development

Use a stable, explicitly pinned Crow release or commit compatible with the selected Asio version. Never silently pull a moving `master` branch. Record dependency versions and licenses. Prefer Crow's packaged CMake target, `Crow::Crow`, and Crow's JSON support; avoid an extra JSON dependency without a concrete need.

Install/configure dependencies once using a documented route. Subsequent runtime use must be offline. Provide an actionable error when development dependencies are missing. Do not fabricate successful build results.

The project should support these commands after dependencies are configured:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Document the server executable, asset location, working directory, host, and port. Keep assets discoverable from configuration or a known install layout rather than accidentally depending on the current shell directory. For Windows, document the corresponding CMake preset/Visual Studio workflow.

Enable useful warnings for the active compiler. Use RAII, clear ownership, standard containers, and explicit limits. Run AddressSanitizer/UndefinedBehaviorSanitizer when supported by the development toolchain.

## Required validation

Use an independent small reference implementation in C++ tests where appropriate. Test behavior and invariants, not just the implementation's own output format.

1. Trie: empty/missing prefixes, exact words, duplicates, deterministic ordering, top-eight caches, and promotion after an accepted word that was previously outside the top eight.
2. Levenshtein: insertion, deletion, substitution, empty strings, equal strings, all thresholds, pruning against a full dictionary scan, and forward edit reconstruction.
3. KMP: empty inputs, pattern longer than text, no matches, repeated prefixes, overlaps, LPS invariants, and agreement with naive search on randomized cases.
4. Traces: enabling recording leaves results unchanged; replay reaches the real final result; indices and graph edges are valid; counters match the operations; restarts and scrubbing are deterministic.
5. Text: punctuation and sentence preservation, capitalization, invalid UTF-8, non-ASCII offset conversion, and UTF-16 caret/match ranges.
6. Routes: valid/invalid JSON, limits, stale revisions, concurrent reads during dictionary replacement, and preservation of the old dictionary on failed replacement.
7. Browser: every mode, Play/Pause/Step/Restart/speed/scrub, keyboard selection, active-token replacement, find navigation, request errors, and narrow-screen overflow.

Use these deterministic presentation cases:

| Input | Expected result |
| --- | --- |
| Prefix `algor`, with the three words present | `algorithm`, `algorithmic`, `algorithms` in the documented rank order |
| `programing` → `programming` | Distance 1; one insertion in the replay |
| `recieve` → `receive` | Distance 2 under standard Levenshtein |
| `kitten` → `sitting` | Distance 3 |
| Search `ana` in `banana` | Zero-based starts 1 and 3 |
| Search `ana` in `banana bandana banana` | Zero-based starts 1, 3, 11, 16, 18 |
| Search `aaaaab` in 36 `a` characters followed by `b` | One match starting at 31; with the specified implementations, 9 LPS + 68 KMP search comparisons, versus 192 naive comparisons |

The final comparison count is a regression fixture for these particular counting rules and implementations, not a universal KMP performance claim.

## Implementation sequence and completion criteria

Implement the smallest complete slices in this order:

1. CMake core library, dictionary, and algorithm tests.
2. Three correct C++ algorithms and independent baselines.
3. Trace recording/replay invariants and graph metadata.
4. Crow JSON routes calling the tested core.
5. Editor/results connected to Crow; remove browser algorithm duplication.
6. Structure/Flow visuals and playback controls.
7. Integrated browser checks, build instructions, and a short presentation walkthrough.

Finish when all three features work through Crow, offline assets load, graph highlighting follows real execution, counts are reproducible, and required checks pass. Report exactly what was implemented, what was tested, and any remaining limits. Do not replace working features with static mockups or call the project complete while browser JavaScript still computes its algorithm results.

## Primary implementation references

- Crow setup and CMake integration: <https://crowcpp.org/master/getting_started/setup/>
- Crow page delivery: <https://crowcpp.org/master/getting_started/a_simple_webpage/>
- Crow Mustache templates: <https://crowcpp.org/master/guides/templating/>
- Crow source and release selection: <https://github.com/CrowCpp/Crow>

These documentation URLs describe APIs; pin an actual dependency version in the build and verify its matching documentation during implementation.
