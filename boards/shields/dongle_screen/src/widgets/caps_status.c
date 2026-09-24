/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/hid_indicators.h>

#include "caps_status.h"

#define HID_LED_CAPS_LOCK BIT(1)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct caps_status_state {
    bool on;
};

static void set_caps(struct zmk_widget_caps_status *widget, struct caps_status_state state) {
    if (state.on) {
        lv_obj_clear_flag(widget->label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(widget->label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void caps_status_update_cb(struct caps_status_state state) {
    struct zmk_widget_caps_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_caps(widget, state); }
}

static struct caps_status_state caps_status_get_state(const zmk_event_t *eh) {
    zmk_hid_indicators_t indicators;

    if (eh == NULL) {
        indicators = zmk_hid_indicators_get_current_profile();
    } else {
        const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
        indicators = ev != NULL ? ev->indicators : zmk_hid_indicators_get_current_profile();
    }

    return (struct caps_status_state){.on = (indicators & HID_LED_CAPS_LOCK) != 0};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_caps_status, struct caps_status_state, caps_status_update_cb,
                            caps_status_get_state)
ZMK_SUBSCRIPTION(widget_caps_status, zmk_hid_indicators_changed);

int zmk_widget_caps_status_init(struct zmk_widget_caps_status *widget, lv_obj_t *parent) {
    widget->label = lv_label_create(parent);
    lv_label_set_text(widget->label, "caps");
    lv_obj_set_style_text_font(widget->label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(widget->label, lv_color_hex(0xE07A4C), 0);
    lv_obj_add_flag(widget->label, LV_OBJ_FLAG_HIDDEN);

    sys_slist_append(&widgets, &widget->node);
    widget_caps_status_init();
    return 0;
}

lv_obj_t *zmk_widget_caps_status_label(struct zmk_widget_caps_status *widget) { return widget->label; }
