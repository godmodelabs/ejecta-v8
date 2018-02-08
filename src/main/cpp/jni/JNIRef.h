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
 * base class for global and local smart pointers
 * should not be used directly
 * Use JNILocalRef or JNIGlobalRef instead
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
    JNIRef(const JNIRef<U>&& ref, T* ptr) : _cnt(ref._cnt), _obj(ptr) {
    }

    // default ctor
    JNIRef(T *obj, bool retained) : _cnt(nullptr), _obj(obj) {
        if(_obj && (!_obj->isPersistent() || retained)) {
            retain();
        }
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
public:
    // move ctor
    JNIRef(JNIRef<T>&& ref) : _cnt(ref._cnt), _obj(ref._obj)  {
        ref._cnt = nullptr;
    }
    // copy ctor
    JNIRef(const JNIRef<T> &ref) : _cnt(ref._cnt), _obj(ref._obj) {
        if(_cnt) (*_cnt)++;
    }

    ~JNIRef() {
        release();
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
template <typename T> class JNIGlobalRef;

/**
 * non-retained local reference to a JNIObject
 * this pointer is only valid until the scope were it was created is left
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
public:
    JNILocalRef(JNILocalRef<T>&& ref) : JNIRef<T>(std::move(ref)) {}
    JNILocalRef(const JNILocalRef<T> &ref) : JNIRef<T>(ref) {}
    JNILocalRef(T *obj) : JNIRef<T>(obj, false) {}

    JNILocalRef<T>& operator=(JNILocalRef<T>&& other) {
        JNIRef<T>::operator=(std::move(other));
        return *this;
    }

    JNILocalRef<T>& operator=(JNILocalRef<T>& other) {
        JNIRef<T>::operator=(other);
        return *this;
    }

    // create a global ref from a local ref
    static JNILocalRef<T> New(JNIGlobalRef<T>& ref) {
        return JNILocalRef<T>(ref.get());
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
 * retained global reference to a JNIObject
 * this pointer keeps the referenced object from being deleted
 */
template <typename T>
class JNIGlobalRef : public JNIRef<T> {
    template<typename>
    friend class JNIGlobalRef;
private:
    template<typename U>
    JNIGlobalRef(const JNIGlobalRef<U> &ref, T* ptr) : JNIRef<T>(ref, ptr) {}
    template<typename U>
    JNIGlobalRef(const JNIGlobalRef<U> &&ref, T* ptr) : JNIRef<T>(std::move(ref), ptr) {}
public:
    JNIGlobalRef(JNIGlobalRef<T>&& ref) : JNIRef<T>(std::move(ref)) {}
    JNIGlobalRef(const JNIGlobalRef<T> &ref) : JNIRef<T>(ref) {}
    JNIGlobalRef(T *obj) : JNIRef<T>(obj, true) {}

    JNIGlobalRef<T>& operator=(JNIGlobalRef<T>&& other) {
        JNIRef<T>::operator=(std::move(other));
        return *this;
    }

    JNIGlobalRef<T>& operator=(JNIGlobalRef<T>& other) {
        JNIRef<T>::operator=(other);
        return *this;
    }

    // create a local ref from a global ref
    static JNIGlobalRef<T> New(JNILocalRef<T>& ref) {
        return JNIGlobalRef<T>(ref.get());
    }

    // cast between different types
    template<typename U>
    static JNIGlobalRef<T> Cast(JNIGlobalRef<U>& ref) {
        return JNIGlobalRef<T>(ref, dynamic_cast<T*>(ref.get()));
    }

    template<typename U>
    static JNIGlobalRef<T> Cast(JNIGlobalRef<U>&& ref) {
        return JNIGlobalRef<T>(std::move(ref), dynamic_cast<T*>(ref.get()));
    }
};

#endif //ANDROID_GUIDANTS_JNIREF_H
