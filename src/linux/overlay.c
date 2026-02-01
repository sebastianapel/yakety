#include "overlay.h"
#include "logging.h"

void overlay_init(void) {
    log_debug("overlay_init: stub");
}

void overlay_show(const char *message) {
    log_info("Overlay: %s", message);
}

void overlay_show_error(const char *message) {
    log_error("Overlay error: %s", message);
}

void overlay_hide(void) {
    log_debug("overlay_hide: stub");
}

void overlay_cleanup(void) {
    log_debug("overlay_cleanup: stub");
}
