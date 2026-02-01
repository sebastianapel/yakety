#include "clipboard.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Buffer to store text for typing
static char *pending_text = NULL;

// Detect display server (cached)
static int is_wayland = -1; // -1 = unknown, 0 = X11, 1 = Wayland

static int detect_wayland(void) {
    if (is_wayland < 0) {
        const char *wayland = getenv("WAYLAND_DISPLAY");
        is_wayland = (wayland && strlen(wayland) > 0) ? 1 : 0;
        log_info("Display server: %s", is_wayland ? "Wayland" : "X11");
    }
    return is_wayland;
}

// Check if a command exists
static int command_exists(const char *cmd) {
    char check[256];
    snprintf(check, sizeof(check), "command -v %s >/dev/null 2>&1", cmd);
    return system(check) == 0;
}

void clipboard_copy(const char *text) {
    if (!text) return;
    
    // Store text for typing
    free(pending_text);
    pending_text = strdup(text);
    
    // Also copy to system clipboard as backup
    FILE *pipe = NULL;
    if (detect_wayland()) {
        pipe = popen("wl-copy 2>/dev/null", "w");
    } else {
        pipe = popen("xclip -selection clipboard 2>/dev/null", "w");
        if (!pipe) {
            pipe = popen("xsel --clipboard --input 2>/dev/null", "w");
        }
    }
    
    if (pipe) {
        fputs(text, pipe);
        pclose(pipe);
    }
}

void clipboard_paste(void) {
    if (!pending_text || strlen(pending_text) == 0) {
        log_error("No text to type");
        return;
    }
    
    // Escape text for shell (replace ' with '\'' )
    size_t len = strlen(pending_text);
    size_t escaped_len = len * 4 + 3; // worst case: every char is '
    char *escaped = malloc(escaped_len);
    if (!escaped) {
        log_error("Failed to allocate memory");
        return;
    }
    
    char *dst = escaped;
    *dst++ = '\'';
    for (const char *src = pending_text; *src; src++) {
        if (*src == '\'') {
            *dst++ = '\'';
            *dst++ = '\\';
            *dst++ = '\'';
            *dst++ = '\'';
        } else {
            *dst++ = *src;
        }
    }
    *dst++ = '\'';
    *dst = '\0';
    
    char cmd[4096];
    int ret = -1;
    
    if (detect_wayland()) {
        // Wayland: try wtype first, then ydotool
        if (command_exists("wtype")) {
            snprintf(cmd, sizeof(cmd), "wtype %s 2>/dev/null", escaped);
            ret = system(cmd);
        }
        if (ret != 0 && command_exists("ydotool")) {
            snprintf(cmd, sizeof(cmd), "ydotool type -- %s 2>/dev/null", escaped);
            ret = system(cmd);
        }
    } else {
        // X11: use xdotool
        if (command_exists("xdotool")) {
            snprintf(cmd, sizeof(cmd), "xdotool type --clearmodifiers -- %s 2>/dev/null", escaped);
            ret = system(cmd);
        }
    }
    
    // Fallback: clipboard paste
    if (ret != 0) {
        log_info("Typing tools not available, falling back to clipboard paste");
        if (detect_wayland()) {
            if (command_exists("wtype")) {
                ret = system("wtype -M ctrl -k v -m ctrl 2>/dev/null");
            }
        } else {
            if (command_exists("xdotool")) {
                ret = system("xdotool key --clearmodifiers ctrl+v 2>/dev/null");
            }
        }
    }
    
    free(escaped);
    
    if (ret == 0) {
        log_debug("Typed %zu characters", strlen(pending_text));
    } else {
        log_error("Failed to type text - install xdotool (X11) or wtype (Wayland)");
    }
    
    free(pending_text);
    pending_text = NULL;
}

void clipboard_cleanup(void) {
    free(pending_text);
    pending_text = NULL;
}
