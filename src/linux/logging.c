#include "logging.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *g_log_file = NULL;

void log_init(void) {
    // Could open a log file here if needed
}

void log_cleanup(void) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

static void log_message(const char *level, const char *format, va_list args) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(stderr, "[%s] [%s] ", time_buf, level);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("INFO", format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("ERROR", format, args);
    va_end(args);
}

void log_debug(const char *format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, format);
    log_message("DEBUG", format, args);
    va_end(args);
#else
    (void) format;
#endif
}
