//
// Created by Martin Kleinhans on 12.12.17.
//

#ifndef ANDROID_GUIDANTS_JNIREF_H
#define ANDROID_GUIDANTS_JNIREF_H

#include <jni.h>

class JNILocalFrame {
private:
    JNIEnv *_env;
public:
    JNILocalFrame(JNIEnv *env, size_t capacity = 0) {
        _env = env;
        _env->PushLocalFrame((jint)capacity);
    }

    ~JNILocalFrame() {
        _env->PopLocalFrame(nullptr);
    }
};

/**
 * base class for retained and local smart pointers
 * should not be used directly
 * Use JNILocalRef or JNIRetainedRef instead
 */
template<typename T>
class JNIRef  {
    template<typename>
    friend class JNIRef;
private:
    std::atomic<uint32_t> *_cnt;
    T *_obj;
protected:
    // cast & copy ctor
    template<typename U>
    JNIRef(const JNIRef<U>& ref, T* ptr) : _cnt(ref._cnt), _obj(ptr) {
        if(_cnt) (*_cnt)++;
    }

    // cast & move ctor
    template<typename U>
    JNIRef(JNIRef<U>&& ref, T* ptr) : _cnt(ref._cnt), _obj(ptr) {
        ref._cnt = nullptr;
    }

    // default ctor
    JNIRef(T *obj, bool retained) : _cnt(nullptr), _obj(obj) {
        if(_obj && (!_obj->isPersistent() || retained)) {
            retain();
        }
    }

    // release referenced object
    void clear() {
        release();
        _obj = nullptr;
        _cnt = nullptr;
    }

    // make ref retaining (initializes counter)
    void retain() {
        // null pointers can not be retained...
        // also make sure we are not already retaining
        if(!_obj || _cnt) return;

        _cnt = new std::atomic<uint32_t>();
        (*_cnt) = 1;

        if(_obj->isPersistent()) {
            _obj->retainJObject();
        }
    }

    // stop retaining
    void release() {
        // if values where moved, or nothing was retained in the first place
        // => _cnt will be null
        if(!_cnt) return;

        uint32_t refs = --(*_cnt);
        JNI_ASSERT(refs >= 0, "Invalid negative ref count");
        if(refs > 0) return;

        if(_obj) {
            if (_obj->isPersistent()) {
                _obj->releaseJObject();
            } else {
                delete _obj;
            }
        }

        delete _cnt;
    }

    // move ctor
    JNIRef(JNIRef<T>&& ref) : _cnt(ref._cnt), _obj(ref._obj)  {
        ref._cnt = nullptr;
    }
    // copy ctor
    JNIRef(const JNIRef<T> &ref) : _cnt(ref._cnt), _obj(ref._obj) {
        if(_cnt) (*_cnt)++;
    }

    // move assignment
    JNIRef<T>& operator=(JNIRef<T>&& other) {
        if (other._obj != _obj || other._cnt != _cnt) {
            release();
            _cnt = other._cnt;
            _obj = other._obj;
            other._cnt = nullptr;
        }
        return *this;
    }

    // copy assignment
    JNIRef<T>& operator=(JNIRef<T>& other) {
        if (other._obj != _obj || other._cnt != _cnt) {
            release();
            _cnt = other._cnt;
            if (_cnt) (*_cnt)++;
            _obj = other._obj;
        }
        return *this;
    }

public:
    ~JNIRef() {
        release();
    }

    operator bool() const {
        return _obj != nullptr;
    }
    T& operator*() const {
        return &_obj;
    }
    T* operator->() const {
        return _obj;
    }
    T* get() const {
        return _obj;
    }
};

template <typename T> class JNILocalRef;
template <typename T> class JNIRetainedRef;

/**
 * non-retained local reference to a JNIObject
 * these references do not explicitly retain the referenced object
 * they are only guaranteed to be valid as long as the pointer they were initialized with
 * is not released!
 *
 * Never use these references outside of a local scope (e.g. as member variables or as globals)
 */
template <typename T>
class JNILocalRef : public JNIRef<T> {
    template<typename>
    friend class JNILocalRef;
private:
    template<typename U>
    JNILocalRef(const JNILocalRef<U> &ref, T* ptr) : JNIRef<T>(ref, ptr) {}
    template<typename U>
    JNILocalRef(const JNILocalRef<U> &&ref, T* ptr) : JNIRef<T>(std::move(ref), ptr) {}

