#ifndef __CONVERT_HTML_MODULE_H__
#define __CONVERT_HTML_MODULE_H__

class ConvertHtmlModule 
{
public:
    static bool HtmlToImage(
        const hncstd::wstring& htmlURL, 
        const hncstd::wstring& resultFilePath, 
        const hncstd::wstring& imageType, 
        int clipX, 
        int clipY, 
        int clipWidth, 
        int clipHeight, 
        int viewportWidth, 
        int vieweportHeight
    );

    static bool HtmlToPdf(
        const hncstd::wstring& htmlURL,
        const hncstd::wstring& resultFilePath,
        const hncstd::wstring& margin,
        int isLandScape
    );
};

#endif // #ifndef __CONVERT_HTML_MODULE_H__

