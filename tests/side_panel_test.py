"""Viewport and execution-visual regression checks against an isolated process."""
import os
import re
from playwright.sync_api import sync_playwright, expect

BASE=os.environ.get('SMART_URL','http://127.0.0.1:8082')
with sync_playwright() as playwright:
    browser=playwright.chromium.launch(headless=True)
    page=browser.new_page(viewport={'width':1366,'height':768})
    errors=[]
    page.on('pageerror',lambda error:errors.append(str(error)))
    page.goto(BASE)
    page.wait_for_load_state('networkidle')
    expect(page.locator('#scrub')).to_be_enabled()
    expect(page.locator('.execution-rail #play')).to_be_visible()
    for selector in ['#analytics','#detail','#result-summary','.legend','#speed','#progress-label']:
        assert page.locator('.execution-rail '+selector).count()==1,selector

    def fits():
        assert page.evaluate('document.documentElement.scrollHeight<=innerHeight+1'),'Desktop page needs vertical scrolling'
        assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),'Desktop page needs horizontal scrolling'
        for selector in ['#play','#step','#restart','#scrub','#speed','#analytics','.legend']:
            box=page.locator(selector).bounding_box()
            assert box and box['y']>=0 and box['y']+box['height']<=page.viewport_size['height']+1,(selector,box)
        assert page.locator('#analytics').evaluate("node => node.getBoundingClientRect().bottom <= node.closest('.execution-content').getBoundingClientRect().bottom+1"), 'Measurements must remain visible inside the rail'
        left=page.locator('.input-rail').bounding_box()
        center=page.locator('.stage').bounding_box()
        right=page.locator('.execution-rail').bounding_box()
        assert left['x']<center['x']<right['x']

    for width,height in [(1024,640),(1024,768),(1280,720),(1366,768),(1440,900)]:
        page.set_viewport_size({'width':width,'height':height})
        for example in ['prefix','insert','overlap']:
            page.locator('#example').select_option(example)
            expect(page.locator('#scrub')).to_be_enabled()
            fits()
            page.locator('#scrub').evaluate("node=>{node.value=node.max;node.dispatchEvent(new Event('input'));}")
            fits()
    page.set_viewport_size({'width':1366,'height':768})
    page.locator('#example').select_option('prefix')
    expect(page.locator('#scrub')).to_be_enabled()
    expect(page.locator('.prefix-progress .prefix-letter')).to_have_count(5)
    page.locator('#step').click()
    page.locator('#step').click()
    expect(page.locator('.prefix-letter.visited')).to_have_count(1)
    page.locator('#restart').click()
    expect(page.locator('.prefix-letter.visited')).to_have_count(0)
    page.locator('#scrub').evaluate("node=>{node.value=node.max;node.dispatchEvent(new Event('input'));}")
    endpoint=page.evaluate("trace.events.find(event=>event.kind==='cache_read').node")
    expect(page.locator('.state-chip').filter(has=page.locator('b',has_text='Node')).locator('em')).to_have_text(str(endpoint))
    page.locator('#restart').click()
    page.screenshot(path='/tmp/opencode/smart-side-panels-trie.png',full_page=True)

    page.locator('#example').select_option('insert')
    expect(page.locator('#scrub')).to_be_enabled()
    page.wait_for_function("trace && trace.algorithm==='dp'")
    cell=page.evaluate("trace.events.findIndex(event=>event.kind==='dp_cell')+1")
    page.locator('#scrub').evaluate("(node,value)=>{node.value=value;node.dispatchEvent(new Event('input'));}",cell)
    expect(page.locator('.cost-choice')).to_have_count(3)
    expect(page.locator('.cost-choice.chosen')).to_have_count(1)
    expect(page.locator('.matrix td.chosen')).to_have_count(1)
    for label in ['i','j']:
        expect(page.locator('.state-chip').filter(has=page.locator('b',has_text=re.compile('^'+label+'$'))).locator('em')).to_have_text('1')
    expect(page.locator('.cost-choice.chosen')).to_contain_text('Match')
    page.screenshot(path='/tmp/opencode/smart-side-panels-dp.png',full_page=True)

    page.locator('#example').select_option('stress')
    expect(page.locator('#scrub')).to_be_enabled()
    page.wait_for_function("trace && trace.algorithm==='kmp'")
    comparison=page.evaluate("trace.events.findIndex(event=>event.kind==='kmp_compare')+1")
    page.locator('#scrub').evaluate("(node,value)=>{node.value=value;node.dispatchEvent(new Event('input'));}",comparison)
    expect(page.locator('.char.text-pointer')).to_have_count(1)
    expect(page.locator('.char.pattern-pointer')).to_have_count(1)
    fits()
    page.screenshot(path='/tmp/opencode/smart-side-panels-kmp.png',full_page=True)
    page.get_by_role('button',name='Flow',exact=True).click()
    fits()
    for width,height in [(375,812),(844,390),(1280,600)]:
        page.set_viewport_size({'width':width,'height':height})
        assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),(width,height)
        page.locator('#play').scroll_into_view_if_needed()
        expect(page.locator('#play')).to_be_visible()
    browser.close()
    assert not errors,errors
print('Side-panel layout and trace-visual checks passed')
