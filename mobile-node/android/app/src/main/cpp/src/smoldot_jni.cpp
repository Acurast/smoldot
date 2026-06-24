//
// Created by Julia Samol on 17.01.2024.
//

#include "smoldot_jni.h"

namespace JNI {
    std::mutex mutex;
    int32_t id = 0;
    jobject smoldot = nullptr;
    std::shared_ptr<EventObserver> eventObserver;

    EventObserver::EventObserver(JavaVM *jvm, int32_t jni_version, int32_t id) : Event::Observer() {
        jvm_ = jvm;
        jni_version_ = jni_version;
        id_ = id;
    }

    void EventObserver::OnEvent(Event::Instance* event) {
        JavaVM *_jvm;
        int32_t _jni_version;
        int32_t _id;
        int32_t _jni_id;

        {
            std::lock_guard<std::mutex> lock(JNI::mutex);
            if (!smoldot) {
                return;
            }

            _jvm = jvm_;
            _jni_version = jni_version_;
            _id = id_;
            _jni_id = JNI::id;
        }

        JNIEnv *env;

        auto did_attach_thread = GetJniEnv(_jvm, &env, _jni_version);
        if (env == nullptr) {
            return;
        }
        switch (event->GetType()) {
            case Event::Type::kLog: {
                auto log_event = dynamic_cast<Event::Log*>(event);
                if (log_event == nullptr || _jni_id != _id) {
                    break;
                }

                android_LogPriority prio;
                switch (log_event->GetLevel()) {
                    case Log::kError:
                        prio = ANDROID_LOG_ERROR;
                        break;
                    case Log::kWarn:
                        prio = ANDROID_LOG_WARN;
                        break;
                    case Log::kInfo:
                        prio = ANDROID_LOG_INFO;
                        break;
                    case Log::kDebug:
                        prio = ANDROID_LOG_DEBUG;
                        break;
                    case Log::kTrace:
                        prio = ANDROID_LOG_VERBOSE;
                        break;
                    default:
                        prio = ANDROID_LOG_DEFAULT;
                        break;
                }

                __android_log_write(prio,
                                    log_event->GetTarget().c_str(),
                                    log_event->GetMessage().c_str());

                break;
            }
            case Event::Type::kPanic: {
                auto panic_event = dynamic_cast<Event::Panic*>(event);
                if (panic_event == nullptr || _jni_id != _id) {
                    break;
                }

                __android_log_write(ANDROID_LOG_ERROR, "PANIC", panic_event->GetMessage().c_str());

                break;
            }
            case Event::Type::kChainInitialized: {
                auto chain_initialized_event = dynamic_cast<Event::ChainInitialized*>(event);
                if (chain_initialized_event == nullptr) {
                    break;
                }

                std::vector<jobject> local_refs;

                auto chain_id = chain_initialized_event->GetChainId();
                auto error = chain_initialized_event->GetError();
                jstring error_str = error.has_value() ? env->NewStringUTF(error.value().c_str()) : nullptr;
                if (error_str) {
                    local_refs.push_back(error_str);
                }

                {
                    std::lock_guard<std::mutex> lock(JNI::mutex);
                    if (!smoldot || id != _id) {
                        DeleteLocalRefs(env, local_refs);
                        break;
                    }

                    jclass clazz = env->GetObjectClass(smoldot);
                    local_refs.push_back(clazz);

                    jmethodID on_chain_initialized = env->GetMethodID(clazz, "onChainInitialized","(JLjava/lang/String;)V");
                    if (HandleException(env) || on_chain_initialized == nullptr) {
                        DeleteLocalRefs(env, local_refs);
                        break;
                    }

                    env->CallVoidMethod(smoldot, on_chain_initialized, static_cast<jlong>(chain_id), error_str);
                    HandleException(env);
                }

                DeleteLocalRefs(env, local_refs);

                break;
            }
            case Event::Type::kJsonRpcResponsesNonEmpty: {
                auto json_rpc_responses_non_empty_event = dynamic_cast<Event::JsonRpcResponsesNonEmpty*>(event);
                if (json_rpc_responses_non_empty_event == nullptr) {
                    break;
                }

                std::vector<jobject> local_refs;

                auto chain_id = json_rpc_responses_non_empty_event->GetChainId();

                {
                    std::lock_guard<std::mutex> lock(JNI::mutex);
                    if (!smoldot || id != _id) {
                        DeleteLocalRefs(env, local_refs);
                        break;
                    }

                    jclass clazz = env->GetObjectClass(smoldot);
                    local_refs.push_back(clazz);

                    jmethodID notify_chain = env->GetMethodID(clazz, "notifyChain", "(J)V");
                    env->CallVoidMethod(smoldot, notify_chain, static_cast<jlong>(chain_id));
                    HandleException(env);
                }

                DeleteLocalRefs(env, local_refs);

                break;
            }
        }
        if (did_attach_thread) {
            _jvm->DetachCurrentThread();
        }
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved) {
    JNIEnv *env;
    __android_log_write(ANDROID_LOG_INFO, "SmoldotAndroid", "Loaded");

    if (jvm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK)
        return -1;

    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_github_smoldot_SmoldotAndroid_jniInit(JNIEnv *env, jobject thiz, jint id, jlong log_level) {
    JavaVM* jvm;
    env->GetJavaVM(&jvm);
    auto jni_version = static_cast<int32_t>(env->GetVersion());

    {
        std::lock_guard<std::mutex> lock(JNI::mutex);
        if (JNI::smoldot) {
            return JNI_FALSE;
        }

        JNI::id = static_cast<int32_t>(id);
        JNI::smoldot = env->NewGlobalRef(thiz);
        JNI::eventObserver = std::make_shared<JNI::EventObserver>(jvm, jni_version, JNI::id);
    }

    Smoldot::State::Get()->SetEventObserver(JNI::eventObserver);

    auto max_log_level = Log::GetLevel(static_cast<uint32_t>(log_level));
    init(max_log_level);

    return JNI_TRUE;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_github_smoldot_SmoldotAndroid_jniAddChain(JNIEnv *env, jobject thiz, jbyteArray chain_spec,
                                                   jbyteArray database_content,
                                                   jbyteArray potential_relay_chains,
                                                   jlong json_rpc_max_pending_requests,
                                                   jlong json_rpc_max_subscriptions) {

    auto chain_spec_vec = GetUInt8Vector(env, chain_spec);
    auto database_content_vec = GetUInt8Vector(env, database_content);
    auto potential_relay_chains_vec = GetUInt8Vector(env, potential_relay_chains);

    return add_chain(chain_spec_vec.data(),
                     chain_spec_vec.size(),
                     database_content_vec.data(),
                     database_content_vec.size(),
                     static_cast<uint32_t>(json_rpc_max_pending_requests),
                     static_cast<uint32_t>(json_rpc_max_subscriptions),
                     potential_relay_chains_vec.data(),
                     potential_relay_chains_vec.size());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_smoldot_SmoldotAndroid_jniRemoveChain(JNIEnv *env, jobject thiz, jlong chain_id) {
    remove_chain(chain_id);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_github_smoldot_SmoldotAndroid_jniSendJsonRpc(JNIEnv *env, jobject thiz, jbyteArray request,
                                                      jlong chain_id) {

    auto request_vec = GetUInt8Vector(env, request);

    return json_rpc_send(request_vec.data(), request_vec.size(), chain_id);
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_github_smoldot_SmoldotAndroid_jniJsonRpcResponsesPeek(JNIEnv *env, jobject thiz,
                                                               jlong chain_id) {

    auto response_info = json_rpc_responses_peek(chain_id);
    auto ptr = response_info->ptr;
    auto len = static_cast<jsize>(response_info->len);

    if (len == 0) {
        return nullptr;
    }

    auto jresponse = env->NewByteArray(len);
    if (!jresponse || env->ExceptionCheck()) {
        return nullptr;
    }

    env->SetByteArrayRegion(jresponse, 0, len, reinterpret_cast<const jbyte*>(ptr));
    if (env->ExceptionCheck()) {
        return nullptr;
    }

    json_rpc_responses_pop(chain_id);

    return jresponse;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_smoldot_SmoldotAndroid_jniDestroy(JNIEnv *env, jobject thiz) {
    Smoldot::State::Reset();
    {
        std::lock_guard<std::mutex> lock(JNI::mutex);

        if (JNI::smoldot) {
            env->DeleteGlobalRef(JNI::smoldot);
            JNI::smoldot = nullptr;
        }
        JNI::eventObserver.reset();
        JNI::id = 0;
    }
}