#include <jni.h>
#include <stdlib.h>

#include <mpv/client.h>

#include "jni_utils.h"
#include "log.h"
#include "globals.h"

extern "C" {
    jni_func(jint, setOptionString, jlong handle, jstring option, jstring value);

    jni_func(jobject, getPropertyInt, jlong handle, jstring property);
    jni_func(void, setPropertyInt, jlong handle, jstring property, jint value);
    jni_func(jobject, getPropertyDouble, jlong handle, jstring property);
    jni_func(void, setPropertyDouble, jlong handle, jstring property, jdouble value);
    jni_func(jobject, getPropertyBoolean, jlong handle, jstring property);
    jni_func(void, setPropertyBoolean, jlong handle, jstring property, jboolean value);
    jni_func(jstring, getPropertyString, jlong handle, jstring jproperty);
    jni_func(void, setPropertyString, jlong handle, jstring jproperty, jstring jvalue);

    jni_func(void, observeProperty, jlong handle, jstring property, jint format);
}

/** 取实例，不存在时返回 NULL（调用方直接返回失败，不 abort 进程）。 */
static MpvInstance *checked_instance(jlong handle)
{
    MpvInstance *inst = instance_get(handle);
    if (!inst || !inst->mpv) {
        ALOGE("libmpv instance %lld is not initialized", (long long) handle);
        return NULL;
    }
    return inst;
}

jni_func(jint, setOptionString, jlong handle, jstring joption, jstring jvalue) {
    MpvInstance *inst = checked_instance(handle);
    if (!inst)
        return MPV_ERROR_INVALID_PARAMETER;

    const char *option = env->GetStringUTFChars(joption, NULL);
    const char *value = env->GetStringUTFChars(jvalue, NULL);

    int result = mpv_set_option_string(inst->mpv, option, value);

    env->ReleaseStringUTFChars(joption, option);
    env->ReleaseStringUTFChars(jvalue, value);

    return result;
}

static int common_get_property(JNIEnv *env, jlong handle, jstring jproperty, mpv_format format, void *output)
{
    MpvInstance *inst = checked_instance(handle);
    if (!inst)
        return MPV_ERROR_INVALID_PARAMETER;

    const char *prop = env->GetStringUTFChars(jproperty, NULL);
    int result = mpv_get_property(inst->mpv, prop, format, output);
    if (result == MPV_ERROR_PROPERTY_UNAVAILABLE)
        ALOGV("mpv_get_property(%s) format %d was unavailable", prop, format);
    else if (result < 0)
        ALOGE("mpv_get_property(%s) format %d returned error %s", prop, format, mpv_error_string(result));
    env->ReleaseStringUTFChars(jproperty, prop);

    return result;
}

static int common_set_property(JNIEnv *env, jlong handle, jstring jproperty, mpv_format format, void *value)
{
    MpvInstance *inst = checked_instance(handle);
    if (!inst)
        return MPV_ERROR_INVALID_PARAMETER;

    const char *prop = env->GetStringUTFChars(jproperty, NULL);
    int result = mpv_set_property(inst->mpv, prop, format, value);
    if (result < 0)
        ALOGE("mpv_set_property(%s, %p) format %d returned error %s", prop, value, format, mpv_error_string(result));
    env->ReleaseStringUTFChars(jproperty, prop);

    return result;
}

jni_func(jobject, getPropertyInt, jlong handle, jstring jproperty) {
    int64_t value = 0;
    if (common_get_property(env, handle, jproperty, MPV_FORMAT_INT64, &value) < 0)
        return NULL;
    return env->NewObject(java_Integer, java_Integer_init, (jint)value);
}

jni_func(jobject, getPropertyDouble, jlong handle, jstring jproperty) {
    double value = 0;
    if (common_get_property(env, handle, jproperty, MPV_FORMAT_DOUBLE, &value) < 0)
        return NULL;
    return env->NewObject(java_Double, java_Double_init, (jdouble)value);
}

jni_func(jobject, getPropertyBoolean, jlong handle, jstring jproperty) {
    int value = 0;
    if (common_get_property(env, handle, jproperty, MPV_FORMAT_FLAG, &value) < 0)
        return NULL;
    return env->NewObject(java_Boolean, java_Boolean_init, (jboolean)value);
}

jni_func(jstring, getPropertyString, jlong handle, jstring jproperty) {
    char *value = NULL;
    if (common_get_property(env, handle, jproperty, MPV_FORMAT_STRING, &value) < 0)
        return NULL;
    jstring jvalue = env->NewStringUTF(value);
    mpv_free(value);
    return jvalue;
}

jni_func(void, setPropertyInt, jlong handle, jstring jproperty, jint jvalue) {
    int64_t value = static_cast<int64_t>(jvalue);
    common_set_property(env, handle, jproperty, MPV_FORMAT_INT64, &value);
}

jni_func(void, setPropertyDouble, jlong handle, jstring jproperty, jdouble jvalue) {
    double value = static_cast<double>(jvalue);
    common_set_property(env, handle, jproperty, MPV_FORMAT_DOUBLE, &value);
}

jni_func(void, setPropertyBoolean, jlong handle, jstring jproperty, jboolean jvalue) {
    int value = jvalue == JNI_TRUE ? 1 : 0;
    common_set_property(env, handle, jproperty, MPV_FORMAT_FLAG, &value);
}

jni_func(void, setPropertyString, jlong handle, jstring jproperty, jstring jvalue) {
    const char *value = env->GetStringUTFChars(jvalue, NULL);
    common_set_property(env, handle, jproperty, MPV_FORMAT_STRING, &value);
    env->ReleaseStringUTFChars(jvalue, value);
}

jni_func(void, observeProperty, jlong handle, jstring property, jint format) {
    MpvInstance *inst = checked_instance(handle);
    if (!inst)
        return;

    const char *prop = env->GetStringUTFChars(property, NULL);
    int result = mpv_observe_property(inst->mpv, 0, prop, (mpv_format)format);
    if (result < 0)
        ALOGE("mpv_observe_property(%s) format %d returned error %s", prop, format, mpv_error_string(result));
    env->ReleaseStringUTFChars(property, prop);
}
