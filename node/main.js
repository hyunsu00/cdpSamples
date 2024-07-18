// main.js
const ConvertHtmlModule = require('./ConvertHtmlModule.js');

// HtmltoImage 메서드 호출
const resultImage = ConvertHtmlModule.HtmltoImage(
    'https://www.naver.com',
    'HtmlToImage.png',
    'png',
    -1,
    -1,
    -1,
    -1,
    -1,
    -1
);
console.log('HtmltoImage result:', resultImage);

// HtmlToPdf 메서드 호출
const resultPdf = ConvertHtmlModule.HtmlToPdf(
    'https://www.naver.com',
    'HtmlToPdf.pdf',
    '0.4',
    0
);
console.log('HtmlToPdf result:', resultPdf);