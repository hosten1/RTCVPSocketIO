#include <jni.h>
#include <string>
#include <memory>
#include <map>
#include <mutex>

#include "lib/sio_client.h"
#include "lib/engineio/engineio_client.h"
#include "json/json.h"

#define JNI_FUNC(name) Java_com_lymvpsocketio_SocketIOClient_##name

namespace {

struct JniContext {
    std::shared_ptr<engineio::EngineIOClient> engineio_client;
    std::shared_ptr<sio::SioClient> sio_client;
    
    jobject java_obj;
    JNIEnv* jni_env;
    JavaVM* jvm;
    
    jmethodID onConnectedMethod;
    jmethodID onDisconnectedMethod;
    jmethodID onErrorMethod;
    jmethodID onEventMethod;
    jmethodID onAckMethod;
    jmethodID onAckTimeoutMethod;
    
    int next_ack_id;
    std::map<int, sio::AckCallback> ack_callbacks;
    std::mutex ack_mutex;
    
    JniContext() : next_ack_id(1) {}
};

JniContext* get_context(jlong handle) {
    return reinterpret_cast<JniContext*>(handle);
}

JNIEnv* get_env(JavaVM* jvm) {
    JNIEnv* env = nullptr;
    if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return nullptr;
        }
    }
    return env;
}

std::string jstring_to_std(JNIEnv* env, jstring jstr) {
    if (!jstr) return "";
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return result;
}

jstring std_to_jstring(JNIEnv* env, const std::string& str) {
    return env->NewStringUTF(str.c_str());
}

Json::Value parse_json_array(const std::string& json_str) {
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    std::string errors;
    
    if (!reader->parse(json_str.data(), json_str.data() + json_str.size(), &root, &errors)) {
        return Json::Value(Json::arrayValue);
    }
    return root;
}

std::string json_value_to_string(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

} // namespace

extern "C" {

JNIEXPORT jlong JNICALL JNI_FUNC(nativeCreate)(JNIEnv* env, jobject obj) {
    auto* ctx = new JniContext();
    
    env->GetJavaVM(&ctx->jvm);
    ctx->java_obj = env->NewGlobalRef(obj);
    
    jclass cls = env->GetObjectClass(obj);
    ctx->onConnectedMethod = env->GetMethodID(cls, "onNativeConnected", "()V");
    ctx->onDisconnectedMethod = env->GetMethodID(cls, "onNativeDisconnected", "(Ljava/lang/String;)V");
    ctx->onErrorMethod = env->GetMethodID(cls, "onNativeError", "(Ljava/lang/String;)V");
    ctx->onEventMethod = env->GetMethodID(cls, "onNativeEvent", "(Ljava/lang/String;Ljava/lang/String;)V");
    ctx->onAckMethod = env->GetMethodID(cls, "onNativeAck", "(ILjava/lang/String;)V");
    ctx->onAckTimeoutMethod = env->GetMethodID(cls, "onNativeAckTimeout", "(I)V");
    
    return reinterpret_cast<jlong>(ctx);
}

JNIEXPORT void JNICALL JNI_FUNC(nativeSetVersion)(JNIEnv* env, jobject obj, jint version) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx) return;
    
    sio::SocketIOVersion sio_ver = sio::SocketIOVersion::V4;
    engineio::EngineIOVersion eio_ver = engineio::EngineIOVersion::V4;
    
    if (version == 2) {
        sio_ver = sio::SocketIOVersion::V2;
        eio_ver = engineio::EngineIOVersion::V2;
    } else if (version == 3) {
        sio_ver = sio::SocketIOVersion::V3;
        eio_ver = engineio::EngineIOVersion::V3;
    }
    
    sio::SioClient::Config sio_config;
    sio_config.version = sio_ver;
    ctx->sio_client = sio::SioClient::Create(sio_config);
    
    engineio::EngineIOClient::Config eio_config;
    eio_config.version = eio_ver;
    eio_config.transport = engineio::TransportType::POLLING;
    eio_config.self_signed_ssl = false;
    ctx->engineio_client = engineio::EngineIOClient::Create(eio_config);
}

