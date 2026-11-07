"""Read-only checks for an explicitly authorized temporary public preview."""
import json
import os
from urllib.error import HTTPError
from urllib.parse import urlsplit
from urllib.request import Request, urlopen
from playwright.sync_api import sync_playwright, expect

BASE = os.environ["SMART_URL"].rstrip("/") + "/"
parts = urlsplit(BASE)
ORIGIN = f"{parts.scheme}://{parts.netloc}"


def fetch(url, status=200, body=None, origin=None):
    headers = {}
    if body is not None:
        headers["Content-Type"] = "application/json"
    if origin:
        headers["Origin"] = origin
    request = Request(url, body, headers)
    try:
        response = urlopen(request, timeout=30)
    except HTTPError as error:
        response = error
    data = response.read()
    assert response.code == status, (url, response.code, data[:200])
    return data


assert json.loads(fetch(BASE + "health"))["status"] == "ok"
fetch(ORIGIN + "/", 404)
fetch(ORIGIN + "/AGENTS.md", 404)
fetch(BASE + "static/../AGENTS.md", 404)
payload = json.dumps({"text": "banana", "pattern": "ana", "revision": 1, "request_id": "tunnel-check"}).encode()
result = json.loads(fetch(BASE + "api/search", body=payload, origin=ORIGIN))
assert [match["start"] for match in result["matches"]] == [1, 3]
fetch(BASE + "api/search", 403, payload, "https://evil.example")
fetch(BASE + "api/search", 413, b" " * 1048577, ORIGIN)

with sync_playwright() as playwright:
    browser = playwright.chromium.launch(headless=True)
    page = browser.new_page(viewport={"width": 1440, "height": 1000})
    failures = []
    trace_requests = []
    page.on("pageerror", lambda error: failures.append(str(error)))
    page.on("request", lambda request: trace_requests.append(request.url) if request.url.endswith("/api/trace") else None)
    page.goto(BASE)
    page.wait_for_load_state("networkidle")
    expect(page.get_by_label("Editor", exact=True)).to_be_visible()
    expect(page.locator("#scrub")).to_be_enabled()
    before = len(trace_requests)
    page.get_by_role("button", name="Step", exact=True).click()
    assert len(trace_requests) == before
    page.locator('#example').select_option('stress')
    expect(page.locator("#scrub")).to_be_enabled()
    page.locator("#scrub").evaluate("node => {node.value=node.max;node.dispatchEvent(new Event('input'));}")
    expect(page.locator("#analytics")).to_contain_text("9 preprocessing + 68 search = 77")
    expect(page.locator("#analytics")).to_contain_text("192")
    assert page.locator("header").count() == 0
    browser.close()
    assert not failures, failures
print("Public tunnel HTTP, origin/body limits, live assets, and playback checks passed")
