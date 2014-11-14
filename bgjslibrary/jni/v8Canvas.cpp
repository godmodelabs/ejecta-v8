#include <jni.h>
#include <time.h>
#include <android/log.h>
#include <android/bitmap.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "v8Engine.h"
#include "cairo.h"

#define  LOG_TAG    "v8Canvas"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

int tick = 0;

static void draw_frame(AndroidBitmapInfo *info, void *pixels) {
	cairo_surface_t *surface = NULL;

	/* Setup our cairo surface to render directly to the bitmap */
	if (info->format == ANDROID_BITMAP_FORMAT_RGB_565) {
		surface = cairo_image_surface_create_for_data((unsigned char*) pixels,
				CAIRO_FORMAT_RGB16_565, info->width, info->height,
				info->stride);
	} else if (info->format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
		surface = cairo_image_surface_create_for_data((unsigned char*) pixels,
				CAIRO_FORMAT_RGB24, info->width, info->height, info->stride);
	} else {
		LOGE("Unsupported buffer format: %d", info->format);
		return;
	}

	cairo_t *cr = cairo_create(surface);

	/* clear the screen */
	memset(pixels, 0, info->stride * info->height);

	/* Normalize our canvas size to make our lives easier */
	cairo_scale(cr, info->width, info->height);

	/* Draw the big X */
	double position = (tick % 30) * (1.0 / 30);
	cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.7);
	cairo_move_to(cr, 0.1, position);
	cairo_line_to(cr, 0.9, 1.0 - position);
	cairo_move_to(cr, 0.9, position);
	cairo_line_to(cr, 0.1, 1.0 - position);
	cairo_set_line_width(cr, 0.1);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_stroke(cr);

	/* Draw three color squares */
	cairo_rectangle(cr, 0, 0, 0.5, 0.5);
	cairo_set_source_rgba(cr, 1, 0, 0, 0.50);
	cairo_fill(cr);

	cairo_rectangle(cr, 0, 0.5, 0.5, 0.5);
	cairo_set_source_rgba(cr, 0, 1, 0, 0.50);
	cairo_fill(cr);

	cairo_rectangle(cr, 0.5, 0, 0.5, 0.5);
	cairo_set_source_rgba(cr, 0, 0, 1, 0.50);
	cairo_fill(cr);

	/* Draw a more complicated path */
	cairo_set_line_width(cr, 0.04);
	cairo_scale(cr, 0.5, 0.5);
	cairo_translate(cr, 0.5, 1.0);
	cairo_set_source_rgba(cr, 1.0, 0.2, 0.0, 0.5);
	cairo_move_to(cr, 0.25, 0.25);
	cairo_line_to(cr, 0.5, 0.375);
	cairo_rel_line_to(cr, 0.25, -0.125);
	cairo_arc(cr, 0.5, 0.5, 0.25 * sqrt(2), -0.25 * M_PI, 0.25 * M_PI);
	cairo_rel_curve_to(cr, -0.25, -0.125, -0.25, 0.125, -0.5, 0);
	cairo_close_path(cr);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
	cairo_stroke(cr);

	/* Clean up. */
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

extern "C" {
JNIEXPORT void JNICALL Java_de_boersego_charting_CairoView_drawFrame(
		JNIEnv * env, jobject obj, jobject bitmap);
}

JNIEXPORT void JNICALL Java_de_boersego_charting_CairoView_drawFrame(
		JNIEnv * env, jobject obj, jobject bitmap) {
	AndroidBitmapInfo info;
	void* pixels;
	int ret;

	++tick;

	if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
		LOGE("AndroidBitmap_getInfo() failed: error=%d", ret);
		return;
	}

	if (info.format != ANDROID_BITMAP_FORMAT_RGB_565
			&& info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
		LOGE("Bitmap format is not RGB_565 or RGBA_8888");
		return;
	}

	if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
		LOGE("AndroidBitmap_lockPixels() failed: error=%d", ret);
	}

	/* Now fill the values with a nice little plasma */
	draw_frame(&info, pixels);

	AndroidBitmap_unlockPixels(env, bitmap);
}