JNIEXPORT void JNICALL JNI_FUNC(nativeSetTransport)(JNIEnv* env, jobject obj, jint transport) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx || !ctx->engineio_client) return;
    
    ctx->engineio_client->set_transport(
        transport == 1 ? engineio::TransportType::WEBSOCKET : engineio::TransportType::POLLING
    );
}

JNIEXPORT void JNICALL JNI_FUNC(nativeSetSelfSignedSSL)(JNIEnv* env, jobject obj, jboolean enabled) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx || !ctx->engineio_client) return;
}

JNIEXPORT void JNICALL JNI_FUNC(nativeConnect)(JNIEnv* env, jobject obj, jstring url) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx || !ctx->engineio_client || !ctx->sio_client) return;
    
    std::string url_str = jstring_to_std(env, url);
    
    auto self = ctx;
    JavaVM* jvm = ctx->jvm;
    jobject java_obj = ctx->java_obj;
    
    ctx->engineio_client->set_open_callback([self, jvm, java_obj]() {
        JNIEnv* env = get_env(jvm);
        if (!env) return;
        
        if (self->sio_client) {
            self->sio_client->set_send_callback(
                [self](const std::string& text, const std::vector<sio::SmartBuffer>& bins) {
                    (void)bins;
                    if (self->engineio_client && self->engineio_client->is_connected()) {
                        self->engineio_client->send(text);
                        return true;
                    }
                    return false;
                }
            );
            
            if (self->sio_client->get_version() >= sio::SocketIOVersion::V3) {
                self->engineio_client->send("0");
            }
        }
        
        env->CallVoidMethod(java_obj, self->onConnectedMethod);
        if (env->ExceptionCheck()) env->ExceptionClear();
    });
    
    ctx->engineio_client->set_message_callback([self, jvm, java_obj](const std::string& message) {
        JNIEnv* env = get_env(jvm);
        if (!env) return;
        
        if (self->sio_client) {
            if (!message.empty() && message[0] == '0') {
                env->CallVoidMethod(java_obj, self->onConnectedMethod);
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            self->sio_client->process_text_packet(message);
        }
    });
    
    ctx->engineio_client->set_close_callback([self, jvm, java_obj](const std::string& reason) {
        JNIEnv* env = get_env(jvm);
        if (!env) return;
        
        jstring jreason = std_to_jstring(env, reason);
        env->CallVoidMethod(java_obj, self->onDisconnectedMethod, jreason);
        env->DeleteLocalRef(jreason);
        if (env->ExceptionCheck()) env->ExceptionClear();
    });
    
    ctx->engineio_client->set_error_callback([self, jvm, java_obj](const std::string& error) {
        JNIEnv* env = get_env(jvm);
        if (!env) return;
        
        jstring jerror = std_to_jstring(env, error);
        env->CallVoidMethod(java_obj, self->onErrorMethod, jerror);
        env->DeleteLocalRef(jerror);
        if (env->ExceptionCheck()) env->ExceptionClear();
    });
    
    ctx->engineio_client->connect(url_str);
}

JNIEXPORT void JNICALL JNI_FUNC(nativeDisconnect)(JNIEnv* env, jobject obj) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx || !ctx->engineio_client) return;
    
    ctx->engineio_client->disconnect();
}

JNIEXPORT jboolean JNICALL JNI_FUNC(nativeIsConnected)(JNIEnv* env, jobject obj) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx || !ctx->engineio_client) return JNI_FALSE;
    
    return ctx->engineio_client->is_connected() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL JNI_FUNC(nativeGetSid)(JNIEnv* env, jobject obj) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx || !ctx->engineio_client) return env->NewStringUTF("");
    
    return std_to_jstring(env, ctx->engineio_client->get_sid());
}

JNIEXPORT void JNICALL JNI_FUNC(nativeEmit)(JNIEnv* env, jobject obj, jstring event, jstring argsJson) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx || !ctx->sio_client) return;
    
    std::string event_str = jstring_to_std(env, event);
    std::string args_str = jstring_to_std(env, argsJson);
    
    Json::Value args = parse_json_array(args_str);
    std::vector<Json::Value> arg_vec;
    if (args.isArray()) {
        for (Json::ArrayIndex i = 0; i < args.size(); ++i) {
            arg_vec.push_back(args[i]);
        }
    }
    
    ctx->sio_client->emit(event_str, arg_vec);
}

