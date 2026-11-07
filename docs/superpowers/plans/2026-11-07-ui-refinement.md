# Editor and visualization refinement

**Goal:** Refine the existing header-free learning workspace and expand the bundled English vocabulary, without changing algorithm behavior or API routes.

**Authority:** `AGENTS.md`, approved header-free layout, and the user's implementation-mode refinement request.

## Bounded design

- Paper `#f5f5f7`, white editor, ink `#20252d`, navy canvas `#121b2b`, cyan `#78dce8`, green `#9be2bd`; amber/red retain execution semantics.
- System sans-serif for UI, system monospace for characters and counters. No external font or package.
- Keep the existing two-column workspace. Make the text input and visualization action a coherent left-hand task; give playback and the actual structure priority on the right.
- Use consistent 8px spacing, restrained borders/shadows, 36px desktop controls and at least 44px touch controls. Keep every action labeled and keyboard accessible.
- Focus the trie on real traced/cached paths by default, with a discoverable branch-context switch and accurate hidden-node counts. No browser algorithm duplication.
- Keep all examples accessible in a compact native selector, not six competing buttons. Keep concise field labels, meaningful empty/error/loading states, and a restrained playback status.
- Replace the minified stylesheet with readable sections in the same file, not a new styling framework.

## Acceptance and work ledger

- [x] Established baseline: trie content was 1,535px high inside a 390px canvas; the root was below the viewport. Live dictionary contained 242 words at version 3.
- [x] Observed failing bundle-size, focus-metadata, and UI checks before implementation; added control-proximity, draft, keyboard, and state coverage.
- [x] Expanded to 5,000 words from pinned SCOWL levels 10/20, preserving all 242 teaching words and unmodified copyright notices. Production C++ dictionary validation and deterministic regeneration checks pass.
- [x] Refined the existing header-free HTML/CSS and presentation handlers; preserved algorithm implementations, routes, counters, and event replay. Added only optional trie-node focus metadata.
- [x] Verified desktop, 375/390px mobile, 844px landscape, 1024px tablet, 200% CSS zoom, keyboard/focus, reduced motion, and loading/error/empty/success behavior in automated Chromium flows. Release/core-only/ASan+UBSan CTest, HTTP checks, full browser acceptance, and ten focused regressions pass.
- [x] Published through the same approved tunnel. Briefly blocked public dictionary mutations, captured the unchanged 242-word/version-3 snapshot and all usage counts, restored all prior words and identical counts into the 5,000-word/version-4 process, verified the public preview, and retired only the old session-owned process.

## Data safeguards

The live dictionary is not disposable. Its version is currently 3, so there have been at most two mutation operations. Each positive-usage word must therefore appear in its own top-eight prefix cache. Querying every current word can recover all usage counts exactly while confirming an unchanged version. If the version exceeds this provable bound or changes during checkpoint/publication, stop deployment rather than guess or reset counts. Do not change the public API to add persistence just for this refinement.

No Git repository exists; retain work in place. The user's implementation request authorizes proceeding without another design approval round.

## Final review and verification record

- Independent read-only review found a native-select keyboard loop; `example_keyboard_navigation` reproduced it before the fix. The selector now retains keyboard selection and clears only after manual input or mode changes.
- `dictionary_error_not_masked` reproduced an analysis error hidden by a success toast; notification order now preserves recovery messages.
- `empty_flow_hides_branch_control` reproduced an irrelevant branch control in empty Flow view; view changes now update visibility independently of trace rendering.
- All ten focused regressions passed after these changes. No API route or algorithm implementation changed; dictionary versions remain monotonic across the preview switch.
- Sanitizer CTest passed 2/2 in 149.40 seconds with the expanded vocabulary. Release CTest passed 2/2; core-only passed 1/1. The imported SCOWL notice and regenerated word asset match their verified sources exactly.
- One headless screenshot capture failed before trace readiness; the test now captures after the trace-ready assertion. One public headless click timed out; an isolated visible/enabled-control probe and a subsequent full public HTTP/Chromium run passed without application changes. These environmental attempts are recorded rather than counted as successful runs.
- Public verification passed: 5,000 words, every previous word retained, version advanced from 3 to 4, unchanged Origin/body limits, inaccessible bare/project paths, real KMP fixture, and local Step playback without computation requests. The former process was stopped only after a final unchanged-source-version check.
- Maintainer entry points: `static/style.css` sections for appearance; `static/app.js` request/draft/render handlers; `src/web/api.cpp::trie_structure` for focused topology; `tools/make_dictionary.cpp` plus `data/README.md` for vocabulary regeneration.
- No human usability testing, assistive-technology session, physical-device testing, or Firefox/Safari/Windows execution was performed. Browser-tab draft recovery depends on session storage; normal server restarts still reset usage, as documented.
