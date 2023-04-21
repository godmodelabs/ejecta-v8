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

EJFont::EJFont (const char* font, int size, bool useFill, float cs) {

	// size is in points, calculate number of pixels from that.
	// Points = 1/72 inch, we can assume 2.22 pixel per pt for mdpi
	float pxSize = (float)size * 1.5f;
	float realPxSize = pxSize * cs;

	_copy = false;
    _isFilled = useFill;

	_font = &font_basier_medium_24;
	if (realPxSize >= 40) {
		_font = &font_basier_medium_40;
	} else if (realPxSize >= 30) {
		_font = &font_basier_medium_30;
	}

	if (_font->size != (float)pxSize || cs != 1.0) {
		_scale = (float)pxSize / _font->size;
	} else {
		_scale = 1;
	}
	_utf32bufsize = 4096;
	_utf32buffer = (uint32_t*)malloc(_utf32bufsize);
	_texture = EJTexture::initWithWidth(_font->tex_width, _font->tex_height, (GLubyte*)_font->tex_data, GL_ALPHA, 1);
}

EJFont::~EJFont() {
	if (_copy) {
		free(_font);
	}
	free(_utf32buffer);
}

void EJFont::drawString (const char* utf8string, EJCanvasContext* toContext, float pen_x, float pen_y) {
    size_t i, j, k;
    const int rawLength = strlen(utf8string);
    const int length = utf8::distance(utf8string, utf8string + rawLength);

    if (length * 4 >= _utf32bufsize) {
    	_utf32bufsize = length * 4 + 4;
    	_utf32buffer = (uint32_t*)realloc(_utf32buffer, _utf32bufsize);
    }

    utf8::utf8to32(utf8string, utf8string + rawLength, _utf32buffer);

	// Figure out the x position with the current textAlign.
	if(toContext->state->textAlign != kEJTextAlignLeft) {
		float w = measureStringFromBuffer(length);
		if( toContext->state->textAlign == kEJTextAlignRight || toContext->state->textAlign == kEJTextAlignEnd ) {
			pen_x -= w;
		} else if( toContext->state->textAlign == kEJTextAlignCenter ) {
			pen_x -= w/2.0f;
		}
	}

	// Figure out the y position with the current textBaseline
	switch( toContext->state->textBaseline ) {
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
			pen_y += PT_TO_PX(_font->ascender * _scale - (0.5*_font->height * _scale));
			break;
		case kEJTextBaselineBottom:
			pen_y += PT_TO_PX(_font->descender * _scale);
			break;
	}

    toContext->save();
    toContext->setTexture(_texture);
    for (i=0; i<length; ++i) {
        texture_glyph_t *glyph = 0;
        for (j=0; j<_font->glyphs_count; ++j) {
            if (_font->glyphs[j].codepoint == _utf32buffer[i]) {
                glyph = &(_font->glyphs[j]);
                break;
            }
        }
        if (!glyph) {
            continue;
        }

        // Find next glyph for kerning
        if (i < length - 1) {
        	uint32_t nextCode = _utf32buffer[i+1];
            for( k=0; k < glyph->kerning_count; ++k) {
                if (glyph->kerning[k].codepoint == nextCode) {
                    pen_x += (glyph->kerning[k].kerning * _scale);
                    break;
                }
            }
        }

        float x = (pen_x + glyph->offset_x * _scale);
        float y = (pen_y - glyph->offset_y * _scale);
        float w  = (glyph->width * _scale);
        float h  = (glyph->height * _scale);
        toContext->pushRectX(x, y, w, h, glyph->s0, glyph->t0 /* - 1.0f/256 */, glyph->s1 - glyph->s0, glyph->t1 - glyph->t0 /* + 1.0f/256 */, _isFilled ? toContext->state->fillColor : toContext->state->strokeColor, toContext->state->transform);
        pen_x += glyph->advance_x * _scale;
    }
    toContext->restore();
}

float EJFont::measureString (const char* utf8string) {
    size_t i, j, k;

    const int rawLength = strlen(utf8string);
    const int length = utf8::distance(utf8string, utf8string + rawLength);

    if (length * 4 >= _utf32bufsize) {
    	_utf32bufsize = length * 4 + 4;
    	_utf32buffer = (uint32_t*)realloc(_utf32buffer, _utf32bufsize);
    }

    utf8::utf8to32(utf8string, utf8string + rawLength, _utf32buffer);

    float width = 0.0f;
    for (i=0; i<length; ++i) {
        texture_glyph_t *glyph = 0;
        for (j=0; j<_font->glyphs_count; ++j) {
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
        	uint32_t nextCode = _utf32buffer[i+1];
            for (k=0; k < glyph->kerning_count; ++k) {
                if (glyph->kerning[k].codepoint == nextCode) {
                    width += (glyph->kerning[k].kerning * _scale);
                    break;
                }
            }
        }
    }
    return width;
}

float EJFont::measureStringFromBuffer (int length) {
    size_t i, j, k;


    float width = 0.0f;
    for (i=0; i<length; ++i) {
        texture_glyph_t *glyph = 0;
        for (j=0; j<_font->glyphs_count; ++j) {
            if (_font->glyphs[j].codepoint == _utf32buffer[i]) {
                glyph = &(_font->glyphs[j]);
                break;
            }
        }
        if (!glyph) {
            continue;
        }

        // Find next glyph for kerning
        if (i < length - 1) {
        	uint32_t nextCode = _utf32buffer[i+1];
            for (k=0; k < glyph->kerning_count; ++k) {
                if (glyph->kerning[k].codepoint == nextCode) {
                    width += (glyph->kerning[k].kerning * _scale);
                    break;
                }
            }
        }

         width += glyph->advance_x * _scale;
    }
    return width;
}