#ifndef __CONVERT_HTML_MODULE_H__
#define __CONVERT_HTML_MODULE_H__

class ConvertHtmlModule 
{
public:
    static bool HtmlToImage(
        const wchar_t* htmlURL, 
        const wchar_t* resultFilePath, 
        const wchar_t* imageType, 
        int clipX, 
        int clipY, 
        int clipWidth, 
        int clipHeight, 
        int viewportWidth, 
        int vieweportHeight
    );

    static bool HtmlToPdf(
        const wchar_t* htmlURL,
        const wchar_t* resultFilePath,
        const wchar_t* margin,
        int isLandScape
    );
};

#endif // #ifndef __CONVERT_HTML_MODULE_H__

