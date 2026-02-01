#include "http.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct DownloadHandleInternal {
    float progress;
    bool success;
    bool complete;
    char *error;
};

DownloadHandle *http_download_start(const char *url, const char *destination,
                                    DownloadProgressCallback progress_cb,
                                    DownloadCompleteCallback complete_cb,
                                    void *userdata) {
    (void)progress_cb;
    (void)complete_cb;
    (void)userdata;
    
    DownloadHandle *handle = calloc(1, sizeof(DownloadHandle));
    if (!handle) return NULL;
    
    struct DownloadHandleInternal *internal = calloc(1, sizeof(struct DownloadHandleInternal));
    if (!internal) {
        free(handle);
        return NULL;
    }
    handle->internal_data = internal;
    
    // Use curl or wget to download
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), 
        "curl -L -o '%s' '%s' 2>/dev/null || wget -O '%s' '%s' 2>/dev/null",
        destination, url, destination, url);
    
    int result = system(cmd);
    
    internal->complete = true;
    handle->is_complete = true;
    
    if (result == 0) {
        internal->success = true;
        internal->progress = 1.0f;
    } else {
        internal->success = false;
        internal->error = strdup("Download failed");
    }
    
    return handle;
}

float http_download_get_progress(DownloadHandle *handle) {
    if (!handle || !handle->internal_data) return 0;
    struct DownloadHandleInternal *internal = handle->internal_data;
    return internal->progress;
}

bool http_download_is_complete(DownloadHandle *handle) {
    return handle && handle->is_complete;
}

bool http_download_is_cancelled(DownloadHandle *handle) {
    return handle && handle->is_cancelled;
}

bool http_download_was_successful(DownloadHandle *handle) {
    if (!handle || !handle->internal_data) return false;
    struct DownloadHandleInternal *internal = handle->internal_data;
    return internal->success;
}

const char *http_download_get_error(DownloadHandle *handle) {
    if (!handle || !handle->internal_data) return NULL;
    struct DownloadHandleInternal *internal = handle->internal_data;
    return internal->error;
}

int http_download_wait(DownloadHandle *handle) {
    if (!handle) return 2;
    if (handle->is_cancelled) return 1;
    
    struct DownloadHandleInternal *internal = handle->internal_data;
    if (!internal) return 2;
    
    return internal->success ? 0 : 2;
}

void http_download_cancel(DownloadHandle *handle) {
    if (handle) {
        handle->is_cancelled = true;
    }
}

void http_download_cleanup(DownloadHandle *handle) {
    if (!handle) return;
    
    if (handle->internal_data) {
        struct DownloadHandleInternal *internal = handle->internal_data;
        free(internal->error);
        free(internal);
    }
    free(handle);
}
