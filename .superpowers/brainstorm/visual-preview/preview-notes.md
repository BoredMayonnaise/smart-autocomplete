# Visual preview — not the application

- [x] Scope: one throwaway visual study with three fixed algorithm snapshots; no algorithm implementation, package installation, or changes to AGENTS.md.
- [x] Palette: paper #f3f6fa, ink #192b42, canvas #101e32, cyan #67dcf3, green #88e0b4, amber #ffd185.
- [x] Type: locally available system sans-serif for controls; system monospace for indexed characters and counters.
- [x] Layout: a narrow light input rail beside a dominant dark algorithm canvas; comparison bars below the canvas; stack on narrow screens.
- [x] Review: make the actual indexed characters the hero, not a marketing headline or decorative statistics grid. Use concise labels, no explanatory paragraphs.
- [x] Build: HTML/CSS/SVG, minimal JavaScript to switch fixed snapshots. Disabled playback and editing clearly distinguish the study from a working application.
- [x] Verify the local preview, private-path restriction, and public tunnel: local/public HTML matches the source exactly; unkeyed, project-file, and traversal requests return 404. Browser rendering and interactions remain untested.
- [ ] Get the user's feedback before any application implementation.

The companion's bundled Node.js server conflicts with the project's stack boundary. Use the already installed Caddy only for this temporary static preview. Keep the future application server in C++/Crow.

## Live preview switch

- The user explicitly approved switching the existing tunnel to the working application after implementation.
- Caddy now strips only the existing secret path and proxies to the C++ server on loopback port 8080. Static mockup files remain intact but are no longer served.
- The proxy rejects foreign Origin headers and caps request bodies at 1 MiB. Host/Origin adaptation happens only after public-origin validation. Caddy's admin API remains disabled.
- `tests/tunnel_test.py` passed through the public HTTPS URL: live browser assets, C++ KMP fixture, local playback, rejected foreign origin/oversized body, and inaccessible bare/project-file paths.
- Anyone with the secret URL can edit the process-local dictionary. This is temporary link-based access, not a public production deployment or account authentication.

Only the content directory is served, under an unguessable path. Directory browsing and the Caddy admin API are disabled. This link is a bearer secret, not account authentication. Stop the preview and tunnel after the design session.
