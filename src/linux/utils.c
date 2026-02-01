#include "utils.h"
#include "logging.h"
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static double g_start_time = 0;

double utils_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

double utils_now(void) {
    if (g_start_time == 0) {
        g_start_time = utils_get_time();
    }
    return utils_get_time() - g_start_time;
}

void utils_sleep_ms(int milliseconds) {
    usleep(milliseconds * 1000);
}

static const char *get_executable_dir(void) {
    static char exe_dir[1024] = {0};
    if (exe_dir[0] == '\0') {
        ssize_t len = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
        if (len > 0) {
            exe_dir[len] = '\0';
            char *last_slash = strrchr(exe_dir, '/');
            if (last_slash) {
                *last_slash = '\0';
            }
        }
    }
    return exe_dir;
}

const char *utils_get_model_path(void) {
    static char path[1024] = {0};
    if (path[0] != '\0') return path;
    
    // Check current directory first
    if (access("ggml-base-q8_0.bin", F_OK) == 0) {
        snprintf(path, sizeof(path), "ggml-base-q8_0.bin");
        return path;
    }
    
    // Check executable directory/models/
    const char *exe_dir = get_executable_dir();
    if (exe_dir[0] != '\0') {
        snprintf(path, sizeof(path), "%s/models/ggml-base-q8_0.bin", exe_dir);
        if (access(path, F_OK) == 0) return path;
    }
    
    // Fall back to config directory
    const char *config_dir = utils_get_config_dir();
    snprintf(path, sizeof(path), "%s/models/ggml-base-q8_0.bin", config_dir);
    return path;
}

const char *utils_get_vad_model_path(void) {
    static char path[1024] = {0};
    if (path[0] != '\0') return path;
    
    // Check current directory first
    if (access("silero-v5.1.2-ggml.bin", F_OK) == 0) {
        snprintf(path, sizeof(path), "silero-v5.1.2-ggml.bin");
        return path;
    }
    
    // Check executable directory/models/
    const char *exe_dir = get_executable_dir();
    if (exe_dir[0] != '\0') {
        snprintf(path, sizeof(path), "%s/models/silero-v5.1.2-ggml.bin", exe_dir);
        if (access(path, F_OK) == 0) return path;
    }
    
    // Fall back to config directory
    const char *config_dir = utils_get_config_dir();
    snprintf(path, sizeof(path), "%s/models/silero-v5.1.2-ggml.bin", config_dir);
    return path;
}

bool utils_is_launch_at_login_enabled(void) {
    // Check for autostart desktop file
    char path[1024];
    const char *config_home = getenv("XDG_CONFIG_HOME");
    if (!config_home) {
        const char *home = getenv("HOME");
        if (!home) return false;
        snprintf(path, sizeof(path), "%s/.config/autostart/yakety.desktop", home);
    } else {
        snprintf(path, sizeof(path), "%s/autostart/yakety.desktop", config_home);
    }
    
    return access(path, F_OK) == 0;
}

bool utils_set_launch_at_login(bool enabled) {
    char path[1024];
    char dir[1024];
    const char *config_home = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    
    if (!home) return false;
    
    if (!config_home) {
        snprintf(dir, sizeof(dir), "%s/.config/autostart", home);
        snprintf(path, sizeof(path), "%s/yakety.desktop", dir);
    } else {
        snprintf(dir, sizeof(dir), "%s/autostart", config_home);
        snprintf(path, sizeof(path), "%s/yakety.desktop", dir);
    }
    
    if (enabled) {
        utils_ensure_dir_exists(dir);
        FILE *f = fopen(path, "w");
        if (!f) return false;
        
        fprintf(f, "[Desktop Entry]\n");
        fprintf(f, "Type=Application\n");
        fprintf(f, "Name=Yakety\n");
        fprintf(f, "Exec=yakety-app\n");
        fprintf(f, "Hidden=false\n");
        fprintf(f, "X-GNOME-Autostart-enabled=true\n");
        fclose(f);
        return true;
    } else {
        return unlink(path) == 0 || errno == ENOENT;
    }
}

const char *utils_get_config_dir(void) {
    static char path[1024] = {0};
    if (path[0] == '\0') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(path, sizeof(path), "%s/.yakety", home);
        }
    }
    return path;
}

bool utils_ensure_dir_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return mkdir(path, 0755) == 0;
}

FILE *utils_fopen_read(const char *path) {
    return fopen(path, "r");
}

FILE *utils_fopen_read_binary(const char *path) {
    return fopen(path, "rb");
}

FILE *utils_fopen_write(const char *path) {
    return fopen(path, "w");
}

FILE *utils_fopen_write_binary(const char *path) {
    return fopen(path, "wb");
}

FILE *utils_fopen_append(const char *path) {
    return fopen(path, "a");
}

char *utils_strdup(const char *str) {
    return str ? strdup(str) : NULL;
}

int utils_stricmp(const char *s1, const char *s2) {
    return strcasecmp(s1, s2);
}

// Atomic operations
bool utils_atomic_read_bool(bool *ptr) {
    return atomic_load((_Atomic bool *)ptr);
}

void utils_atomic_write_bool(bool *ptr, bool value) {
    atomic_store((_Atomic bool *)ptr, value);
}

int utils_atomic_read_int(int *ptr) {
    return atomic_load((_Atomic int *)ptr);
}

void utils_atomic_write_int(int *ptr, int value) {
    atomic_store((_Atomic int *)ptr, value);
}

// Mutex implementation
struct utils_mutex {
    pthread_mutex_t mutex;
};

utils_mutex_t *utils_mutex_create(void) {
    utils_mutex_t *m = malloc(sizeof(utils_mutex_t));
    if (m) {
        pthread_mutex_init(&m->mutex, NULL);
    }
    return m;
}

void utils_mutex_destroy(utils_mutex_t *mutex) {
    if (mutex) {
        pthread_mutex_destroy(&mutex->mutex);
        free(mutex);
    }
}

void utils_mutex_lock(utils_mutex_t *mutex) {
    if (mutex) {
        pthread_mutex_lock(&mutex->mutex);
    }
}

void utils_mutex_unlock(utils_mutex_t *mutex) {
    if (mutex) {
        pthread_mutex_unlock(&mutex->mutex);
    }
}

void *utils_thread_id(void) {
    return (void *)(uintptr_t)pthread_self();
}

// Async execution
typedef struct {
    async_work_fn work;
    void *arg;
    async_callback_fn callback;
} AsyncWorkItem;

static void *async_worker(void *data) {
    AsyncWorkItem *item = (AsyncWorkItem *)data;
    void *result = item->work(item->arg);
    if (item->callback) {
        item->callback(result);
    }
    free(item);
    return NULL;
}

void utils_execute_async(async_work_fn work, void *arg, async_callback_fn callback) {
    AsyncWorkItem *item = malloc(sizeof(AsyncWorkItem));
    if (!item) {
        void *result = work(arg);
        if (callback) callback(result);
        return;
    }
    
    item->work = work;
    item->arg = arg;
    item->callback = callback;
    
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    if (pthread_create(&thread, &attr, async_worker, item) != 0) {
        void *result = work(arg);
        if (callback) callback(result);
        free(item);
    }
    
    pthread_attr_destroy(&attr);
}

void utils_execute_main_thread(int delay_ms, delay_callback_fn callback, void *arg) {
    // Simple implementation: just sleep and call
    if (delay_ms > 0) {
        usleep(delay_ms * 1000);
    }
    if (callback) {
        callback(arg);
    }
}
