//
// Created by Martin Kleinhans on 12.12.17.
//

#ifndef ANDROID_GUIDANTS_JNIREF_H
#define ANDROID_GUIDANTS_JNIREF_H

struct JNIRefData {
    std::atomic<uint32_t> num;
    bool retained;
};

template<typename T>
class JNIRef  {
    template<typename> friend class JNIRef;
private:
    JNIRefData *_cnt;
    T *_obj;
protected:
    template<typename U>
    JNIRef(const JNIRef<U>& ref, T* ptr) {
        _cnt = ref._cnt;
        _cnt->num++;
        _obj = ptr;
    }

    JNIRef(T *obj, bool retained) {
        _cnt = new JNIRefData();
        _cnt->num = 1;
        _cnt->retained = false;

        _obj = obj;

        retain(retained);
    }

    void retain(bool retain = true) {
        bool isPersistent;

        // null pointers can not be retained...
        // also make sure we are not already retaining
        if(!_obj || _cnt->retained) return;

        // non-persistent objects are always retained!
        isPersistent = _obj->isPersistent();
        if(!retain && isPersistent) return;

        if(isPersistent) {
            _obj->retainJObject();
        }
        _cnt->retained = true;
    }

public:
    JNIRef (JNIRef<T>&& ref) : _cnt(ref._cnt), _obj(ref._obj)
    {
        ref._cnt = nullptr;
        __android_log_print(ANDROID_LOG_WARN, "JNIRef", "called move constructor");
    }

    JNIRef(const JNIRef<T> &ref) : _cnt(ref._cnt), _obj(ref._obj) {
        _cnt->num++;
    }

    ~JNIRef() {
        // if values where moved, _cnt will be null
        if(!_cnt) return;

        uint32_t refs = --_cnt->num;
        JNI_ASSERT(refs >= 0, "Invalid negative ref count");
        if(refs > 0) return;

        if(_obj && _cnt->retained) {
            if (_obj->isPersistent()) {
                _obj->releaseJObject();
            } else {
                delete _obj;
            }
        }

        delete _cnt;
    }

    JNIRef<T>& operator=(JNIRef<T>&& other) {
        __android_log_print(ANDROID_LOG_WARN, "JNIRef", "called move assignment operator");
        _cnt = other._cnt;
        _obj = other._obj;
        other._cnt = nullptr;
        return *this;
    }

    JNIRef<T>& operator=(JNIRef<T>& other) {
        return operator=(std::move(other));
    }

    bool isRetained() const {
        return _cnt->retained;
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

template <typename T>
class JNILocalRef : public JNIRef<T> {
    template<typename> friend class JNILocalRef;
private:
    template<typename U> JNILocalRef(const JNILocalRef<U> &ref, T* ptr) : JNIRef<T>(ref, ptr) {}
public:
    JNILocalRef(JNILocalRef<T>&& ref) : JNIRef<T>(std::move(ref)) {}
    JNILocalRef(const JNIRef<T> &ref) : JNIRef<T>(ref) {}
    JNILocalRef(const JNILocalRef<T> &ref) : JNIRef<T>(ref) {}
    JNILocalRef(T *obj) : JNIRef<T>(obj, false) {}

    template<typename U> JNILocalRef<U> As() {
        return JNILocalRef<U>(*this, dynamic_cast<U*>(this->get()));
    }

    JNILocalRef<T>& operator=(JNILocalRef<T>&& other) {
        JNIRef<T>::operator=(std::move(other));
        return *this;
    }

    JNILocalRef<T>& operator=(JNILocalRef<T>& other) {
        JNIRef<T>::operator=(other);
        return *this;
    }
};

template <typename T>
class JNIGlobalRef : public JNIRef<T> {
    template<typename> friend class JNIGlobalRef;
private:
    template<typename U> JNIGlobalRef(const JNIGlobalRef<U> &ref, T* ptr) : JNIRef<T>(ref, ptr) {}
public:
    JNIGlobalRef(JNIGlobalRef<T>&& ref) : JNIRef<T>(std::move(ref)) {}
    JNIGlobalRef(const JNIGlobalRef<T> &ref) : JNIRef<T>(ref) {}
    JNIGlobalRef(const JNIRef<T> &ref) : JNIRef<T>(ref) {
        this->retain();
    }
    JNIGlobalRef(T *obj) : JNIRef<T>(obj, true) {}

    template<typename U> JNIGlobalRef<U> As() {
        return JNIGlobalRef<U>(*this, dynamic_cast<U*>(this->get()));
    }

    JNIGlobalRef<T>& operator=(JNIGlobalRef<T>&& other) {
        JNIRef<T>::operator=(std::move(other));
        return *this;
    }

    JNIGlobalRef<T>& operator=(JNIGlobalRef<T>& other) {
        JNIRef<T>::operator=(other);
        return *this;
    }
};

#endif //ANDROID_GUIDANTS_JNIREF_H
