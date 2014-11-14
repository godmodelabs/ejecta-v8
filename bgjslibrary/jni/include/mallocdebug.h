#ifndef __MALLOCDEBUG_H
#define __MALLOCDEBUG_H	1

#include <cstring>

/*
#define malloc(x) myMalloc(x, __FILE__,__LINE__,__func__)
#define free(x) myFree(x, __FILE__,__LINE__,__func__)
#define memset(x, y, z) myMemset (x, y, z, __FILE__,__LINE__,__func__)
#define strdup(x) myStrDup(x, __FILE__,__LINE__,__func__)
#define realloc(x, y) myRealloc(x, y, __FILE__,__LINE__,__func__)
#define memcpy(x, y, z) myMemcpy(x, y, z, __FILE__,__LINE__,__func__) */
// #define bcopy(x, y, z) myBcopy(x, y, z, __FILE__,__LINE__,__func__)
// #define strcpy


void myFree(void* ptr, const char* file, int line, const char* func);
void* myMalloc (size_t size, const char* file, int line, const char* func);
void* myMemset (void* ptr, int value, size_t num, const char* file, int line, const char* func);
char* myStrDup (const char* str, const char* file, int line, const char* func);
void* myRealloc (void* ptr, size_t size, const char* file, int line, const char* func);
void* myMemcpy ( void * destination, const void * source, size_t num, const char* file, int line, const char* func);
// void* myBcopy ( const void *src, const void *dest, size_t num, const char* file, int line, const char* func);

#endif
