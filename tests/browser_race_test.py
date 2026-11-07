"""Focused replay and response-order regressions against the real Crow API."""
import asyncio
import os
from playwright.async_api import async_playwright, expect

BASE = os.environ.get("SMART_URL", "http://127.0.0.1:8080").rstrip("/")


async def reset(context, words=None):
    current = await (await context.request.get(BASE + "/api/dictionary")).json()
    response = await context.request.put(BASE + "/api/dictionary", data={
        "words": words or ["algorithm", "algorithmic", "algorithms", "cat", "cart"],
        "dictionary_version": current["dictionary_version"], "revision": 0, "request_id": "test-reset"
    })
    assert response.status == 200


async def ready(browser):
    context = await browser.new_context()
    await reset(context)
    page = await context.new_page()
    await page.goto(BASE)
    await page.wait_for_load_state("networkidle")
    await expect(page.locator("#scrub")).to_be_enabled()
    return context, page


async def lps_pointers(browser):
    context, page = await ready(browser)
    await page.locator('[data-mode="kmp"]').click()
    await page.locator("#editor").fill("aaaa")
    await page.locator("#pattern").fill("aaa")
    async with page.expect_response(lambda response: response.url.endswith("/api/trace")) as pending:
        await page.locator("#run").click()
    data = await (await pending.value).json()
    await expect(page.locator("#scrub")).to_be_enabled()
    write = next(index for index, event in enumerate(data["events"]) if event["kind"] == "lps_write" and event["value"] == 1)
    await page.locator("#scrub").evaluate("(node, value) => {node.value=value;node.dispatchEvent(new Event('input'));}", write + 1)
    await expect(page.locator("#detail")).to_contain_text("i = 1 · j = 1")
    await page.locator("#scrub").evaluate("node => {node.value=2;node.dispatchEvent(new Event('input'));}")
    await expect(page.locator("#canvas")).to_contain_text("LPS preprocessing")
    await context.close()


async def failed_accept_cancels_analysis(browser):
    context, page = await ready(browser)
    started, release, finished = asyncio.Event(), asyncio.Event(), asyncio.Event()

    async def hold(route):
        response = await route.fetch()
        started.set()
        await release.wait()
        try:
            await route.fulfill(response=response)
        except Exception:
            pass  # Cancellation is the behavior this test expects.
        finished.set()

    await page.route("**/api/analyze", hold)
    await page.route("**/api/accept", lambda route: route.abort())
    await page.locator("#run").click()
    await asyncio.wait_for(started.wait(), 5)
    await page.evaluate("void accept('algorithm')")
    await expect(page.locator("#notice")).to_be_visible()
    release.set()
    await asyncio.wait_for(finished.wait(), 5)
    await page.wait_for_load_state("networkidle")
    await expect(page.locator(".suggestion")).to_have_count(0)
    await expect(page.locator("#scrub")).to_be_disabled()
    await context.close()


async def stale_accept_ignored(browser):
    context, page = await ready(browser)
    started, release, finished = asyncio.Event(), asyncio.Event(), asyncio.Event()

    async def hold(route):
        response = await route.fetch()
        started.set()
        await release.wait()
        await route.fulfill(response=response)
        finished.set()

    await page.route("**/api/accept", hold)
    await page.locator("#completions .suggestion").first.click()
    await asyncio.wait_for(started.wait(), 5)
    await reset(context, ["cat", "cart"])
    newer = await (await context.request.get(BASE + "/api/dictionary")).json()
    # Observe the real newer snapshot without changing the editor revision via focus events.
    await page.evaluate("version => observeVersion(version)", newer["dictionary_version"])
    release.set()
    await asyncio.wait_for(finished.wait(), 5)
    await page.wait_for_load_state("networkidle")
    await expect(page.locator("#editor")).to_have_value("algor")
    await context.close()


