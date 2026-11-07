# Side-panel algorithm workspace

**Scope:** The user wants all execution data and playback beside the visualization, avoiding downward page scrolling. Keep C++ algorithms, API routes, dictionary, trace semantics, and existing header-free visual direction.

- [x] Add desktop journey checks: editor left, drawing center, execution right; measurements must remain inside the visible execution content at 1024×640, 1024×768, 1280×720, 1366×768, and 1440×900.
- [x] Move controls/data into a semantic execution sidebar with compact desktop breakpoints and stacked flow below 1024px. Playback and legend remain outside the flexible details area. Long narration/results have keyboard-accessible scrolling so measurements retain space.
- [x] Improve trace-only presentation: prefix-character progress, width-aware trie/KMP layout, labeled KMP pointers, and DP predecessor choices. Replay retains the trie endpoint/missing index and updates DP indices from C++ events. Exact character matches are labeled Match rather than Replace.
- [ ] Run existing C++/HTTP/browser checks plus keyboard, responsive, scrubbing, empty/error/loading, and side-panel tests; inspect screenshots.
- [ ] Preserve live words and exact usage counts during the switch; keep the public tunnel URL, origin validation, request cap, and static-file restrictions.

**Design:** light input/execution rails frame a dominant navy canvas. Keep existing colors and type. Playback stays at the top of the execution rail; current operation, result, counters, optional counting rules, and legend sit below it. Desktop has no global header or bottom dashboard. Mobile and short screens retain document scrolling rather than squeeze or clip controls.

**Verification boundary:** automated Chromium walks and screenshots are not human usability or physical-device testing. No new dependency or server/API feature is planned.

## Latest verification

- Release build and both CTest suites pass (API and core).
- A temporary test driver called the real C++ API without HTTP, then replayed seven returned traces through the current JavaScript presentation functions using the installed JavaScriptCore engine. Syntax, restart/scrub equivalence, trie endpoint/missing markers, complete DP matrices and edits, DP state chips/choices, KMP match ranges/pointers/fallbacks, and the 9 + 68 = 77 comparison text pass. Minimal DOM stubs do not verify layout or browser behavior.
- HTML IDs are unique, markup asset references are local, and the updated browser test parses.
- Final browser reruns remain pending. Chromium exits at startup with `Operation not permitted`; Crow also cannot bind the isolated port under this sandbox. The requested final test-server startup could not complete. Earlier screenshots were inspected, but they do not prove the latest changes.
- Sanitizer validation is incomplete: LeakSanitizer fails under the restricted tracing environment. With leak detection disabled, API tests pass; core tests exceed the 30-second limit. The unrestricted-time attempt was interrupted. No C++ code changed in this UI pass.
- No live dictionary replacement or public-process switch occurred during this verification session.
