#!/usr/bin/env python3
from playwright.sync_api import sync_playwright
import logging
import time

logging.basicConfig(level=logging.DEBUG)  # 로깅 레벨 설정

def HtmlToImage(htmlURL, resultFilePath, imageType):
    with sync_playwright() as playwright:
        try:
            chromium = playwright.chromium
            browser = chromium.launch()
            page = browser.new_page()
            page.goto(htmlURL)
            page.screenshot(path=resultFilePath, type=imageType, full_page=True)
            page.close()
        except Exception as e:
            print('htmlToImageFail : %s' % (e))
            

def HtmlToPdf(htmlURL, resultFilePath):
    with sync_playwright() as playwright:
        try:
            chromium = playwright.chromium
            browser = chromium.launch()
            page = browser.new_page()
            page.goto(htmlURL)
            page.pdf(path=resultFilePath)
            page.close()
        except Exception as e:
            print('htmlToPdfFail : %s' % (e))
    
if __name__ == "__main__":
    # 1. HtmlToImage 테스트
    start_time = time.time()  # 시작 시간을 측정합니다.
    HtmlToImage("https://www.naver.com/", "playwright_naver.png", "png")
    end_time = time.time()  # 종료 시간을 측정합니다.
    elapsed_time = end_time - start_time  # 실행 시간을 계산합니다.
    logging.info("HtmlToImage %s seconds to run.", elapsed_time)

    # 2. HtmlToPdf 테스트
    start_time = time.time()  # 시작 시간을 측정합니다.
    HtmlToPdf("https://www.naver.com/", "playwright_naver.pdf")
    end_time = time.time()  # 종료 시간을 측정합니다.
    elapsed_time = end_time - start_time  # 실행 시간을 계산합니다.
    logging.info("HtmlToPdf : %s seconds to run.", elapsed_time)