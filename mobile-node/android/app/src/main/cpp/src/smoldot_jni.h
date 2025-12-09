//
// Created by Julia Samol on 17.01.2024.
//

#ifndef SMOLDOT_ANDROID_SMOLDOT_JNI_H
#define SMOLDOT_ANDROID_SMOLDOT_JNI_H

#include "utils.h"
#include "data.h"
#include "smoldot.h"
#include "smoldot_ffi.h"

#include <android/log.h>
#include <jni.h>

#include <mutex>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

namespace JNI {
    class EventObserver : public Event::Observer {
    private:
        JavaVM* jvm_;
        int32_t jni_version_;
        int32_t id_;

    public:
        EventObserver(JavaVM* jvm, int32_t jni_version, int32_t id);
        void OnEvent(Event::Instance* event) override;
    };
}

#ifdef __cplusplus
};
#endif //__cplusplus

#endif //SMOLDOT_ANDROID_SMOLDOT_JNI_H
