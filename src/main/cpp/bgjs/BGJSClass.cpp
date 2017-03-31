#include "BGJSClass.h"

using namespace v8;

#define LOG_TAG "BGJSClass"

////////////////////////////////////////////////////////////////////////
Local<FunctionTemplate> BGJSClass::makeStaticCallableFunc(FunctionCallback func)
{
    EscapableHandleScope scope(Isolate::GetCurrent());
    Local<FunctionTemplate> funcTemplate = FunctionTemplate::New(Isolate::GetCurrent(), func, classPtrToExternal());
    return scope.Escape(funcTemplate);
}

////////////////////////////////////////////////////////////////////////
//
// Makes the "this" pointer be an external so that it can be accessed by
// the static callback functions
//
// \return Local<External> containing the "this" pointer
////////////////////////////////////////////////////////////////////////
Local<External> BGJSClass::classPtrToExternal()
{
    EscapableHandleScope scope(Isolate::GetCurrent());
    return scope.Escape(External::New(Isolate::GetCurrent(), reinterpret_cast<void *>(this)));
}
