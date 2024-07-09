#!/usr/bin/env python3.7
from playwright.sync_api import sync_playwright

def HtmlToImage(htmlURL, resultFilePath, imageType, clipX, clipY, clipWidth, clipHeight, viewportWidth, viewportHeight):
    with sync_playwright() as playwright:
        try:
            chromium = playwright.chromium
            browser = chromium.launch()
            page = browser.new_page()
            page.set_default_navigation_timeout(5000)
            page.goto(htmlURL)
            if viewportWidth > -1 or viewportHeight > -1:
                print('apply viewport')
                viewPort = dict()
                if viewportWidth > -1:
                    viewPort["width"] = viewportWidth
                if viewportWidth > -1:
                    viewPort["height"] = viewportHeight
                page.set_viewport_size(viewport_size=viewPort)
            if clipX > -1 or clipY > -1 or clipWidth > -1 or clipHeight > -1:
                print('apply clip')
                cliptemp = dict()
                if clipX > -1:
                    cliptemp["x"] = clipX
                if clipY > -1:
                    cliptemp["y"] = clipY
                if clipWidth > -1:
                    cliptemp["width"] = clipWidth
                if clipHeight > -1:
                    cliptemp["height"] = clipHeight
                page.screenshot(path=resultFilePath, type=imageType, full_page=True, clip=cliptemp)
            else:
                print('not apply clip')
                page.screenshot(path=resultFilePath, type=imageType, full_page=True)
            page.close()
        except Exception as e:
            print('htmlToImageFailFromPlaywright : %s' % (e))
            return False
        
def HtmlToPdf(htmlURL, resultFilePath, margin, isLandScape):
    with sync_playwright() as playwright:
        try:
            print('in HtmlToPdf, margin = %s, isLandScape=%d' % (margin, isLandScape))
            chromium = playwright.chromium
            browser = chromium.launch()
            page = browser.new_page()
            page.set_default_navigation_timeout(5000)
            page.goto(htmlURL)
            landScapeTemp=False
            if isLandScape == 1:
                landScapeTemp=True 
            if margin != "":
                marginTemp = dict()
                marginTemp['top'] = margin
                marginTemp['right'] = margin
                marginTemp['bottom'] = margin
                marginTemp['left'] = margin
                page.pdf(path=resultFilePath, margin=marginTemp, landscape=landScapeTemp)
            else:
                page.pdf(path=resultFilePath, landscape=landScapeTemp)
            page.close()
        except Exception as e:
            print('htmlToPdfFailFromPlaywright : %s' % (e))
            return False
         
if __name__ == "__main__":
    # 1. HtmlToImage 테스트
    HtmlToImage("https://www.naver.com/", "naver.png", "png", -1, -1, -1, -1, -1, -1)
    # 2. HtmlToPdf 테스트
    HtmlToPdf("https://www.naver.com/", "naver.pdf", "", 0)