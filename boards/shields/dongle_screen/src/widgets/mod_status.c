#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/hid.h>
#include <lvgl.h>

#include "mod_status.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static void append_mod(char *text, size_t size, const char *word) {
    size_t len = strlen(text);

    if (len > 0 && len + 1 < size) {
        text[len++] = '\n';
        text[len] = '\0';
    }
    strncat(text, word, size - strlen(text) - 1);
}

static void update_mod_status(struct zmk_widget_mod_status *widget) {
    uint8_t mods = zmk_hid_get_keyboard_report()->body.modifiers;
    char text[32] = "";

    if (mods & (MOD_LCTL | MOD_RCTL)) {
        append_mod(text, sizeof(text), "ctl");
    }
    if (mods & (MOD_LALT | MOD_RALT)) {
        append_mod(text, sizeof(text), "alt");
    }
    if (mods & (MOD_LSFT | MOD_RSFT)) {
        append_mod(text, sizeof(text), "sft");
    }
    if (mods & (MOD_LGUI | MOD_RGUI)) {
        append_mod(text, sizeof(text), "sup");
    }

    lv_label_set_text(widget->label, text);
}

static void mod_status_timer_cb(struct k_timer *timer) {
    struct zmk_widget_mod_status *widget = k_timer_user_data_get(timer);
    update_mod_status(widget);
}

static struct k_timer mod_status_timer;

int zmk_widget_mod_status_init(struct zmk_widget_mod_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 56, 96);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(widget->obj, 0, 0);
    lv_obj_set_style_pad_all(widget->obj, 0, 0);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_CLICKABLE);

    widget->label = lv_label_create(widget->obj);
    lv_obj_set_width(widget->label, 56);
    lv_obj_set_style_text_font(widget->label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(widget->label, lv_color_hex(0xF4EFE4), 0);
    lv_obj_set_style_text_align(widget->label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(widget->label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_label_set_text(widget->label, "");

    k_timer_init(&mod_status_timer, mod_status_timer_cb, NULL);
    k_timer_user_data_set(&mod_status_timer, widget);
    k_timer_start(&mod_status_timer, K_MSEC(100), K_MSEC(100));

    return 0;
}

lv_obj_t *zmk_widget_mod_status_obj(struct zmk_widget_mod_status *widget) { return widget->obj; }
