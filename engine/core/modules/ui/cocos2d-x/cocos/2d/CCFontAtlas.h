/****************************************************************************
 Copyright (c) 2013      Zynga Inc.
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#ifndef _CCFontAtlas_h_
#define _CCFontAtlas_h_

/// @cond DO_NOT_SHOW

#include <EASTL/string.h>
#include <unordered_map>

#include "platform/CCPlatformMacros.h"
#include "base/CCRef.h"
#include "platform/CCStdC.h" // ssize_t on windows
#include "renderer/CCTexture2D.h"

NS_CC_BEGIN

class Font;
class EventCustom;
class EventListenerCustom;
class FontFreeType;

struct FontLetterDefinition
{
    float U;
    float V;
    float width;
    float height;
    float offsetX;
    float offsetY;
    int textureID;
    bool validDefinition;
    int xAdvance;
    bool rotated;
};

class CC_DLL FontAtlas : public Ref
{
public:
    static const int CacheTextureWidth;
    static const int CacheTextureHeight;
    static const char* CMD_PURGE_FONTATLAS;
    static const char* CMD_RESET_FONTATLAS;
    /**
     * @js ctor
     */
    FontAtlas(Font &theFont);
    /**
     * @js NA
     * @lua NA
     */
    virtual ~FontAtlas();
    
    void addLetterDefinition(char32_t utf32Char, const FontLetterDefinition &letterDefinition);
    bool getLetterDefinitionForChar(char32_t utf32Char, FontLetterDefinition &letterDefinition);
    
    bool prepareLetterDefinitions(const eastl::u32string& utf16String);

    const std::unordered_map<ssize_t, Texture2D*>& getTextures() const { return _atlasTextures; }
    void  addTexture(Texture2D *texture, int slot);
    float getLineHeight() const { return _lineHeight; }
    void  setLineHeight(float newHeight);
    
    std::string getFontName() const;

    Texture2D* getTexture(int slot);
    const Font* getFont() const { return _font; }

    /** listen the event that renderer was recreated on Android/WP8
     It only has effect on Android and WP8.
     */
    void listenRendererRecreated(EventCustom *event);
    
    /** Removes textures atlas.
     It will purge the textures atlas and if multiple texture exist in the FontAtlas.
     */
    void purgeTexturesAtlas();

    /** sets font texture parameters:
     - GL_TEXTURE_MIN_FILTER = GL_LINEAR
     - GL_TEXTURE_MAG_FILTER = GL_LINEAR
     */
     void setAntiAliasTexParameters();

     /** sets font texture parameters:
     - GL_TEXTURE_MIN_FILTER = GL_NEAREST
     - GL_TEXTURE_MAG_FILTER = GL_NEAREST
     */
     void setAliasTexParameters();

protected:
    void reset();
    
    void reinit();
    
    void releaseTextures();

    void findNewCharacters(const eastl::u32string& u32Text, std::unordered_map<unsigned int, unsigned int>& charCodeMap);

    void conversionU32TOGB2312(const eastl::u32string& u32Text, std::unordered_map<unsigned int, unsigned int>& charCodeMap);

    void initTextureWithZeros(Texture2D *texture);

    /**
     * Scale each font letter by scaleFactor.
     *
     * @param scaleFactor A float scale factor for scaling font letter info.
     */
    void scaleFontLetterDefinition(float scaleFactor);
    
    void updateTextureContent(backend::PixelFormat format, int startY);

    std::unordered_map<ssize_t, Texture2D*> _atlasTextures;
    std::unordered_map<char32_t, FontLetterDefinition> _letterDefinitions;
    float _lineHeight = 0.f;
    Font* _font = nullptr;
    FontFreeType* _fontFreeType = nullptr;
    void* _iconv = nullptr;

    // Dynamic GlyphCollection related stuff
    int _currentPage = 0;
    unsigned char *_currentPageData = nullptr;
    unsigned char *_currentPageDataRGBA = nullptr;
    int _currentPageDataSize = 0;
    int _currentPageDataSizeRGBA = 0;
    float _currentPageOrigX = 0;
    float _currentPageOrigY = 0;
    int _letterPadding = 0;
    int _letterEdgeExtend = 0;

    int _fontAscender = 0;
    EventListenerCustom* _rendererRecreatedListener = nullptr;
    bool _antialiasEnabled = true;
    int _currLineHeight = 0;

    friend class Label;
};

NS_CC_END

/// @endcond
#endif /* defined(__cocos2d_libs__CCFontAtlas__) */
