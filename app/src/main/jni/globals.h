#pragma once

#include <atomic>
#include <pthread.h>
#include <jni.h>

#include <mpv/client.h>

extern JavaVM *g_vm;

/**
 * 每个 Java 侧 MpvVideoPlayer 实例对应一个原生会话。
 *
 * 早先是进程级单例（g_mpv），重复 create 直接 abort；这让 Java 侧不得不
 * 为「同一时刻只能有一个实例」写一整套排队协议。改成实例句柄后，多个
 * 播放器可以像其它内核一样各自独立创建、销毁。
 */
struct MpvInstance {
    mpv_handle *mpv = nullptr;
    pthread_t event_thread_id = {};
    std::atomic<bool> event_thread_request_exit{false};
    /** 持有的 Android Surface 全局引用（attachSurface 时建立）。 */
    jobject surface = nullptr;
};

MpvInstance *instance_get(jlong handle);

/** 反查某个会话的 handle，供事件线程把 handle 带回 Java。 */
jlong instance_find_handle(MpvInstance *inst);