async def stale_dictionary_ignored(browser):
    context, page = await ready(browser)
    started, release, finished = asyncio.Event(), asyncio.Event(), asyncio.Event()

    async def hold(route):
        if route.request.method != "PUT":
            await route.continue_()
            return
        response = await route.fetch()
        started.set()
        await release.wait()
        await route.fulfill(response=response)
        finished.set()

    await page.route("**/api/dictionary", hold)
    await page.locator("#open-dictionary").click()
    await page.locator("#dictionary-words").fill("dog")
    await page.locator("#save-dictionary").click()
    await asyncio.wait_for(started.wait(), 5)
    await reset(context, ["cat", "cart", "dog"])
    await page.locator("#close-dictionary").click()
    await page.locator("#open-dictionary").click()
    await expect(page.locator("#word-count")).to_have_text("3")
    release.set()
    await asyncio.wait_for(finished.wait(), 5)
    await page.wait_for_load_state("networkidle")
    await expect(page.locator("#dictionary-dialog")).to_be_visible()
    await expect(page.locator("#word-count")).to_have_text("3")
    await context.close()


async def flow_loop_visible(browser):
    context, page = await ready(browser)
    await page.locator('[data-view="flow"]').click()
    heights = await page.locator("#canvas path.edge").evaluate_all("paths => paths.map(path => path.getBBox().height)")
    assert heights and all(height > 1 for height in heights), "Every control-flow edge, including a self-loop, must have visible vertical extent"
    await page.locator('#example').select_option('insert')
    await expect(page.locator("#scrub")).to_be_enabled()
    paths = await page.locator("#canvas path.edge").evaluate_all("paths => paths.map(path => path.getAttribute('d'))")
    assert len(paths) == len(set(paths)), "Parallel True/False branches must not overlap exactly"
    await context.close()


async def initial_previous_match(browser):
    context, page = await ready(browser)
    await page.locator('#example').select_option('overlap')
    await expect(page.locator("#find-count")).to_have_text("5 matches")
    await page.locator("#previous").click()
    selection = await page.locator("#editor").evaluate("node => [node.selectionStart,node.selectionEnd]")
    assert selection == [18, 21], "Previous from no selection must choose the last match"
    await context.close()


async def initial_up_suggestion(browser):
    context, page = await ready(browser)
    await page.locator("#editor").focus()
    await page.locator("#editor").press("ArrowUp")
    await expect(page.locator('.suggestion[aria-pressed="true"]')).to_contain_text("algorithms")
    await context.close()


async def example_keyboard_navigation(browser):
    context, page = await ready(browser)
    await page.locator('#example').focus()
    for expected in ['prefix','insert','transpose']:
        await page.keyboard.press('ArrowDown')
        await expect(page.locator('#example')).to_have_value(expected)
    await page.locator('#source').fill('another')
    await expect(page.locator('#example')).to_have_value('')
    await context.close()


async def dictionary_error_not_masked(browser):
    context, page = await ready(browser)
    await page.route('**/api/analyze',lambda route:route.fulfill(status=503,content_type='application/json',body='{"error":{"message":"Analysis unavailable"}}'))
    await page.locator('#open-dictionary').click()
    await page.locator('#dictionary-words').fill('cat\ncart')
    await page.locator('#save-dictionary').click()
    await page.wait_for_load_state('networkidle')
    await expect(page.locator('#notice')).to_contain_text('Analysis unavailable')
    await expect(page.locator('#notice')).to_have_attribute('role','alert')
    await context.close()


async def empty_flow_hides_branch_control(browser):
    context, page = await ready(browser)
    await page.locator('#editor').fill('changed')
    await expect(page.locator('#scrub')).to_be_disabled()
    await page.locator('[data-view="flow"]').click()
    await expect(page.locator('#graph-context')).to_be_hidden()
    await context.close()


async def main():
    failed = []
    async with async_playwright() as playwright:
        browser = await playwright.chromium.launch(headless=True)
        for test in [lps_pointers, failed_accept_cancels_analysis, stale_accept_ignored, stale_dictionary_ignored, flow_loop_visible, initial_previous_match, initial_up_suggestion, example_keyboard_navigation, dictionary_error_not_masked, empty_flow_hides_branch_control]:
            try:
                await test(browser)
                print("PASS", test.__name__)
            except Exception as error:
                failed.append(test.__name__)
                print("FAIL", test.__name__, str(error))
        await browser.close()
    assert not failed, failed


asyncio.run(main())