    JNILocalRef(const JNIRetainedRef<T> &ref) : JNIRef<T>(ref) {}
    JNILocalRef(const JNIRetainedRef<T> &&ref) : JNIRef<T>(ref) {}
public:
    JNILocalRef(JNILocalRef<T>&& ref) : JNIRef<T>(std::move(ref)) {}
    JNILocalRef(const JNILocalRef<T> &ref) : JNIRef<T>(ref) {}
    JNILocalRef(T *obj = nullptr) : JNIRef<T>(obj, false) {}

    JNILocalRef<T>& operator=(JNILocalRef<T>&& other) {
        JNIRef<T>::operator=(std::move(other));
        return *this;
    }

    JNILocalRef<T>& operator=(JNILocalRef<T>& other) {
        JNIRef<T>::operator=(other);
        return *this;
    }

    // create a local ref from a global ref
    static JNILocalRef<T> New(JNIRetainedRef<T>& ref) {
        return JNILocalRef<T>(ref);
    }
    static JNILocalRef<T> New(JNIRetainedRef<T>&& ref) {
        return JNILocalRef<T>(ref);
    }

    // cast between different types
    template<typename U>
    static JNILocalRef<T> Cast(JNILocalRef<U>& ref) {
        return JNILocalRef<T>(ref, dynamic_cast<T*>(ref.get()));
    }

    template<typename U>
    static JNILocalRef<T> Cast(JNILocalRef<U>&& ref) {
        return JNILocalRef<T>(std::move(ref), dynamic_cast<T*>(ref.get()));
    }
};

/**
 * retained reference to a JNIObject
 * these references type keeps the referenced object from being garbage collected
 * they can safely be used as class members or globals
 */
template <typename T>
class JNIRetainedRef : public JNIRef<T> {
    template<typename>
    friend class JNIRetainedRef;
private:
    template<typename U>
    JNIRetainedRef(const JNIRetainedRef<U> &ref, T* ptr) : JNIRef<T>(ref, ptr) {}
    template<typename U>
    JNIRetainedRef(const JNIRetainedRef<U> &&ref, T* ptr) : JNIRef<T>(std::move(ref), ptr) {}

    JNIRetainedRef(const JNILocalRef<T> &ref) : JNIRef<T>(ref) {
        JNIRef<T>::retain();
    }
    JNIRetainedRef(const JNILocalRef<T> &&ref) : JNIRef<T>(ref) {
        JNIRef<T>::retain();
    }
public:
    JNIRetainedRef(JNIRetainedRef<T>&& ref) : JNIRef<T>(std::move(ref)) {}
    JNIRetainedRef(const JNIRetainedRef<T> &ref) : JNIRef<T>(ref) {}
    JNIRetainedRef(T *obj = nullptr) : JNIRef<T>(obj, true) {}

    JNIRetainedRef<T>& operator=(JNIRetainedRef<T>&& other) {
        JNIRef<T>::operator=(std::move(other));
        return *this;
    }

    JNIRetainedRef<T>& operator=(JNIRetainedRef<T>& other) {
        JNIRef<T>::operator=(other);
        return *this;
    }

    // clears this reference
    // if there are no other references to the underlying object
    // it is free to be garbage collected afterwards
    void reset() {
        JNIRef<T>::clear();
    }

    // create a global ref from a local ref
    static JNIRetainedRef<T> New(JNILocalRef<T>&& ref) {
        return JNIRetainedRef<T>(ref);
    }
    static JNIRetainedRef<T> New(JNILocalRef<T>& ref) {
        return JNIRetainedRef<T>(ref);
    }

    // cast between different types
    template<typename U>
    static JNIRetainedRef<T> Cast(JNIRetainedRef<U>& ref) {
        return JNIRetainedRef<T>(ref, dynamic_cast<T*>(ref.get()));
    }

    template<typename U>
    static JNIRetainedRef<T> Cast(JNIRetainedRef<U>&& ref) {
        return JNIRetainedRef<T>(std::move(ref), dynamic_cast<T*>(ref.get()));
    }
};

#endif //ANDROID_GUIDANTS_JNIREF_H