JNIEXPORT jint JNICALL JNI_FUNC(nativeEmitWithAck)(JNIEnv* env, jobject obj,
                                                     jstring event, jstring argsJson, jint timeoutMs) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx || !ctx->sio_client) return -1;
    
    std::string event_str = jstring_to_std(env, event);
    std::string args_str = jstring_to_std(env, argsJson);
    
    Json::Value args = parse_json_array(args_str);
    std::vector<Json::Value> arg_vec;
    if (args.isArray()) {
        for (Json::ArrayIndex i = 0; i < args.size(); ++i) {
            arg_vec.push_back(args[i]);
        }
    }
    
    int ack_id = ctx->next_ack_id++;
    auto self = ctx;
    JavaVM* jvm = ctx->jvm;
    jobject java_obj = ctx->java_obj;
    
    ctx->sio_client->emit(
        event_str,
        arg_vec,
        [self, ack_id, jvm, java_obj](const std::vector<Json::Value>& args) {
            JNIEnv* env = get_env(jvm);
            if (!env) return;
            
            Json::Value arr(Json::arrayValue);
            for (const auto& arg : args) {
                arr.append(arg);
            }
            std::string json_str = json_value_to_string(arr);
            
            jstring jjson = std_to_jstring(env, json_str);
            env->CallVoidMethod(java_obj, self->onAckMethod, ack_id, jjson);
            env->DeleteLocalRef(jjson);
            if (env->ExceptionCheck()) env->ExceptionClear();
        },
        [self, ack_id, jvm, java_obj](int) {
            JNIEnv* env = get_env(jvm);
            if (!env) return;
            
            env->CallVoidMethod(java_obj, self->onAckTimeoutMethod, ack_id);
            if (env->ExceptionCheck()) env->ExceptionClear();
        },
        std::chrono::milliseconds(timeoutMs)
    );
    
    return ack_id;
}

JNIEXPORT void JNICALL JNI_FUNC(nativeOn)(JNIEnv* env, jobject obj, jstring eventName) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx || !ctx->sio_client) return;
    
    std::string event_str = jstring_to_std(env, eventName);
    
    auto self = ctx;
    JavaVM* jvm = ctx->jvm;
    jobject java_obj = ctx->java_obj;
    
    ctx->sio_client->on(event_str,
        [self, event_str, jvm, java_obj](const std::vector<Json::Value>& args, sio::AckResponder ack) {
            (void)ack;
            JNIEnv* env = get_env(jvm);
            if (!env) return;
            
            Json::Value arr(Json::arrayValue);
            for (const auto& arg : args) {
                arr.append(arg);
            }
            std::string json_str = json_value_to_string(arr);
            
            jstring jevent = std_to_jstring(env, event_str);
            jstring jjson = std_to_jstring(env, json_str);
            env->CallVoidMethod(java_obj, self->onEventMethod, jevent, jjson);
            env->DeleteLocalRef(jevent);
            env->DeleteLocalRef(jjson);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    );
}

JNIEXPORT void JNICALL JNI_FUNC(nativeOff)(JNIEnv* env, jobject obj, jstring eventName) {
    jlong handle = env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J"));
    auto* ctx = get_context(handle);
    if (!ctx || !ctx->sio_client) return;
    
    std::string event_str = jstring_to_std(env, eventName);
    ctx->sio_client->off(event_str);
}

JNIEXPORT void JNICALL JNI_FUNC(nativeRelease)(JNIEnv* env, jobject obj, jlong handle) {
    auto* ctx = get_context(handle);
    if (!ctx) return;
    
    if (ctx->engineio_client) {
        ctx->engineio_client->disconnect();
    }
    
    if (ctx->java_obj) {
        env->DeleteGlobalRef(ctx->java_obj);
    }
    
    delete ctx;
}

} // extern "C"
