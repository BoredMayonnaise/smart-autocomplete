"""HTTP transport-boundary checks; uses only the Python standard library."""
import http.client
import json
import os
from urllib.parse import urlsplit

base = urlsplit(os.environ.get("SMART_URL", "http://127.0.0.1:8080"))


def request(method, path, body=None, headers=None, status=200):
    connection = http.client.HTTPConnection(base.hostname, base.port or 80, timeout=10)
    connection.request(method, path, body, headers or {})
    response = connection.getresponse()
    payload = response.read()
    assert response.status == status, (method, path, response.status, payload)
    connection.close()
    return payload, dict(response.getheaders())


payload, headers = request("GET", "/health")
assert json.loads(payload)["status"] == "ok"
assert headers["X-Content-Type-Options"] == "nosniff"
assert headers["X-Frame-Options"] == "DENY"
request("GET", "/health", headers={"Host": "evil.example"}, status=403)
request("GET", "/health", headers={"Origin": "https://evil.example"}, status=403)
request("POST", "/api/search", "{}", {"Content-Type": "text/plain"}, 415)
for path in ["/AGENTS.md", "/static/AGENTS.md", "/static/../AGENTS.md", "/static/%2e%2e/AGENTS.md", "/static/app.js/../../AGENTS.md"]:
    request("GET", path, status=404)
for body in [b"{", b'{"text":"\xc0\x80"}', b"[]"]:
    payload, _ = request("POST", "/api/search", body, {"Content-Type": "application/json"}, 400)
    assert "error" in json.loads(payload)
request("POST", "/api/search", b" " * 1048577, {"Content-Type": "application/json"}, 413)
payload, _ = request("POST", "/api/search", json.dumps({"text": "banana", "pattern": "ana", "revision": 1, "request_id": "http-test"}), {"Content-Type": "application/json"})
assert [match["start"] for match in json.loads(payload)["matches"]] == [1, 3]
print("HTTP checks passed")
