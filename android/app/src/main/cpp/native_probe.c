#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/utsname.h>
#include <unistd.h>

#define LOG_TAG "ZAAndroidProbe"

JNIEXPORT jstring JNICALL
Java_org_ronnie_zarecomp_MainActivity_nativeGetProbeInfo(JNIEnv *env, jclass clazz) {
    (void)clazz;

    struct utsname os_info = {0};
    struct timespec now = {0};
    long page_size = sysconf(_SC_PAGESIZE);
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);

    if (uname(&os_info) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "uname() failed");
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &now);

    char info[768];
    (void)snprintf(info, sizeof(info),
        "Pokémon Z-A Android Port — Native Probe\n"
        "Status: JNI native library loaded\n"
        "ABI / machine: %s\n"
        "Kernel: %s\n"
        "Online CPUs: %ld\n"
        "Page size: %ld bytes\n"
        "Monotonic clock: %lld.%09ld seconds\n"
        "Build ABI target: arm64-v8a",
        os_info.machine[0] ? os_info.machine : "unknown",
        os_info.release[0] ? os_info.release : "unknown",
        cpu_count,
        page_size,
        (long long)now.tv_sec,
        now.tv_nsec);

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Native probe loaded");
    return (*env)->NewStringUTF(env, info);
}
