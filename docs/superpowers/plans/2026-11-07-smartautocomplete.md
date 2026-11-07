# SmartAutocomplete Implementation Plan

> **For agentic workers:** Use executing-plans inline. Track each checkbox here; no per-task subagents.

**Goal:** Build the local C++ algorithm editor and visualizer specified in `AGENTS.md`, using the approved header-free layout.

**Architecture:** A standard-library-only core owns Unicode offsets, dictionary snapshots, algorithms, counters, and optional event traces. Crow serializes these results. Vanilla JavaScript renders and replays immutable traces, never computes algorithm results.

**Tech Stack:** C++20, CMake/CTest, Crow v1.3.4, standalone Asio, local HTML/CSS/SVG/JavaScript.

**Spec:** `AGENTS.md`; user-approved visual study in `.superpowers/brainstorm/visual-preview/content/index.html`. User requested implementation without more design questions.

## Global Constraints

- No application header; light input rail, dark visualization canvas, concise labels and analytics instead of descriptive paragraphs.
- C++ algorithms only; offline runtime; loopback default; no new database, frontend packages, or external APIs.
- Default limits: 5,000 unique dictionary words, 32-code-point words/DP strings, 8,000-code-point text, 1,000-code-point pattern, eight suggestions, 20,000 events.
- UTF-8 boundary validation, code-point indexing, end-exclusive UTF-16 browser ranges, ASCII-only case folding.
- Crow pinned to v1.3.4; core builds without Crow. Keep the visual study intact.

## Review Focus

- Surrogate-pair selections must reject interior offsets rather than corrupt text.
- In-flight requests must not repopulate UI after edits, dictionary changes, or errors.
- Oversized traces must fail explicitly without breaking ordinary result requests.
- Invalid dictionary replacement must preserve the previous version and words.
- Playback must reproduce earlier states without reusing final matrix/LPS state.

## Tasks

### Task 1: Core algorithms, text, dictionary, and traces

**Files:** `include/smartautocomplete/core.hpp`, `src/core/{text,dictionary,algorithms}.cpp`, `tests/core_tests.cpp`, `CMakeLists.txt`.

**Interfaces:** `decode(string_view) -> Text`, `encode(u32string_view) -> string`; `Dictionary(vector<string>)`, `prefix(u32string_view, Trace*) -> PrefixResult`, `fuzzy(u32string_view, int) -> vector<Candidate>`, `accept(u32string_view)`; `levenshtein(u32string_view,u32string_view,Trace*) -> DistanceResult`; `kmp(const Text&,const Text&,Trace*) -> SearchResult`; `naive(const Text&,const Text&) -> SearchResult`. Plain core structs carry immutable input structures, result ranges, counters, edit operations, and ordered events.

- [x] Write literal regression tests, independent randomized references, trace-invariance and Unicode tests; observed the missing KMP overlap behavior fail before implementation.
- [x] Implement text validation, top-eight trie caches and promotion, pruned DP dictionary search, deterministic matrix/backtrack/forward edits, KMP/LPS and independent baselines.
- [x] Pass core CTest and ASan/UBSan builds, including 500 deterministic randomized cases.

### Task 2: Crow API and bounded snapshot store

**Files:** `src/web/{api.hpp,api.cpp}`, `src/main.cpp`, `tests/api_tests.cpp`, `docs/api.md`, `data/{words.txt,README.md}`, `CMakePresets.json`.

**Interfaces:** `Api::handle(method,path,body) -> {status,body}` consumes Task 1. Routes follow `AGENTS.md`; JSON contract is written before browser integration. `Api` owns immutable dictionary snapshots with version-checked publication, request limits, and structured errors.

- [x] Write route tests for success, malformed JSON/types, limits, version conflicts, invalid replacement preservation, parallel reads, and trace envelopes; observed unavailable health route fail before implementation.
- [x] Pin/download Crow, document dependency licenses, implement routes/JSON validation, bounded trace serialization, and fixed static routes.
- [x] Pass core and API CTest; browser acceptance checks exercise the live Crow routes.

### Task 3: Header-free editor and trace playback

