const playwright = require('playwright');

(async () => {
  try {
    const browser = await playwright.chromium.launch();
    const context = await browser.newContext();
    const page = await context.newPage();
    await page.goto('https://www.naver.com/');
    await page.screenshot({ path: 'naver.png', type: 'png', fullPage: true });
    await browser.close();
  } catch (error) {
    console.log('htmlToImageFail :', error);
  }
})();