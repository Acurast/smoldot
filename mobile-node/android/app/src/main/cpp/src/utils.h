//
// Created by Julia Samol on 17.01.2024.
//

#ifndef SMOLDOT_ANDROID_UTILS_H
#define SMOLDOT_ANDROID_UTILS_H

#include <jni.h>
#include <optional>
#include <string>
#include <vector>

/******** Rust -> C++ ********/

std::string GetString(uint8_t *ptr, size_t len);

std::optional<std::string> GetOptString(uint8_t *ptr, size_t len);

/******** JVM -> C++ ********/

std::vector<uint8_t> GetUInt8Vector(JNIEnv *env, jbyteArray bytes);

/******** JVM ********/

bool GetJniEnv(JavaVM *vm, JNIEnv **env, int32_t version);

#endif //SMOLDOT_ANDROID_UTILS_H
