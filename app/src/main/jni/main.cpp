#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <locale.h>
#include <atomic>
#include <map>
#include <mutex>

#include <mpv/client.h>

#include <pthread.h>

extern "C" {
    #include <libavcodec/jni.h>
}

#include "log.h"
#include "jni_utils.h"
#include "globals.h"
#include "event.h"

#define ARRAYLEN(a) (sizeof(a)/sizeof(a[0]))

extern "C" {
    jni_func(jlong, nativeCreate, jobject appctx);
    jni_func(void, init, jlong handle);
    jni_func(void, destroy, jlong handle);

    jni_func(void, command, jlong handle, jobjectArray jarray);
};

JavaVM *g_vm;

/** 句柄表：key 是给 Java 侧的不透明 handle，value 是会话。 */
static std::mutex g_instances_lock;
static std::map<jlong, MpvInstance *> g_instances;
static jlong g_next_handle = 1;

static jobject global_appctx;

MpvInstance *instance_get(jlong handle)
{
    std::lock_guard<std::mutex> lock(g_instances_lock);
    auto it = g_instances.find(handle);
    return it == g_instances.end() ? nullptr : it->second;
}

jlong instance_find_handle(MpvInstance *inst)
{
    std::lock_guard<std::mutex> lock(g_instances_lock);
    for (auto &pair : g_instances) {
        if (pair.second == inst)
            return pair.first;
    }
    return 0;
}

static void prepare_environment(JNIEnv *env, jobject appctx) {
    setlocale(LC_NUMERIC, "C");

    g_vm = NULL;
    env->GetJavaVM(&g_vm);
    if (!g_vm)
        die("failed to get jvm");
    av_jni_set_java_vm(g_vm, NULL);

    if (global_appctx)
        env->DeleteGlobalRef(global_appctx);
    global_appctx = env->NewGlobalRef(appctx);
    if (global_appctx)
        av_jni_set_android_app_ctx(global_appctx, NULL);

    init_methods_cache(env);
}

jni_func(jlong, nativeCreate, jobject appctx) {
    prepare_environment(env, appctx);

    auto *inst = new MpvInstance();
    inst->mpv = mpv_create();
    if (!inst->mpv) {
        delete inst;
        die("context init failed");
    }

    // use terminal log level but request verbose messages
    // this way --msg-level can be used to adjust later
    mpv_request_log_messages(inst->mpv, "terminal-default");
    mpv_set_option_string(inst->mpv, "msg-level", "all=v");

    std::lock_guard<std::mutex> lock(g_instances_lock);
    jlong handle = g_next_handle++;
    g_instances[handle] = inst;
    return handle;
}

jni_func(void, init, jlong handle) {
    MpvInstance *inst = instance_get(handle);
    if (!inst || !inst->mpv)
        die("mpv is not created");

    if (mpv_initialize(inst->mpv) < 0)
        die("mpv init failed");

    inst->event_thread_request_exit = false;
    if (pthread_create(&inst->event_thread_id, NULL, event_thread, inst) != 0)
        die("thread create failed");
    pthread_setname_np(inst->event_thread_id, "event_thread");
}

jni_func(void, destroy, jlong handle) {
    MpvInstance *inst;
    {
        std::lock_guard<std::mutex> lock(g_instances_lock);
        auto it = g_instances.find(handle);
        if (it == g_instances.end()) {
            ALOGV("mpv destroy called but it's already destroyed");
            return;
        }
        inst = it->second;
        g_instances.erase(it);
    }

    if (inst->mpv) {
        // poke event thread and wait for it to exit
        inst->event_thread_request_exit = true;
        mpv_wakeup(inst->mpv);
        pthread_join(inst->event_thread_id, NULL);

        mpv_terminate_destroy(inst->mpv);
        inst->mpv = nullptr;
    }

    if (inst->surface) {
        env->DeleteGlobalRef(inst->surface);
        inst->surface = nullptr;
    }

    delete inst;
}

jni_func(void, command, jlong handle, jobjectArray jarray) {
    MpvInstance *inst = instance_get(handle);
    if (!inst || !inst->mpv)
        die("libmpv is not initialized");

    jstring strings[64] = {0};
    const char *arguments[64] = {0};
    jsize len = env->GetArrayLength(jarray);
    if (len >= ARRAYLEN(arguments)) // null-terminated
        die("too many command arguments");

    for (jsize i = 0; i < len; ++i) {
        strings[i] = (jstring)env->GetObjectArrayElement(jarray, i);
        arguments[i] = env->GetStringUTFChars(strings[i], NULL);
    }

    mpv_command(inst->mpv, arguments);

    for (jsize i = 0; i < len; ++i) {
        env->ReleaseStringUTFChars(strings[i], arguments[i]);
        env->DeleteLocalRef(strings[i]);
    }
}
