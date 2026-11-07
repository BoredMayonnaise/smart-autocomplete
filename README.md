# SmartAutocomplete

A local C++20 text editor and algorithm visualizer for teaching trie prefix lookup, standard Levenshtein distance, and KMP search. Crow provides the HTTP interface. Local HTML/CSS/SVG and vanilla JavaScript display C++ results and replay C++ events; they do not implement the algorithms.

## Build

Requires CMake 3.24+, a C++20 compiler (tested GCC 13.3), and standalone Asio (tested 1.28.1). Crow v1.3.4 is fetched once from its release archive and verified by SHA-256.

Ubuntu development prerequisites:

```sh
sudo apt-get install build-essential cmake libasio-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
./build/smartautocomplete
```

Open **http://127.0.0.1:8080/**. The process stays running until interrupted with Ctrl+C. It binds only to loopback and uses two Crow threads. No internet is needed at runtime. Assets and dictionary default to the absolute source directory embedded at build time, so running from another working directory works. To relocate the project or executable:

```sh
/path/to/smartautocomplete --root /path/to/project --port 8080
```

The root must contain `static/index.html`, `static/style.css`, `static/app.js`, and `data/words.txt`. Static assets load at startup; restart the process after changing them. No arbitrary filesystem paths are served.

For an isolated localhost instance and temporary Cloudflare preview, with Caddy, cloudflared, curl, and OpenSSL already installed:

```sh
bash scripts/preview.sh
```

The launcher uses loopback ports 8082 and 8318, starts the current built application with fresh bundled words, and prints the public URL after local/proxy/public health checks. Keep the terminal open; Ctrl+C stops only its child processes. It leaves existing previews untouched. Access uses a random secret path, validates public Origin headers, and caps request bodies at 1 MiB. Anyone with the link can change this instance's dictionary. Runtime logs live in a private temporary directory. Launcher shell syntax and proxy configuration were checked; full startup requires a host that permits local sockets and outbound tunnel connections.

### Offline builds and dependencies

The first configure needs internet unless Crow is cached. Keep the downloaded archive at `third_party/Crow-v1.3.4.tar.gz`, or set `-DCROW_ARCHIVE=/absolute/path/Crow-v1.3.4.tar.gz`. SHA-256:

```text
6be759b3a648b41f0adc3a8408a53fd0243775217b4e32e8101d69e4aadf48cc
```

Archive source: https://codeload.github.com/CrowCpp/Crow/tar.gz/refs/tags/v1.3.4. A populated build directory reuses its dependency extraction. New offline build directories need the cached archive and locally installed Asio. Missing Asio is reported by CMake; install `libasio-dev` or configure `-DASIO_INCLUDE_DIR=/path/containing/asio.hpp`.

| Dependency | Version tested | License |
|---|---|---|
| Crow | 1.3.4 (pinned release, SHA-256 above) | BSD-3-Clause; upstream `LICENSE` in extracted source |
| standalone Asio | 1.28.1 | Boost Software License 1.0; installed Asio package copyright/license |
| bundled vocabulary | 10,000 words: SCOWL 2020.12.07 + original teaching list | SCOWL permissive notices + CC0-1.0 teaching words; see `data/README.md` and `data/SCOWL-Copyright.txt` |

No extra JSON library, Node.js, frontend package manager, Python application service, database, remote fonts, or CDN assets are used.

### Windows / Visual Studio 2022

Install Visual Studio 2022 with Desktop development with C++, and CMake 3.24+. Download standalone Asio 1.28.1 from https://github.com/chriskohlhoff/asio/releases/tag/asio-1-28-1 and extract its `asio/include` directory. Run in a developer PowerShell:

```powershell
cmake --preset windows -DASIO_INCLUDE_DIR=C:/deps/asio/asio/include
cmake --build --preset windows
ctest --preset windows
.\build-windows\Release\smartautocomplete.exe --root C:/path/to/autocomplete
```

VS Code can use the same CMake definitions; no editor is required. Windows instructions are provided but were not executed in the Linux VM.

## Use

- Type in the editor. C++ identifies the active English token and returns completions or spelling candidates. Arrow Up/Down selects a suggestion; Tab or Enter accepts the selected word. Clicking a suggestion also accepts it. Punctuation and supported capitalization remain intact.
- Text, selection, source/target, pattern, and selected mode are saved in browser-tab session storage so refreshing does not discard a draft. No dictionary, usage counts, or traces are stored there. If browser storage is blocked, editing still works; tab draft recovery is unavailable.
- **Trie / complete:** press **Visualize** to trace the active token. Cached suffixes remain gray unless actually visited. The graph reports hidden nodes.
- **Levenshtein / correct:** enter source and target; visualize initialization, matrix cells, predecessor costs, backtracking, alignment, and forward edits.
- **KMP / find:** enter a pattern; the editor highlights all matches and Previous/Next selects their UTF-16 ranges. Visualize preprocessing, LPS writes/fallbacks, comparisons, and overlapping matches.
- **Structure / Flow:** switch views of the same trace. Flow shows observed operations and branches, not paths that were never executed.
- **Show branches / Focus path:** the trie starts with actual traced/cached paths visible, including the root. The switch reveals server-provided neighboring branches; the count always reports what remains hidden. It does not imply that suffixes were traversed or that the entire dictionary fits on screen.
- **Try an example:** a grouped native selector exposes every presentation case without crowding the toolbar.
- **Play/Pause**, **Step**, **Restart**, speed, and the progress slider use the existing trace. No computation request is made per step. Changing relevant inputs invalidates the trace; hiding the page pauses playback.
- Desktop uses input, visualization, and execution columns. Playback, measurements, results, and the pinned legend share the execution rail; long step/result text scrolls within its panel. Below 1024px, columns stack and the page scrolls. Large structures use labeled canvas windows or scrolling.
- **Measurements:** suggestion metrics show actual fuzzy nodes/cells/pruning and eligible words before the top-eight cap. Trace metrics show input sizes and snapshot size. KMP compares complete counts only at completion and reports when preprocessing makes it more expensive than naive search. No animation delay is reported as algorithm time.
- **Dictionary:** replace the process-local vocabulary. Invalid replacement preserves the existing dictionary. Replacement and restart reset usage counts; accepting words updates ranking and dictionary version.

