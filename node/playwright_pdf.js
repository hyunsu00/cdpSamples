const playwright = require('playwright');

(async () => {
  try {
    const browser = await playwright.chromium.launch();
    const page = await browser.newPage();
    await page.goto('https://www.naver.com/');
    await page.pdf({ path: 'naver.pdf', format: 'A4'});
    await browser.close();
  } catch (error) {
    console.log('htmlToPdfFail :', error);
  }
})();