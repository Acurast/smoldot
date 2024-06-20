//
// Created by Julia Samol on 17.01.2024.
//

#include "utils.h"

/******** Rust -> C++ ********/

std::string GetString(uint8_t *ptr, size_t len) {
    auto* buffer = reinterpret_cast<char*>(ptr);
    std::string value(buffer, len);

    return value;
}

std::optional<std::string> GetOptString(uint8_t *ptr, size_t len) {
    if (ptr == nullptr) {
        return std::nullopt;
    }

    return GetString(ptr, len);
}

/******** JVM -> C++ ********/

std::vector<uint8_t> GetUInt8Vector(JNIEnv *env, jbyteArray bytes) {
    auto len = env->GetArrayLength(bytes);
    auto *elements = env->GetByteArrayElements(bytes, nullptr);
    std::vector<uint8_t> result(elements, elements + len);

    env->ReleaseByteArrayElements(bytes, elements, JNI_ABORT);

    return result;
}

/******** JVM ********/

bool GetJniEnv(JavaVM *vm, JNIEnv **env, int32_t version) {
    bool did_attach_thread = false;
    *env = nullptr;
    auto get_env_result = vm->GetEnv((void**)env, version);
    if (get_env_result == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(env, nullptr) == JNI_OK) {
            did_attach_thread = true;
        } else {
            // Failed to attach thread.
        }
    } else if (get_env_result == JNI_EVERSION) {
        // Unsupported JNI version.
    }
    return did_attach_thread;
}