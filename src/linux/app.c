#include "app.h"
#include "logging.h"
#include "utils.h"
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static bool g_running = false;
static bool g_is_console = false;
static AppReadyCallback g_on_ready = NULL;

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

int app_init(const char *name, const char *version, bool is_console, AppReadyCallback on_ready) {
    log_init();
    log_info("Initializing %s v%s", name, version);
    printf("Test1\n");
    
    g_is_console = is_console;
    g_on_ready = on_ready;
    g_running = true;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    return 0;
}

void app_cleanup(void) {
    log_cleanup();
}

void app_run(void) {
    if (g_on_ready) {
        g_on_ready();
    }
    
    while (g_running) {
        usleep(100000); // 100ms
    }
}

void app_quit(void) {
    g_running = false;
}

bool app_is_console(void) {
    return g_is_console;
}

bool app_is_running(void) {
    return g_running;
}

// Async execution support
typedef struct {
    async_work_fn work;
    void *arg;
    void *result;
    bool done;
} AsyncTask;

static void *async_thread_func(void *arg) {
    AsyncTask *task = (AsyncTask *)arg;
    task->result = task->work(task->arg);
    task->done = true;
    return NULL;
}

void *app_execute_async_blocking(async_work_fn work, void *arg) {
    AsyncTask task = {.work = work, .arg = arg, .result = NULL, .done = false};
    
    pthread_t thread;
    if (pthread_create(&thread, NULL, async_thread_func, &task) != 0) {
        return work(arg); // Fallback to sync
    }
    
    while (!task.done && g_running) {
        usleep(10000); // 10ms
    }
    
    pthread_join(thread, NULL);
    return task.result;
}

void **app_execute_async_blocking_all(async_work_fn *tasks, void **args, int count) {
    void **results = calloc(count, sizeof(void *));
    if (!results) return NULL;
    
    for (int i = 0; i < count; i++) {
        results[i] = app_execute_async_blocking(tasks[i], args[i]);
    }
    
    return results;
}

void app_sleep_responsive(int milliseconds) {
    usleep(milliseconds * 1000);
}