## Limits

Defaults: 10,000 unique words; 32 code points per word and DP-pair input; 8,000 text code points; 1,000 pattern code points; eight suggestions; 20,000 events; 1 MiB request JSON; 8 MiB serialized trace. Trace overflow returns an error, never a truncated replay. Ordinary result endpoints do not record traces.

Startup supports `--max-words`, `--max-text`, `--max-pattern`, `--max-events`, `--max-body`, and `--max-trace-bytes`, each followed by a positive integer. Word/DP length and suggestion count are defined by core `Limits` and remain 32/eight in this executable. Higher resource limits need new load testing.

The dictionary and tokenizer support English ASCII letters and internal apostrophes/hyphens. Other characters are token boundaries, not silently lowercased bytes. Search and pair-distance support Unicode code points. There is no NFC equivalence, full Unicode case folding, grapheme-cluster indexing, or one-cost transposition. `recieve` to `receive` has distance **2**.

This is a **single-user teaching application**, not a public production service. The C++ server binds to loopback; Host/Origin checks reject non-local origins, and JSON mutations require `application/json`. Crow buffers HTTP bodies before the API applies its 1 MiB rejection, so this cap is not a transport-level memory bound. Do not expose Crow directly to untrusted clients.

The user-approved temporary public preview uses a separate loopback Caddy proxy with a secret URL path, a 1 MiB request-body cap before proxy buffering, and public-origin validation. Only after that validation does the proxy adapt Host/Origin for the loopback server. Bare paths and project files remain inaccessible. The secret URL is a bearer credential, not an account login: anyone with it can edit the shared in-memory dictionary. Stop `cloudflared` to remove public access. The original static mockup remains on disk but is no longer served by this tunnel.

## Verify

Core-only build (does not fetch or start Crow):

```sh
cmake -S . -B build-core -DSMART_BUILD_SERVER=OFF -DBUILD_TESTING=ON
cmake --build build-core --parallel 2
ctest --test-dir build-core --output-on-failure
```

Sanitizers, when supported by GCC/Clang:

```sh
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
```

Optional browser tests use already-installed **Python Playwright** and its Chromium browser, solely as test tooling. With the server running:

```sh
python3 tests/browser_test.py
python3 tests/browser_race_test.py
python3 tests/http_test.py
python3 tests/ui_refinement_test.py
python3 tests/side_panel_test.py
```

Set `SMART_URL` to test another local port (`ui_refinement_test.py` defaults to 8082 to avoid the live demo). Browser tests that replace dictionaries must run against an isolated process, never the live shared preview.

Side-panel checks cover desktop visibility down to 1024×640, trie prefix progress and retained node state, DP predecessor choices and indices, KMP pointers, and narrow-screen overflow. The latest UI changes still require a browser rerun: this session's sandbox blocks Chromium startup and binding the isolated Crow server (`Operation not permitted`).

The acceptance test checks all modes, playback controls, keyboard acceptance, find navigation, dictionary failures/replacement, 390px viewport overflow, and absence of remote asset requests. Ten focused regressions check LPS pointer replay, cancelled analysis after errors, obsolete mutation responses, visible flow loops, initial backward navigation, native example-selector keyboard use, error-notification precedence, and empty Flow controls. HTTP checks verify static-file restrictions, Host/Origin and content-type enforcement, malformed UTF-8/JSON, and the body limit. Refinement checks cover focused graph visibility, controls near inputs, draft restoration, loading/error recovery, 375/390px and landscape layouts, reduced motion, and 200% CSS zoom. CTest includes independent randomized C++ checks, 10,000-word vocabulary validation, offset validation, trace replay/result invariance, version conflicts, and API validation/concurrency. Earlier revisions passed HTTP and Chromium checks in the Linux VM. For the current 10,000-word changes, Release CTest (2/2), core-only CTest (1/1), deterministic dictionary regeneration, and JavaScriptCore replay/metric checks pass. Chromium and Crow socket startup are currently blocked by sandbox permissions, so current browser layout and HTTP behavior remain unverified. Debug ASan/UBSan CTest exceeded 25 seconds per test with leak detection disabled; direct runs also exceeded 30 seconds without sanitizer diagnostics. Sanitizer validation remains incomplete. GCC may report `-Wmaybe-uninitialized` inside upstream Crow's JSON parser; the current Release build reports no project warnings. These are automated checks and screenshot inspections, not human usability testing or physical-device testing.

See [API](docs/api.md), [architecture](docs/architecture.md), and [presentation walkthrough](docs/demo.md).
