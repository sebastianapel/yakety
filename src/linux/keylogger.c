#include "keylogger.h"
#include "logging.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_KEYBOARDS 8

typedef struct {
    int fd;
    struct libevdev *dev;
    char name[128];
} KeyboardDevice;

typedef struct {
    KeyCallback on_press;
    KeyCallback on_release;
    KeyCallback on_cancel;
    void *userdata;

    pthread_t thread;
    volatile bool running;
    volatile bool paused;

    // Multiple keyboard support
    KeyboardDevice keyboards[MAX_KEYBOARDS];
    int keyboard_count;

    KeyCombination target_combo;
    KeyloggerState state;
    bool combo_pressed;

    // Track pressed keys (across all keyboards)
    uint32_t pressed_keys[32];
    int pressed_count;
} KeyloggerContext;

static KeyloggerContext *g_keylogger = NULL;

static bool is_key_pressed(uint32_t code) {
    if (!g_keylogger) return false;
    for (int i = 0; i < g_keylogger->pressed_count; i++) {
        if (g_keylogger->pressed_keys[i] == code) return true;
    }
    return false;
}

static void add_pressed_key(uint32_t code) {
    if (!g_keylogger) return;
    if (!is_key_pressed(code) && g_keylogger->pressed_count < 32) {
        g_keylogger->pressed_keys[g_keylogger->pressed_count++] = code;
    }
}

static void remove_pressed_key(uint32_t code) {
    if (!g_keylogger) return;
    for (int i = 0; i < g_keylogger->pressed_count; i++) {
        if (g_keylogger->pressed_keys[i] == code) {
            for (int j = i; j < g_keylogger->pressed_count - 1; j++) {
                g_keylogger->pressed_keys[j] = g_keylogger->pressed_keys[j + 1];
            }
            g_keylogger->pressed_count--;
            break;
        }
    }
}

static bool check_combination_match(void) {
    if (!g_keylogger) return false;
    if (g_keylogger->pressed_count != g_keylogger->target_combo.count) return false;

    for (int i = 0; i < g_keylogger->target_combo.count; i++) {
        if (!is_key_pressed(g_keylogger->target_combo.keys[i].code)) return false;
    }
    return true;
}

static bool is_combo_key(uint32_t code) {
    if (!g_keylogger) return false;
    for (int i = 0; i < g_keylogger->target_combo.count; i++) {
        if (g_keylogger->target_combo.keys[i].code == code) return true;
    }
    return false;
}

// Check if device looks like a keyboard
static bool is_keyboard_device(struct libevdev *dev) {
    return libevdev_has_event_type(dev, EV_KEY) &&
           libevdev_has_event_code(dev, EV_KEY, KEY_A) &&
           libevdev_has_event_code(dev, EV_KEY, KEY_ENTER);
}

// Find all keyboard devices
static int find_keyboard_devices(KeyboardDevice *keyboards, int max_count) {
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        log_error("Cannot open /dev/input: %s", strerror(errno));
        return 0;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        char path[256];
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        struct libevdev *dev = NULL;
        if (libevdev_new_from_fd(fd, &dev) == 0) {
            if (is_keyboard_device(dev)) {
                keyboards[count].fd = fd;
                keyboards[count].dev = dev;
                strncpy(keyboards[count].name, libevdev_get_name(dev), sizeof(keyboards[count].name) - 1);
                log_info("Found keyboard %d: %s (%s)", count + 1, path, keyboards[count].name);
                count++;
            } else {
                libevdev_free(dev);
                close(fd);
            }
        } else {
            close(fd);
        }
    }
    closedir(dir);
    return count;
}

