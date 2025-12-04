#ifndef __EJFONT_H
#define __EJFONT_H	1

#include "EJTexture.h"

class EJCanvasContext;

typedef struct
{
    uint32_t codepoint;
    float kerning;
} kerning_t;

typedef struct
{
    uint32_t codepoint;
    int width, height;
    int offset_x, offset_y;
    float advance_x, advance_y;
    float s0, t0, s1, t1;
    size_t kerning_count;
    kerning_t kerning[1];
} texture_glyph_t;

typedef struct
{
    size_t tex_width;
    size_t tex_height;
    size_t tex_depth;
    unsigned char tex_data[65536];
    float size;
    float height;
    float linegap;
    float ascender;
    float descender;
	float hanging;
    size_t glyphs_count;
    texture_glyph_t glyphs[102];
} texture_font_t;

class EJFont {
private:
    texture_font_t* _font;
    bool _copy;
    bool isFilled;
    float _scale;
    uint32_t* _utf32buffer;
    size_t utf32bufsize;
    EJTexture* _texture;
    bool _textureInitialized;

    void ensureTextureInitialized();
    float measureStringFromBuffer(int length);

public:
    EJFont(const char* font, int size, bool useFill, float cs);
    ~EJFont();

    void drawString(const char* utf8string, EJCanvasContext* toContext, float pen_x, float pen_y);
    float measureString(const char* utf8string);
};

#endif
