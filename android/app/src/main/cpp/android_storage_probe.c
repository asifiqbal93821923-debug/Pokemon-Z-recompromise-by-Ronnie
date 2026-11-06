#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TAG "ZAStorageProbe"
#define ASSET_NAME "za_android_storage_probe.txt"
#define ASSET_EXPECTED "ZA_ANDROID_ASSET_MANAGER_PROBE_V1\n"
#define FILE_PAYLOAD "ZA_ANDROID_PRIVATE_FILES_PROBE_V1"

static bool ensure_dir(const char *base, const char *leaf, char *out, size_t cap)
{
    if (!base || base[0] != '/' || !leaf || !*leaf) return false;
    int n = snprintf(out, cap, "%s/%s", base, leaf);
    if (n <= 0 || (size_t)n >= cap) return false;
    if (mkdir(out, 0700) == 0) return true;
    if (errno != EEXIST) return false;
    struct stat st;
    return lstat(out, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool file_roundtrip(const char *dir)
{
    char path[1024];
    int n = snprintf(path, sizeof path, "%s/storage_probe.tmp", dir);
    if (n <= 0 || (size_t)n >= sizeof path) return false;
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    const size_t len = sizeof(FILE_PAYLOAD) - 1;
    bool ok = write(fd, FILE_PAYLOAD, len) == (ssize_t)len;
    if (ok) ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    char readback[sizeof(FILE_PAYLOAD)] = {0};
    if (ok) {
        fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) ok = false;
        else {
            ssize_t got = read(fd, readback, sizeof readback - 1);
            if (got != (ssize_t)len || memcmp(readback, FILE_PAYLOAD, len) != 0) ok = false;
            if (close(fd) != 0) ok = false;
        }
    }
    if (unlink(path) != 0) ok = false;
    return ok;
}

static bool asset_roundtrip(JNIEnv *env, jobject java_assets)
{
    AAssetManager *manager = AAssetManager_fromJava(env, java_assets);
    if (!manager) return false;
    AAsset *asset = AAssetManager_open(manager, ASSET_NAME, AASSET_MODE_BUFFER);
    if (!asset) return false;
    char buffer[128] = {0};
    const int expected = (int)(sizeof ASSET_EXPECTED - 1);
    int total = 0;
    while (total < expected) {
        int n = AAsset_read(asset, buffer + total, (size_t)(expected - total));
        if (n <= 0) break;
        total += n;
    }
    AAsset_close(asset);
    return total == expected && memcmp(buffer, ASSET_EXPECTED, (size_t)expected) == 0;
}

JNIEXPORT jstring JNICALL
Java_org_ronnie_zarecomp_MainActivity_nativeRunStorageProbe(
        JNIEnv *env, jclass clazz, jstring files_dir, jstring cache_dir, jobject java_assets)
{
    (void)clazz;
    if (!files_dir || !cache_dir || !java_assets)
        return (*env)->NewStringUTF(env, "ANDROID_STORAGE_BRIDGE result=FAIL missing Java inputs");
    const char *files = (*env)->GetStringUTFChars(env, files_dir, NULL);
    const char *cache = (*env)->GetStringUTFChars(env, cache_dir, NULL);
    if (!files || !cache) {
        if (files) (*env)->ReleaseStringUTFChars(env, files_dir, files);
        if (cache) (*env)->ReleaseStringUTFChars(env, cache_dir, cache);
        return (*env)->NewStringUTF(env, "ANDROID_STORAGE_BRIDGE result=FAIL JNI string access");
    }

    char data_dir[1024], save_dir[1024], cache_dir_path[1024];
    bool dirs = ensure_dir(files, "za-data", data_dir, sizeof data_dir)
             && ensure_dir(files, "za-save", save_dir, sizeof save_dir)
             && ensure_dir(cache, "za-cache", cache_dir_path, sizeof cache_dir_path);
    bool files_ok = dirs && file_roundtrip(data_dir) && file_roundtrip(save_dir)
                         && file_roundtrip(cache_dir_path);
    bool asset_ok = asset_roundtrip(env, java_assets);
    (*env)->ReleaseStringUTFChars(env, files_dir, files);
    (*env)->ReleaseStringUTFChars(env, cache_dir, cache);

    char report[384];
    snprintf(report, sizeof report,
             "ANDROID_STORAGE_BRIDGE result=%s private_dirs=%s file_roundtrip=%s asset_manager_read=%s",
             files_ok && asset_ok ? "PASS" : "FAIL",
             dirs ? "PASS" : "FAIL", files_ok ? "PASS" : "FAIL",
             asset_ok ? "PASS" : "FAIL");
    __android_log_print(files_ok && asset_ok ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, TAG, "%s", report);
    return (*env)->NewStringUTF(env, report);
}
