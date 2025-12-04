#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "stdlib.h"
#include "mallocdebug.h"
#include "EJFont.h"
#include "EJCanvasContext.h"
#include "fonts/basier-medium-24.h"
#include "fonts/basier-medium-30.h"
#include "fonts/basier-medium-40.h"
#include "GLcompat.h"
#include "NdkMisc.h"
#include <utf8.h>

#define PT_TO_PX(pt) pt
#define LOG_TAG "EJFont"

EJFont::EJFont(const char* font, int size, bool useFill, float cs) {
    // size is in points, calculate number of pixels from that.
    // Points = 1/72 inch, we can assume 2.22 pixel per pt for mdpi
    float pxSize = (float)size * 1.5f;
    float realPxSize = pxSize * cs;

    _copy = false;
    isFilled = useFill;
    _textureInitialized = false;
    _texture = NULL;  // Don't create texture yet - wait for OpenGL context

    // Select appropriate font
    _font = &font_basier_medium_24;
    if (realPxSize >= 40) {
        _font = &font_basier_medium_40;
    } else if (realPxSize >= 30) {
        _font = &font_basier_medium_30;
    }

    // Calculate scale
    if (_font->size != (float)pxSize || cs != 1.0) {
        _scale = (float)pxSize / _font->size;
    } else {
        _scale = 1;
    }

    // Allocate UTF-32 buffer
    utf32bufsize = 4096;
    _utf32buffer = (uint32_t*)malloc(utf32bufsize);

    // Validate font data
    LOGI("Font selected: size=%f, tex_width=%d, tex_height=%d, glyphs=%d, scale=%f",
         _font->size, _font->tex_width, _font->tex_height,
         _font->glyphs_count, _scale);

    if (!_font->tex_data) {
        LOGE("ERROR: Font texture data is NULL!");
    }

    if (_font->tex_width == 0 || _font->tex_height == 0) {
        LOGE("ERROR: Invalid font texture dimensions!");
    }
}

EJFont::~EJFont() {
    if (_copy) {
        free(_font);
    }
    free(_utf32buffer);

    if (_texture) {
        delete _texture;
        _texture = NULL;
    }
}

void EJFont::ensureTextureInitialized() {
    // If already initialized, nothing to do
    if (_textureInitialized && _texture && _texture->textureId != 0) {
        return;
    }

    LOGI("Lazy initializing font texture...");

    // Validate font data before creating texture
    if (!_font->tex_data) {
        LOGE("Cannot initialize texture: font data is NULL");
        return;
    }

    if (_font->tex_width == 0 || _font->tex_height == 0) {
        LOGE("Cannot initialize texture: invalid dimensions (%dx%d)",
             _font->tex_width, _font->tex_height);
        return;
    }

    // Delete old texture if it exists
    if (_texture) {
        delete _texture;
        _texture = NULL;
    }

    // Create new texture
    _texture = EJTexture::initWithWidth(_font->tex_width,
                                        _font->tex_height,
                                        (GLubyte*)_font->tex_data,
                                        GL_ALPHA, 1);

    if (!_texture) {
        LOGE("EJTexture::initWithWidth returned NULL!");
        return;
    }

    if (_texture->textureId == 0) {
        LOGE("Texture created but textureId is 0!");

        // Try manual texture creation as fallback
        GLuint texId;
        glGenTextures(1, &texId);

        if (texId == 0) {
            LOGE("glGenTextures failed! OpenGL context may not be ready.");
            GLenum err = glGetError();
            LOGE("OpenGL error: 0x%x", err);
            return;
        }

        LOGI("Manual texture creation: generated ID %d", texId);

        glBindTexture(GL_TEXTURE_2D, texId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA,
                     _font->tex_width, _font->tex_height,
                     0, GL_ALPHA, GL_UNSIGNED_BYTE,
                     _font->tex_data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            LOGE("OpenGL error after manual texture creation: 0x%x", err);
            glDeleteTextures(1, &texId);
            return;
        }

        // Override the texture ID
        _texture->textureId = texId;
    }

    // Bind and set parameters
    _texture->bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    _textureInitialized = true;
    LOGI("Font texture initialized successfully: textureId=%d", _texture->textureId);
}