**Files:** `static/{index.html,style.css,app.js}`, `docs/{architecture.md,demo.md}`, `README.md`.

**Interfaces:** consumes only documented API results/events; browser code chooses layout and derives display state by replay, not algorithm decisions.

- [x] Establish browser checks against the running API before wiring controls; observed the missing Editor fail.
- [x] Add editor, autocomplete/corrections, keyboard acceptance, find navigation/highlights, dictionary replacement, and three visualizer modes.
- [x] Add Structure/Flow, Play/Pause/Step/Restart/speed/scrub, event replay, late-response guards, and empty/error clearing.
- [x] Browser controls, narrow layouts, local-only assets, reproducible counters, release/core-only/ASan/UBSan CTest, HTTP checks, and seven focused replay/race regressions pass after the independent review.

## Execution ledger

- Ruling: work in the existing directory, which is not a Git repository; no worktree or commits can be made without initializing unrelated version-control state.
- Ruling: continue inline without a plan approval pause because the user explicitly said to focus on building now.
- Pre-flight: Task 2 consumes Task 1 core types; Task 3 consumes Task 2 JSON, documented before coupling. No conflicting existing application code exists.
- Task 1: complete — release core CTest and ASan/UBSan core checks passed.
- Task 2: complete — release and ASan/UBSan API checks passed; HTTP paths exercised by Playwright.
- Task 3: implemented — Chromium acceptance checks passed at desktop and 390px mobile sizes. Independent review pending.
- Verification: `ctest --test-dir build-sanitize --output-on-failure` passed both suites in 93.06 seconds. The first combined configure/build/test command hit its 120-second tool limit; running the finished tests separately passed with no sanitizer findings.
- Final independent review: completed by `code-reviewer`; four blocking issues and one flow-rendering suggestion reproduced with failing tests before fixes. No second reviewer was dispatched.
- Final: fixed acceptance overflow before snapshot publication — `Acceptance cannot exceed editor limit` RED→GREEN; rejected output leaves dictionary version unchanged.
- Final: fixed pending analysis after acceptance errors — `failed_accept_cancels_analysis` RED→GREEN; revision invalidation and request cancellation prevent stale suggestions and traces.
- Final: fixed obsolete mutation responses — `stale_accept_ignored` and `stale_dictionary_ignored` RED→GREEN; mutation handlers honor rejected versions.
- Final: fixed LPS phase and pointer replay — `lps_pointers` RED→GREEN; write events restore both indices and the preprocessing phase.
- Final: upgraded collapsed/overlapping flow loops to a teaching-correctness issue, then fixed — `flow_loop_visible` RED→GREEN; self-loops curve visibly and parallel True/False branches separate.
- Final: fixed initial backward-navigation boundary cases — `initial_previous_match` and `initial_up_suggestion` RED→GREEN.
- Verification environment: the original test helper left an orphan server caching old JavaScript. Identified its exact PID and served-content mismatch, stopped only that session-owned process, then used `exec` in helper server commands. Fresh-server acceptance and all seven focused tests passed.
- Final verification: release CTest 2/2; core-only CTest 1/1; ASan/UBSan CTest 2/2 (92.69 seconds); HTTP boundary checks; Chromium acceptance and seven focused regressions all passed.
- Final deployment boundary: Crow still buffers transport bodies before API validation. Preserve loopback-only binding and document the need for an authenticated size-limiting proxy before any untrusted public deployment; no broader server/framework redesign was added.
- Final reviewer exclusions: no findings were silently dropped. Dependency internals, Windows portability, and public deployment were not verified. Documentation and full test reruns were completed by the implementer afterward. No deferred code-polish findings remain.
- Task 3: complete for the implemented local teaching application; retained source and plan in place because no Git history exists. The separate static mockup tunnel was not switched to the mutable application without approval.
- Authorized follow-up: user requested switching the existing tunnel to the live app. Updated only the preview proxy, preserving loopback Crow, secret-path access, static-file restrictions, and original mockup files. Added streaming request-body and public-Origin guards; public read-only HTTP/Chromium checks passed via `tests/tunnel_test.py`.
