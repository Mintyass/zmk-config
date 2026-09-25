/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include "layer_status.h"

#define ROLLER_W 196
#define ROLLER_ANIM_MS 300
#define FADE_BANDS 4
#define FADE_BAND_H 5

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_status_state {
    uint8_t index;
};

static lv_obj_t *fade_bands[FADE_BANDS * 2];

/* Top to bottom, so Base shows Raise above it and Lower below. */
static const char *const roller_order[] = {"Function", "Raise", "Base", "Lower"};
static uint8_t layer_row[ZMK_KEYMAP_LAYERS_LEN];

static const char *layer_name_at(uint8_t index) {
    const char *name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index));
    return (name != NULL && name[0] != '\0') ? name : NULL;
}

static lv_color_t color_for_layer(uint8_t index) {
    const char *name = layer_name_at(index);

    if (name == NULL) {
        return lv_color_hex(0xE8E4DA);
    }
    if (strcmp(name, "Lower") == 0) {
        return lv_color_hex(0x7ED0C4);
    }
    if (strcmp(name, "Raise") == 0) {
        return lv_color_hex(0xE8B06A);
    }
    if (strcmp(name, "Function") == 0) {
        return lv_color_hex(0xC4B2EE);
    }
    return lv_color_hex(0xE8E4DA);
}

static void layer_roller_set(lv_obj_t *roller, struct layer_status_state state) {
    /* A style change snaps the label and deletes its slide. Recolor first,
     * put the label back where it was, then animate from there. */
    lv_obj_t *label = lv_obj_get_child(roller, 0);
    lv_coord_t y = lv_obj_get_y(label);

    if (state.index >= ZMK_KEYMAP_LAYERS_LEN) {
        return;
    }

    lv_obj_set_style_text_color(roller, color_for_layer(state.index), LV_PART_SELECTED);
    lv_obj_set_y(label, y);
    lv_roller_set_selected(roller, layer_row[state.index], LV_ANIM_ON);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_layer_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { layer_roller_set(widget->obj, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    return (struct layer_status_state){
        .index = zmk_keymap_highest_layer_active(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

static void style_roller(lv_obj_t *roller) {
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 0, LV_PART_SELECTED);
    lv_obj_set_style_pad_all(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_LEFT, LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_40, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_hex(0xD8D8D8), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(roller, 0, LV_PART_SELECTED);
    /* Inherited screen line-space would inflate the selected band and clip the neighbors. */
    lv_obj_set_style_text_line_space(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(roller, 0, LV_PART_SELECTED);
    lv_obj_set_style_anim_time(roller, ROLLER_ANIM_MS, LV_PART_MAIN);
    lv_obj_clear_flag(roller, LV_OBJ_FLAG_CLICKABLE);
}

static void make_fade_band(lv_obj_t *parent, int slot, lv_opa_t opa) {
    lv_obj_t *band = lv_obj_create(parent);
    lv_obj_remove_style_all(band);
    lv_obj_set_size(band, ROLLER_W, FADE_BAND_H);
    lv_obj_set_style_bg_color(band, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(band, opa, 0);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_CLICKABLE);
    fade_bands[slot] = band;
}

static int append_option(char *options, size_t cap, size_t *used, const char *name) {
    int wrote = snprintf(options + *used, cap - *used, "%s%s", (*used > 0) ? "\n" : "", name);

    if (wrote < 0 || (size_t)wrote >= cap - *used) {
        return -1;
    }
    *used += wrote;
    return 0;
}

static void build_roller_options(char *options, size_t cap) {
    size_t used = 0;
    uint8_t row = 0;
    bool placed[ZMK_KEYMAP_LAYERS_LEN] = {0};

    options[0] = '\0';
    for (int layer = 0; layer < ZMK_KEYMAP_LAYERS_LEN; layer++) {
        layer_row[layer] = 0;
    }

    for (size_t i = 0; i < ARRAY_SIZE(roller_order); i++) {
        for (int layer = 0; layer < ZMK_KEYMAP_LAYERS_LEN; layer++) {
            const char *name = layer_name_at(layer);

            if (placed[layer] || name == NULL || strcmp(name, roller_order[i]) != 0) {
                continue;
            }
            if (append_option(options, cap, &used, name) < 0) {
                return;
            }
            placed[layer] = true;
            layer_row[layer] = row++;
            break;
        }
    }

    for (int layer = 0; layer < ZMK_KEYMAP_LAYERS_LEN; layer++) {
        char fallback[4];
        const char *name = layer_name_at(layer);

        if (placed[layer]) {
            continue;
        }
        if (name == NULL) {
            snprintf(fallback, sizeof(fallback), "%d", layer);
            name = fallback;
        }
        if (append_option(options, cap, &used, name) < 0) {
            return;
        }
        layer_row[layer] = row++;
    }
}

int zmk_widget_layer_status_init(struct zmk_widget_layer_status *widget, lv_obj_t *parent) {
    char options[256];
    static const lv_opa_t fade_opa[FADE_BANDS] = {230, 170, 90, 30};
    uint8_t active = zmk_keymap_highest_layer_active();

    build_roller_options(options, sizeof(options));

    widget->obj = lv_roller_create(parent);
    lv_obj_set_width(widget->obj, ROLLER_W);
    style_roller(widget->obj);
    /* Row height is the main font's line height. Size after the 40px style, or the
     * window stays at the default 20px font and the neighbors collapse to a few pixels. */
    lv_roller_set_visible_row_count(widget->obj, 3);
    lv_roller_set_options(widget->obj, options, LV_ROLLER_MODE_NORMAL);
    if (active < ZMK_KEYMAP_LAYERS_LEN) {
        lv_obj_set_style_text_color(widget->obj, color_for_layer(active), LV_PART_SELECTED);
        lv_roller_set_selected(widget->obj, layer_row[active], LV_ANIM_OFF);
    }

    for (int i = 0; i < FADE_BANDS; i++) {
        make_fade_band(parent, i, fade_opa[i]);
        make_fade_band(parent, FADE_BANDS + i, fade_opa[i]);
    }

    sys_slist_append(&widgets, &widget->node);
    widget_layer_status_init();
    return 0;
}

void zmk_widget_layer_status_place_fades(struct zmk_widget_layer_status *widget) {
    for (int i = 0; i < FADE_BANDS; i++) {
        lv_obj_align_to(fade_bands[i], widget->obj, LV_ALIGN_TOP_LEFT, 0, i * FADE_BAND_H);
        lv_obj_align_to(fade_bands[FADE_BANDS + i], widget->obj, LV_ALIGN_BOTTOM_LEFT, 0,
                        -(i * FADE_BAND_H));
        lv_obj_move_foreground(fade_bands[i]);
        lv_obj_move_foreground(fade_bands[FADE_BANDS + i]);
    }
}

lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget) { return widget->obj; }