static void process_key_event(struct input_event *ev, const char *keyboard_name) {
    bool keyDown = (ev->value == 1);  // 1 = press
    bool keyUp = (ev->value == 0);    // 0 = release
    // ev->value == 2 is key repeat, ignore
    if (!keyDown && !keyUp) return;

    // Debug: show key events
    const char *key_name = libevdev_event_code_get_name(EV_KEY, ev->code);
    if (keyDown) {
        log_debug("[%s] KEY DOWN: %s (code=%d/0x%02X)", 
            keyboard_name, key_name ? key_name : "UNKNOWN", ev->code, ev->code);
        add_pressed_key(ev->code);
    } else {
        log_debug("[%s] KEY UP:   %s (code=%d/0x%02X)", 
            keyboard_name, key_name ? key_name : "UNKNOWN", ev->code, ev->code);
        remove_pressed_key(ev->code);
    }

    // Debug: show currently pressed keys
    if (g_keylogger->pressed_count > 0) {
        char buf[256] = "Pressed keys: ";
        for (int i = 0; i < g_keylogger->pressed_count; i++) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%d ", g_keylogger->pressed_keys[i]);
            strcat(buf, tmp);
        }
        log_debug("%s", buf);
    }

    // State machine
    switch (g_keylogger->state) {
        case KEYLOGGER_STATE_IDLE:
            if (keyDown && check_combination_match()) {
                g_keylogger->state = KEYLOGGER_STATE_COMBO_ACTIVE;
                g_keylogger->combo_pressed = true;
                log_debug("STATE: IDLE -> COMBO_ACTIVE (combo matched!)");
                if (g_keylogger->on_press) {
                    g_keylogger->on_press(g_keylogger->userdata);
                }
            }
            break;

        case KEYLOGGER_STATE_COMBO_ACTIVE:
            if (keyDown && !is_combo_key(ev->code)) {
                g_keylogger->state = KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED;
                g_keylogger->combo_pressed = false;
                log_debug("STATE: COMBO_ACTIVE -> WAITING (cancelled - extra key)");
                if (g_keylogger->on_cancel) {
                    g_keylogger->on_cancel(g_keylogger->userdata);
                }
            } else if (keyUp && !check_combination_match()) {
                g_keylogger->combo_pressed = false;
                log_debug("STATE: COMBO_ACTIVE -> releasing");
                if (g_keylogger->on_release) {
                    g_keylogger->on_release(g_keylogger->userdata);
                }
                // Check immediately if all keys are already released
                if (g_keylogger->pressed_count == 0) {
                    g_keylogger->state = KEYLOGGER_STATE_IDLE;
                    log_debug("STATE: COMBO_ACTIVE -> IDLE (all keys already released)");
                } else {
                    g_keylogger->state = KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED;
                    log_debug("STATE: COMBO_ACTIVE -> WAITING (waiting for remaining keys)");
                }
            }
            break;

        case KEYLOGGER_STATE_WAITING_FOR_ALL_RELEASED:
            if (g_keylogger->pressed_count == 0) {
                g_keylogger->state = KEYLOGGER_STATE_IDLE;
                log_debug("STATE: WAITING -> IDLE (all keys released)");
            }
            break;
    }
}

