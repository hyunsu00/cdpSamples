from playwright.sync_api import sync_playwright

with sync_playwright() as playwright:
    try:
        chromium = playwright.chromium
        browser = chromium.launch()
        page = browser.new_page()
        page.goto('https://www.naver.com/')
        page.screenshot(path="naver.png", type="png", full_page=True)
        page.close()
    except Exception as e:
        print('htmlToImageFail : %s' % (e))