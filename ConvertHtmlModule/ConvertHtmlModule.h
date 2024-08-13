#ifndef __CONVERT_HTML_MODULE_H__
#define __CONVERT_HTML_MODULE_H__

class ConvertHtmlModule 
{
public:
    static std::wstring GetChromePath();
    static void SetChromePath(const std::wstring& chromePath);

    static bool HtmlToImage(
        const std::wstring& htmlURL,
        const std::wstring& resultFilePath,
        const std::wstring& imageType,
        int clipX,
        int clipY,
        int clipWidth,
        int clipHeight,
        int viewportWidth,
        int vieweportHeight
    );

    static bool HtmlToPdf(
        const std::wstring& htmlURL,
        const std::wstring& resultFilePath,
        const std::wstring& margin,
        int isLandScape
    );
};

#endif // #ifndef __CONVERT_HTML_MODULE_H__