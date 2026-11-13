#include <jni.h>

#include <mpv/client.h>

#include "jni_utils.h"
#include "log.h"
#include "globals.h"

extern "C" {
    jni_func(void, attachSurface, jlong handle, jobject surface_);
    jni_func(void, detachSurface, jlong handle);
};

static void set_wid(MpvInstance *inst, int64_t wid)
{
    int result = mpv_set_option(inst->mpv, "wid", MPV_FORMAT_INT64, &wid);
    if (result < 0)
         ALOGE("mpv_set_option(wid) returned error %s", mpv_error_string(result));
}

jni_func(void, attachSurface, jlong handle, jobject surface_) {
    MpvInstance *inst = instance_get(handle);
    if (!inst || !inst->mpv)
        die("libmpv is not initialized");

    if (inst->surface)
        env->DeleteGlobalRef(inst->surface);

    inst->surface = env->NewGlobalRef(surface_);
    if (!inst->surface)
        die("invalid surface provided");
    set_wid(inst, reinterpret_cast<intptr_t>(inst->surface));
}

jni_func(void, detachSurface, jlong handle) {
    MpvInstance *inst = instance_get(handle);
    if (!inst || !inst->mpv)
        die("libmpv is not initialized");

    set_wid(inst, 0);

    if (inst->surface) {
        env->DeleteGlobalRef(inst->surface);
        inst->surface = NULL;
    }
}
