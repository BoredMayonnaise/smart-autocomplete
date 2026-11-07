"""Optional browser acceptance test. Requires Python Playwright, not an application service."""
import os
import re
from playwright.sync_api import sync_playwright, expect

with sync_playwright() as playwright:
    browser = playwright.chromium.launch(headless=True)
    page = browser.new_page(viewport={"width": 1440, "height": 1000})
    failures = []
    page.on("pageerror", lambda error: failures.append(str(error)))
    trace_requests = []
    external_requests = []
    page.on("request", lambda request: trace_requests.append(request.url) if request.url.endswith("/api/trace") else None)
    page.on("request", lambda request: external_requests.append(request.url) if not request.url.startswith(os.environ.get("SMART_URL", "http://127.0.0.1:8080")) else None)
    page.goto(os.environ.get("SMART_URL", "http://127.0.0.1:8080"))
    page.wait_for_load_state("networkidle")
    expect(page.get_by_label("Editor", exact=True)).to_be_visible()
    assert page.locator("header").count() == 0
    expect(page.locator("#scrub")).to_be_enabled()

    def end_trace():
        expect(page.locator("#scrub")).to_be_enabled()
        page.locator("#scrub").evaluate("node => { node.value=node.max; node.dispatchEvent(new Event('input', {bubbles:true})); }")

    end_trace()
    expect(page.locator("#result-summary")).to_contain_text("algorithmic")
    expect(page.locator("#analytics")).to_contain_text("5")
    page.get_by_role("button", name="Restart", exact=True).click()
    expect(page.locator("#progress-label")).to_have_text(re.compile(r"0 / \d+"))
    before = len(trace_requests)
    page.get_by_role("button", name="Step", exact=True).click()
    expect(page.locator("#progress-label")).to_have_text(re.compile(r"1 / \d+"))
    assert len(trace_requests) == before, "Step must not compute a new trace"
    page.get_by_role("button", name="Flow", exact=True).click()
    expect(page.locator("#canvas")).to_contain_text("Observed operations")
    page.get_by_role("button", name="Structure", exact=True).click()
    page.get_by_label("Playback speed").select_option("4")
    page.get_by_role("button", name="▶ Play", exact=True).click()
    expect(page.get_by_role("button", name="Ⅱ Pause", exact=True)).to_be_visible()
    page.get_by_role("button", name="Ⅱ Pause", exact=True).click()

    page.locator('#example').select_option('insert')
    expect(page.locator("#scene-title")).to_contain_text("Levenshtein")
    end_trace()
    expect(page.locator("#result-summary")).to_have_text("Distance 1 · 1 edit")
    expect(page.locator(".transformed")).to_have_text("programming")
    page.get_by_role("button", name="Restart", exact=True).click()
    assert page.locator(".matrix td.pending").count() > 0
    assert page.locator(".transformed").count() == 0
    page.locator('#example').select_option('transpose')
    end_trace()
    expect(page.locator("#result-summary")).to_have_text("Distance 2 · 2 edits")
    page.locator('#example').select_option('distance')
    end_trace()
    expect(page.locator("#result-summary")).to_have_text("Distance 3 · 3 edits")

    page.locator('#example').select_option('overlap')
    expect(page.locator("#find-count")).to_have_text("5 matches")
    end_trace()
    expect(page.locator("#result-summary")).to_contain_text("starts 1, 3, 11, 16, 18")
    page.get_by_label("Next match", exact=True).click()
    assert page.locator("#editor").evaluate("node => [node.selectionStart,node.selectionEnd]") == [1, 4]
    page.locator('#example').select_option('stress')
    end_trace()
    expect(page.locator("#analytics")).to_contain_text("9 preprocessing + 68 search = 77")
    expect(page.locator("#analytics")).to_contain_text("192")
    expect(page.locator("#analytics")).to_contain_text("KMP uses 115 fewer comparisons.")
    page.get_by_role("button", name="Restart", exact=True).click()
    expect(page.locator("#analytics")).to_contain_text("KMP count is partial")
    expect(page.locator("#analytics")).not_to_contain_text("KMP uses 115 fewer")
    end_trace()
    page.screenshot(path="/tmp/opencode/smartautocomplete-kmp.png", full_page=True)
    page.locator('#editor').fill('abc')
    page.locator('#pattern').fill('abc')
    page.locator('#run').click()
    end_trace()
    expect(page.locator('#analytics')).to_contain_text('KMP uses 2 more comparisons.')
    page.get_by_label("Find pattern", exact=True).fill("")
    expect(page.locator("#find-count")).to_have_text("0 matches")
    expect(page.locator("#scrub")).to_be_disabled()

    page.locator('#example').select_option('prefix')
    expect(page.locator("#completions .suggestion").first).to_be_visible()
    page.get_by_label("Editor", exact=True).fill("Hello, ALGOR!")
    page.locator("#editor").evaluate("node => { node.setSelectionRange(12,12); node.dispatchEvent(new Event('select')); }")
    expect(page.locator("#completions .suggestion").first).to_contain_text("algorithm")
    page.get_by_label("Editor", exact=True).press("ArrowDown")
    page.get_by_label("Editor", exact=True).press("Tab")
    expect(page.get_by_label("Editor", exact=True)).to_have_value("Hello, ALGORITHM!")

    page.get_by_label("Editor", exact=True).fill("zz")
    expect(page.locator("#analysis-metrics")).to_contain_text("Fuzzy:")
    expect(page.locator("#analysis-metrics")).to_contain_text("pruned rows")
    page.get_by_label("Editor", exact=True).fill("a" * 33)
    expect(page.locator("#token-status")).to_have_text("Word too long")
    expect(page.locator("#analysis-metrics")).to_be_empty()
    expect(page.locator("#corrections")).to_contain_text("32 code points")
    page.get_by_label("Editor", exact=True).fill("ca")

    page.set_viewport_size({"width": 390, "height": 844})
    assert page.evaluate("document.documentElement.scrollWidth <= innerWidth"), "No page-wide mobile overflow"
    page.screenshot(path="/tmp/opencode/smartautocomplete-mobile.png", full_page=True)
    page.set_viewport_size({"width": 1440, "height": 1000})
    page.screenshot(path="/tmp/opencode/smartautocomplete-desktop.png", full_page=True)
    browser.close()
    assert not failures, failures
    assert not external_requests, external_requests
    print("Browser checks passed")
