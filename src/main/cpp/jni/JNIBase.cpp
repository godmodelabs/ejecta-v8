//
// Created by Martin Kleinhans on 22.09.17.
//

#include "JNIBase.h"
#include <assert.h>

#include "JNIClass.h"
#include "JNIWrapper.h"

JNIBase::JNIBase(JNIClassInfo *info) {
    _jniClassInfo = info;
}

JNIBase::~JNIBase() {
}

bool JNIBase::isPersistent() const {
    return _jniClassInfo->type == JNIObjectType::kPersistent;
}

const std::string& JNIBase::getCanonicalName() const {
    return _jniClassInfo->canonicalName;
}

const jclass JNIBase::getJClass() const {
    return _jniClassInfo->jniClassRef;
}

const std::string JNIBase::getSignature() const {
    return "L" + _jniClassInfo->canonicalName + ";";
}