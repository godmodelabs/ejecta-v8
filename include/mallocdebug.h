#ifndef __MALLOCDEBUG_H
#define __MALLOCDEBUG_H	1

#include <cstring>
#include <v8.h>

// v8::Persistent<v8::Object>* __debugPersistentAllocObject(v8::Isolate* isolate, v8::Local<v8::Object> *data, const char* file, int line, const char* func);
// v8::Persistent<v8::Function>* __debugPersistentAllocFunction(v8::Isolate* isolate, v8::Local<v8::Function> *data, const char* file, int line, const char* func);

/*
#define malloc(x) myMalloc(x, __FILE__,__LINE__,__func__)
#define free(x) myFree(x, __FILE__,__LINE__,__func__)
#define memset(x, y, z) myMemset (x, y, z, __FILE__,__LINE__,__func__)
#define strdup(x) myStrDup(x, __FILE__,__LINE__,__func__)
#define realloc(x, y) myRealloc(x, y, __FILE__,__LINE__,__func__)
#define memcpy(x, y, z) myMemcpy(x, y, z, __FILE__,__LINE__,__func__) */
// #define bcopy(x, y, z) myBcopy(x, y, z, __FILE__,__LINE__,__func__)
// #define strcpy

#undef DEBUG_MALLOC

#ifdef DEBUG_MALLOC

    #define BGJS_RESET_PERSISTENT(isolate, pers, data) LOGD("BGJS_PERS_RESET %p file %s line %i func %s", &pers, __FILE__, __LINE__, __func__); \
    pers.Reset(isolate, data); \
    LOGD("BGJS_PERS_NEW %p file %s line %i func %s", &pers, __FILE__, __LINE__, __func__); 

    #define BGJS_NEW_PERSISTENT_PTR(persistent) LOGD("BGJS_PERS_NEW_PTR %p file %s line %i func %s", persistent, __FILE__, __LINE__, __func__);
    #define BGJS_NEW_PERSISTENT(persistent) LOGD("BGJS_PERS_NEW %p file %s line %i func %s", &persistent, __FILE__, __LINE__, __func__);

    #define BGJS_CLEAR_PERSISTENT(pers) if (!pers.IsEmpty()) { \
        LOGD("BGJS_PERS_RESET %p file %s line %i func %s", &pers, __FILE__, __LINE__, __func__); \
        pers.Reset(); \
    }

    #define BGJS_CLEAR_PERSISTENT_PTR(pers) if (!pers->IsEmpty()) { \
        LOGD("BGJS_PERS_RESET %p file %s line %i func %s", pers, __FILE__, __LINE__, __func__); \
        pers->Reset(); \
    }

#else
    #define BGJS_RESET_PERSISTENT(isolate, pers, data) pers.Reset(isolate, data);

    #define BGJS_NEW_PERSISTENT_PTR(persistent) 
    #define BGJS_NEW_PERSISTENT(persistent) 

    #define BGJS_CLEAR_PERSISTENT(pers) pers.Reset();

    #define BGJS_CLEAR_PERSISTENT_PTR(pers) if (pers && !pers->IsEmpty()) { pers->Reset(); }
#endif


void myFree(void* ptr, const char* file, int line, const char* func);
void* myMalloc (size_t size, const char* file, int line, const char* func);
void* myMemset (void* ptr, int value, size_t num, const char* file, int line, const char* func);
char* myStrDup (const char* str, const char* file, int line, const char* func);
void* myRealloc (void* ptr, size_t size, const char* file, int line, const char* func);
void* myMemcpy ( void * destination, const void * source, size_t num, const char* file, int line, const char* func);
// void* myBcopy ( const void *src, const void *dest, size_t num, const char* file, int line, const char* func);

#endif
