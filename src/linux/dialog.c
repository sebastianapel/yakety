#include "dialog.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dialog_error(const char *title, const char *message) {
    log_error("%s: %s", title, message);
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), 
        "zenity --error --title='%s' --text='%s' 2>/dev/null || "
        "kdialog --error '%s' --title '%s' 2>/dev/null",
        title, message, message, title);
    int ret = system(cmd);
    (void)ret;
}

void dialog_info(const char *title, const char *message) {
    log_info("%s: %s", title, message);
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "zenity --info --title='%s' --text='%s' 2>/dev/null || "
        "kdialog --msgbox '%s' --title '%s' 2>/dev/null",
        title, message, message, title);
    int ret = system(cmd);
    (void)ret;
}

bool dialog_confirm(const char *title, const char *message) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "zenity --question --title='%s' --text='%s' 2>/dev/null || "
        "kdialog --yesno '%s' --title '%s' 2>/dev/null",
        title, message, message, title);
    return system(cmd) == 0;
}

bool dialog_keycombination_capture(const char *title, const char *message, KeyCombination *result) {
    (void)title;
    (void)message;
    log_info("Key combination capture not implemented on Linux");
    
    if (result) {
        result->count = 0;
    }
    return false;
}

bool dialog_models_and_language(const char *title, char *selected_model, size_t model_buffer_size,
                                char *selected_language, size_t language_buffer_size,
                                char *download_url, size_t url_buffer_size) {
    (void)title;
    log_info("Models dialog not implemented on Linux");
    
    if (selected_model && model_buffer_size > 0) selected_model[0] = '\0';
    if (selected_language && language_buffer_size > 0) strncpy(selected_language, "en", language_buffer_size);
    if (download_url && url_buffer_size > 0) download_url[0] = '\0';
    
    return false;
}

int dialog_model_download(const char *model_name, const char *download_url, const char *file_path) {
    (void)model_name;
    (void)download_url;
    (void)file_path;
    log_info("Model download dialog not implemented on Linux");
    return 2; // Error
}
