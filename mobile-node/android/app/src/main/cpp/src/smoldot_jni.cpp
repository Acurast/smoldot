//
// Created by Julia Samol on 17.01.2024.
//

#include "smoldot_jni.h"

namespace JNI {
    jobject smoldot;
    EventObserver *eventObserver;

    EventObserver::EventObserver(JavaVM *jvm, int32_t jni_version) : Event::Observer() {
        jvm_ = jvm;
        jni_version_ = jni_version;
    }

    void EventObserver::OnEvent(Event::Instance* event) {
        JNIEnv *env;
        if (smoldot == nullptr) {
            return;
        }

        auto did_attach_thread = GetJniEnv(jvm_, &env, jni_version_);
        switch (event->GetType()) {
            case Event::Type::kLog: {
                auto log_event = dynamic_cast<Event::Log*>(event);
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

                __android_log_write(ANDROID_LOG_ERROR, "PANIC", panic_event->GetMessage().c_str());

                break;
            }
            case Event::Type::kChainInitialized: {
                auto chain_initialized_event = dynamic_cast<Event::ChainInitialized*>(event);
                auto chain_id = chain_initialized_event->GetChainId();
                auto error = chain_initialized_event->GetError();

                jclass clazz = env->GetObjectClass(smoldot);
                jmethodID on_chain_initialized =  env->GetMethodID(clazz, "onChainInitialized", "(JLjava/lang/String;)V");
                env->CallVoidMethod(smoldot, on_chain_initialized, static_cast<jlong>(chain_id),
                                    error.has_value() ? env->NewStringUTF(error.value().c_str()) : nullptr);

                env->DeleteLocalRef(clazz);


                break;
            }
            case Event::Type::kJsonRpcResponsesNonEmpty: {
                auto json_rpc_responses_non_empty_event = dynamic_cast<Event::JsonRpcResponsesNonEmpty*>(event);
                auto chain_id = json_rpc_responses_non_empty_event->GetChainId();

                jclass clazz = env->GetObjectClass(smoldot);
                jmethodID notify_chain = env->GetMethodID(clazz, "notifyChain", "(J)V");
                env->CallVoidMethod(smoldot, notify_chain, static_cast<jlong>(chain_id));

                env->DeleteLocalRef(clazz);

                break;
            }
        }
        if (did_attach_thread) {
            jvm_->DetachCurrentThread();
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
JNIEXPORT void JNICALL
Java_com_github_smoldot_SmoldotAndroid_jniInit(JNIEnv *env, jobject thiz, jlong log_level) {
    JavaVM* jvm;
    env->GetJavaVM(&jvm);
    JNI::smoldot = env->NewGlobalRef(thiz);
    auto jni_version = static_cast<int32_t>(env->GetVersion());
    JNI::eventObserver = new JNI::EventObserver(jvm, jni_version);

    Smoldot::State::Get()->SetEventObserver(JNI::eventObserver);

    auto max_log_level = Log::GetLevel(static_cast<uint32_t>(log_level));

    init(max_log_level);
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
    env->SetByteArrayRegion(jresponse, 0, len, reinterpret_cast<const jbyte*>(ptr));

    json_rpc_responses_pop(chain_id);

    return jresponse;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_smoldot_SmoldotAndroid_jniDestroy(JNIEnv *env, jobject thiz) {
    Smoldot::State::Reset();

    if (JNI::smoldot != nullptr) {
        env->DeleteGlobalRef(JNI::smoldot);
    }

    if (JNI::eventObserver != nullptr) {
        delete JNI::eventObserver;
    }
}