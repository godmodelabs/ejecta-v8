// #include "mallocdebug.h"
#include <cstring>
#include <strings.h>

#include <android/log.h>
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , "~~~~~~", __VA_ARGS__)

void* myMalloc (size_t size, const char* file, int line, const char* func) {
	void* ret = malloc(size);
	LOGD("malloc of %u = %p in %s line %d file %s", size, ret, func, line, file);
	return ret;
}

void* myRealloc (void* ptr, size_t size, const char* file, int line, const char* func) {
	void* ret = realloc(ptr, size);
	LOGD("realloc of %u, %p = %p in %s line %d file %s", size, ptr, ret, func, line, file);
	return ret;
}

void myFree(void* ptr, const char* file, int line, const char* func) {
	LOGD("free of %p in %s line %d file %s", ptr, func, line, file);
	free(ptr);
}

void* myMemset (void* ptr, int value, size_t num, const char* file, int line, const char* func) {
	LOGD("memset of %d bytes at %p with %i in %s line %d file %s", num, ptr, value, func, line, file);
	return memset (ptr, value, num);
}

char* myStrDup (const char* str, const char* file, int line, const char* func) {
	char *ret = strdup (str);
	LOGD("strdup of %p = %p in %s line %d file %s", str, ret, func, line, file);
	return ret;
}

void* myMemcpy ( void * destination, const void * source, size_t num, const char* file, int line, const char* func) {
	LOGD("memcpy by %u from %p - %p to %p - %p in %s line %d file %s", num, source, (char*)source + num, destination, (char*)destination + num, func, line, file);
	return memcpy(destination, source, num);
}

void myBcopy ( const void *src, const void *dest, size_t num, const char* file, int line, const char* func) {
	LOGD("memcpy by %u from %p - %p to %p - %p in %s line %d file %s", num, src, (char*)src + num, dest, (char*)dest + num, func, line, file);
	// bcopy (src, dest, num);
}