static void *keylogger_thread(void *arg) {
    (void)arg;
    
    struct pollfd fds[MAX_KEYBOARDS];
    
    // Setup poll file descriptors
    for (int i = 0; i < g_keylogger->keyboard_count; i++) {
        fds[i].fd = g_keylogger->keyboards[i].fd;
        fds[i].events = POLLIN;
    }

    while (g_keylogger && g_keylogger->running) {
        if (g_keylogger->paused) {
            usleep(50000);
            continue;
        }

        // Poll all keyboards with 50ms timeout
        int ret = poll(fds, g_keylogger->keyboard_count, 50);
        if (ret < 0) {
            if (errno != EINTR) {
                log_error("poll error: %s", strerror(errno));
            }
            continue;
        }
        if (ret == 0) continue; // timeout

        // Check each keyboard for events
        for (int i = 0; i < g_keylogger->keyboard_count; i++) {
            if (!(fds[i].revents & POLLIN)) continue;

            struct input_event ev;
            int rc;
            while ((rc = libevdev_next_event(g_keylogger->keyboards[i].dev, 
                    LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS) {
                if (ev.type == EV_KEY) {
                    process_key_event(&ev, g_keylogger->keyboards[i].name);
                }
            }
        }
    }
    return NULL;
}

int keylogger_init(KeyCallback on_press, KeyCallback on_release, KeyCallback on_key_cancel, void *userdata) {
    if (g_keylogger) return -1;

    g_keylogger = calloc(1, sizeof(KeyloggerContext));
    if (!g_keylogger) return -1;

    g_keylogger->on_press = on_press;
    g_keylogger->on_release = on_release;
    g_keylogger->on_cancel = on_key_cancel;
    g_keylogger->userdata = userdata;
    g_keylogger->state = KEYLOGGER_STATE_IDLE;

    // Find all keyboards
    g_keylogger->keyboard_count = find_keyboard_devices(g_keylogger->keyboards, MAX_KEYBOARDS);
    if (g_keylogger->keyboard_count == 0) {
        log_error("No keyboard devices found.");
        log_error("Ensure user is in 'input' group: sudo usermod -aG input $USER");
        log_error("Then log out and back in for changes to take effect.");
        free(g_keylogger);
        g_keylogger = NULL;
        return -1;
    }

    log_info("Monitoring %d keyboard(s)", g_keylogger->keyboard_count);

    g_keylogger->running = true;
    if (pthread_create(&g_keylogger->thread, NULL, keylogger_thread, NULL) != 0) {
        log_error("Failed to create keylogger thread");
        for (int i = 0; i < g_keylogger->keyboard_count; i++) {
            libevdev_free(g_keylogger->keyboards[i].dev);
            close(g_keylogger->keyboards[i].fd);
        }
        free(g_keylogger);
        g_keylogger = NULL;
        return -1;
    }

    log_info("Keylogger initialized with libevdev");
    return 0;
}

void keylogger_cleanup(void) {
    if (!g_keylogger) return;

    g_keylogger->running = false;
    pthread_join(g_keylogger->thread, NULL);

    for (int i = 0; i < g_keylogger->keyboard_count; i++) {
        if (g_keylogger->keyboards[i].dev) {
            libevdev_free(g_keylogger->keyboards[i].dev);
        }
        if (g_keylogger->keyboards[i].fd >= 0) {
            close(g_keylogger->keyboards[i].fd);
        }
    }

    free(g_keylogger);
    g_keylogger = NULL;
    log_info("Keylogger cleaned up");
}

void keylogger_pause(void) {
    if (g_keylogger) {
        g_keylogger->paused = true;
        log_info("Keylogger paused");
    }
}

void keylogger_resume(void) {
    if (g_keylogger) {
        g_keylogger->paused = false;
        g_keylogger->pressed_count = 0;
        g_keylogger->state = KEYLOGGER_STATE_IDLE;
        g_keylogger->combo_pressed = false;
        log_info("Keylogger resumed");
    }
}

void keylogger_set_combination(const KeyCombination *combo) {
    if (g_keylogger && combo) {
        g_keylogger->target_combo = *combo;
        g_keylogger->combo_pressed = false;
        g_keylogger->pressed_count = 0;
        g_keylogger->state = KEYLOGGER_STATE_IDLE;

        log_info("Keylogger combo set with %d keys:", combo->count);
        for (int i = 0; i < combo->count; i++) {
            log_info("  Key %d: code=0x%02X (%d)", i + 1, combo->keys[i].code, combo->keys[i].code);
        }
    }
}

KeyCombination keylogger_get_fn_combination(void) {
    // Linux doesn't have FN key like macOS. Use Right Ctrl as default.
    // KEY_RIGHTCTRL = 97 in linux/input-event-codes.h
    KeyCombination combo = {0};
    combo.count = 1;
    combo.keys[0].code = KEY_RIGHTCTRL;  // 97
    combo.keys[0].flags = 0;
    return combo;
}