void EJFont::drawString(const char* utf8string, EJCanvasContext* toContext, float pen_x, float pen_y) {
    size_t i, j, k;
    const int rawLength = strlen(utf8string);
    const int length = utf8::distance(utf8string, utf8string + rawLength);

    if (length * 4 >= utf32bufsize) {
        utf32bufsize = length * 4 + 4;
        _utf32buffer = (uint32_t*)realloc(_utf32buffer, utf32bufsize);
    }

    utf8::utf8to32(utf8string, utf8string + rawLength, _utf32buffer);

    // Figure out the x position with the current textAlign.
    if (toContext->state->textAlign != kEJTextAlignLeft) {
        float w = measureStringFromBuffer(length);
        if (toContext->state->textAlign == kEJTextAlignRight ||
            toContext->state->textAlign == kEJTextAlignEnd) {
            pen_x -= w;
        } else if (toContext->state->textAlign == kEJTextAlignCenter) {
            pen_x -= w / 2.0f;
        }
    }

    // Figure out the y position with the current textBaseline
    switch (toContext->state->textBaseline) {
        case kEJTextBaselineAlphabetic:
        case kEJTextBaselineIdeographic:
            break;
        case kEJTextBaselineTop:
            pen_y += PT_TO_PX(_font->ascender * _scale);
            break;
        case kEJTextBaselineHanging:
            pen_y += PT_TO_PX(_font->hanging * _scale);
            break;
        case kEJTextBaselineMiddle:
            pen_y += PT_TO_PX(_font->ascender * _scale - (0.5 * _font->height * _scale));
            break;
        case kEJTextBaselineBottom:
            pen_y += PT_TO_PX(_font->descender * _scale);
            break;
    }

    toContext->save();

    // **LAZY INITIALIZATION: Create texture on first draw**
    ensureTextureInitialized();

    // Validate texture before drawing
    if (!_texture || _texture->textureId == 0) {
        LOGE("Cannot draw text: texture not initialized (texture=%p, id=%d)",
             _texture, _texture ? _texture->textureId : -1);
        toContext->restore();
        return;
    }

    // Bind texture for drawing
    _texture->bind();
    toContext->setTexture(_texture);

    LOGI("Drawing string '%s' with textureId=%d, length=%d",
         utf8string, _texture->textureId, length);

    // Draw each glyph
    for (i = 0; i < length; ++i) {
        texture_glyph_t* glyph = 0;

        // Find glyph for this character
        for (j = 0; j < _font->glyphs_count; ++j) {
            if (_font->glyphs[j].codepoint == _utf32buffer[i]) {
                glyph = &(_font->glyphs[j]);
                break;
            }
        }

        if (!glyph) {
            LOGI("Glyph not found for codepoint: %d (char: %c)",
                 _utf32buffer[i], _utf32buffer[i]);
            continue;
        }

        // Debug first character
        if (i == 0) {
            LOGI("First glyph: cp=%d, s0=%f, t0=%f, s1=%f, t1=%f, w=%d, h=%d",
                 glyph->codepoint, glyph->s0, glyph->t0,
                 glyph->s1, glyph->t1, glyph->width, glyph->height);
        }

        float x = (pen_x + glyph->offset_x * _scale);
        float y = (pen_y - glyph->offset_y * _scale);
        float w = (glyph->width * _scale);
        float h = (glyph->height * _scale);

        toContext->pushRectX(x, y, w, h,
                             glyph->s0, glyph->t0,
                             glyph->s1 - glyph->s0, glyph->t1 - glyph->t0,
                             isFilled ? toContext->state->fillColor : toContext->state->strokeColor,
                             toContext->state->transform);

        pen_x += glyph->advance_x * _scale;

        // Find next glyph for kerning
        if (i < length - 1) {
            uint32_t nextCode = _utf32buffer[i + 1];
            for (k = 0; k < glyph->kerning_count; ++k) {
                if (glyph->kerning[k].codepoint == nextCode) {
                    pen_x += (glyph->kerning[k].kerning * _scale);
                    break;
                }
            }
        }
    }

    toContext->restore();
}

float EJFont::measureString(const char* utf8string) {
    size_t i, j, k;
    const int rawLength = strlen(utf8string);
    const int length = utf8::distance(utf8string, utf8string + rawLength);

    if (length * 4 >= utf32bufsize) {
        utf32bufsize = length * 4 + 4;
        _utf32buffer = (uint32_t*)realloc(_utf32buffer, utf32bufsize);
    }

    utf8::utf8to32(utf8string, utf8string + rawLength, _utf32buffer);

    return measureStringFromBuffer(length);
}

float EJFont::measureStringFromBuffer(int length) {
    size_t i, j, k;
    float width = 0.0f;

    for (i = 0; i < length; ++i) {
        texture_glyph_t* glyph = 0;

        for (j = 0; j < _font->glyphs_count; ++j) {
            if (_font->glyphs[j].codepoint == _utf32buffer[i]) {
                glyph = &(_font->glyphs[j]);
                break;
            }
        }

        if (!glyph) {
            continue;
        }

        width += glyph->advance_x * _scale;

        // Find next glyph for kerning
        if (i < length - 1) {
            uint32_t nextCode = _utf32buffer[i + 1];
            for (k = 0; k < glyph->kerning_count; ++k) {
                if (glyph->kerning[k].codepoint == nextCode) {
                    width += (glyph->kerning[k].kerning * _scale);
                    break;
                }
            }
        }
    }

    return width;
}