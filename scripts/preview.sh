#!/usr/bin/env bash
# Run an isolated local app and temporary, private-path Cloudflare preview.
set -euo pipefail
umask 077

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
for program in caddy cloudflared curl openssl; do
  command -v "$program" >/dev/null || { echo "Missing prerequisite: $program" >&2; exit 1; }
done
[[ -x "$project_root/build/smartautocomplete" ]] || { echo 'Build the application first; see README.md.' >&2; exit 1; }
runtime=$(mktemp -d /tmp/smartautocomplete-preview.XXXXXXXX)
children=()
cleanup() {
  trap - EXIT
  for child in "${children[@]}"; do kill "$child" 2>/dev/null || true; done
  for child in "${children[@]}"; do wait "$child" 2>/dev/null || true; done
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
fail() { echo "$1 Logs: $runtime" >&2; exit 1; }

"$project_root/build/smartautocomplete" --port 8082 --root "$project_root" >"$runtime/app.log" 2>&1 &
app_pid=$!; children+=("$app_pid")
sleep 1
kill -0 "$app_pid" 2>/dev/null || { cat "$runtime/app.log" >&2; fail 'Local app could not start.'; }
curl --noproxy '*' --fail --silent --max-time 5 http://127.0.0.1:8082/health >"$runtime/local-health.json" || fail 'Local health check failed.'
echo 'Local app: http://127.0.0.1:8082/'

# A separate config avoids inheriting any existing named-tunnel configuration.
printf '{}\n' >"$runtime/cloudflared.yml"
cloudflared tunnel --config "$runtime/cloudflared.yml" --no-autoupdate --metrics 127.0.0.1:0 --url http://127.0.0.1:8318 >"$runtime/tunnel.log" 2>&1 &
tunnel_pid=$!; children+=("$tunnel_pid")
public_origin=''
for ((attempt=0; attempt<120; attempt++)); do
  kill -0 "$app_pid" 2>/dev/null || fail 'Local app exited.'
  kill -0 "$tunnel_pid" 2>/dev/null || fail 'Tunnel exited.'
  public_origin=$(sed -nE 's/.*(https:\/\/[a-z0-9-]+\.trycloudflare\.com).*/\1/p' "$runtime/tunnel.log" | head -n 1)
  [[ -z "$public_origin" ]] || break
  sleep 0.5
done
[[ -n "$public_origin" ]] || fail 'Tunnel did not provide a URL within 60 seconds.'
private_path=$(openssl rand -hex 16)
cat >"$runtime/Caddyfile" <<EOF
{
  admin off
  auto_https off
}
:8318 {
  bind 127.0.0.1
  header {
    Referrer-Policy no-referrer
    X-Content-Type-Options nosniff
    X-Frame-Options DENY
    Cache-Control no-store
  }
  request_body {
    max_size 1048576
  }
  @foreign_origin {
    header Origin *
    not header Origin $public_origin
  }
  route {
    respond @foreign_origin "Forbidden origin" 403
    handle_path /$private_path/* {
      reverse_proxy 127.0.0.1:8082 {
        header_up Host 127.0.0.1:8082
        # Remove Origin only after validating the public origin above.
        header_up -Origin
      }
    }
    handle {
      respond "Not found" 404
    }
  }
}
EOF
caddy validate --config "$runtime/Caddyfile" --adapter caddyfile >"$runtime/validate.log" 2>&1 || fail 'Proxy configuration is invalid.'
caddy run --config "$runtime/Caddyfile" --adapter caddyfile >"$runtime/proxy.log" 2>&1 &
proxy_pid=$!; children+=("$proxy_pid")
sleep 1
kill -0 "$proxy_pid" 2>/dev/null || fail 'Proxy could not start.'
curl --noproxy '*' --fail --silent --max-time 5 "http://127.0.0.1:8318/$private_path/health" >/dev/null || fail 'Proxy health check failed.'
preview_url="$public_origin/$private_path/"
curl --fail --silent --retry 10 --retry-all-errors --retry-delay 2 --connect-timeout 5 --max-time 10 "${preview_url}health" >"$runtime/public-health.json" || fail 'Public health check failed.'
cmp -s "$runtime/local-health.json" "$runtime/public-health.json" || fail 'Public health response does not match the local app.'
for asset in index.html style.css app.js; do
  route="static/$asset"
  [[ "$asset" != index.html ]] || route=''
  curl --fail --silent --connect-timeout 5 --max-time 15 "$preview_url$route" >"$runtime/$asset" || fail "Cannot load public $asset."
  cmp -s "$project_root/static/$asset" "$runtime/$asset" || fail "Public $asset does not match the current source."
done
echo "Public preview: $preview_url"
echo 'Anyone with this link can edit the process-local dictionary. Ctrl+C stops this preview.'
echo "Logs: $runtime"
wait -n "$app_pid" "$proxy_pid" "$tunnel_pid"
fail 'A preview process exited.'
