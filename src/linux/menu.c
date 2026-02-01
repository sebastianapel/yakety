#include "menu.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>

static MenuSystem *g_menu = NULL;

MenuSystem *menu_create(void) {
    MenuSystem *menu = calloc(1, sizeof(MenuSystem));
    if (!menu) return NULL;
    
    menu->items = calloc(MAX_MENU_ITEMS, sizeof(MenuItem));
    if (!menu->items) {
        free(menu);
        return NULL;
    }
    
    menu->max_items = MAX_MENU_ITEMS;
    menu->item_count = 0;
    return menu;
}

void menu_destroy(MenuSystem *menu) {
    if (!menu) return;
    free(menu->items);
    free(menu);
}

int menu_add_item(MenuSystem *menu, const char *title, MenuCallback callback) {
    if (!menu || menu->item_count >= menu->max_items) return -1;
    
    int index = menu->item_count;
    menu->items[index].title = title;
    menu->items[index].callback = callback;
    menu->items[index].is_separator = false;
    menu->item_count++;
    
    return index;
}

int menu_add_separator(MenuSystem *menu) {
    if (!menu || menu->item_count >= menu->max_items) return -1;
    
    int index = menu->item_count;
    menu->items[index].title = NULL;
    menu->items[index].callback = NULL;
    menu->items[index].is_separator = true;
    menu->item_count++;
    
    return index;
}

int menu_init(void) {
    if (g_menu) return 0;
    
    g_menu = menu_create();
    if (!g_menu) return -1;
    
    return menu_setup_items(g_menu);
}

void menu_cleanup(void) {
    if (g_menu) {
        menu_destroy(g_menu);
        g_menu = NULL;
    }
}

MenuSystem *menu_get_system(void) {
    return g_menu;
}

int menu_show(void) {
    log_info("Menu system: Linux tray icon not implemented");
    return 0;
}

void menu_show_context_menu(void) {
    log_debug("menu_show_context_menu: stub");
}

void menu_hide(void) {
    log_debug("menu_hide: stub");
}

void menu_update_item(int index, const char *new_title) {
    if (!g_menu || index < 0 || index >= g_menu->item_count) return;
    g_menu->items[index].title = new_title;
}

void menu_set_enabled(bool enabled) {
    (void)enabled;
}
