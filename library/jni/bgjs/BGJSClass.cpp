#include "BGJSClass.h"

using namespace v8;

#define LOG_TAG "BGJSClass"

////////////////////////////////////////////////////////////////////////
Local<FunctionTemplate> BGJSClass::makeStaticCallableFunc(InvocationCallback func)
{
    HandleScope scope;
    Local<FunctionTemplate> funcTemplate = FunctionTemplate::New(func, classPtrToExternal());
    return scope.Close(funcTemplate);
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
    HandleScope scope;
    return scope.Close(External::New(reinterpret_cast<void *>(this)));
}
