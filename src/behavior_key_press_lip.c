/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_key_press_lip

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/hid.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_key_press_lip_config {
    uint8_t index;
    uint8_t keycodes_count;
    uint32_t keycodes[];
};

struct behavior_key_press_lip_data {
    uint8_t keycode_states_count;
    bool keycode_states[];
    bool phys_held[];
};

static int kp_lip_behavior_get_index(const struct device *dev, uint32_t keycode) {
    const struct behavior_key_press_lip_config *config = dev->config;
    for (int i = 0; i < config->keycodes_count; i++) {
        if (config->keycodes[i] == keycode) {
            return i;
        }
    }
    return -1;
}

static int on_key_press_lip_binding_pressed(struct zmk_behavior_binding *binding,
                                            struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_key_press_lip_data *data = dev->data;
    const struct behavior_key_press_lip_config *config = dev->config;

    int idx = kp_lip_behavior_get_index(dev, binding->param1);
    if (idx < 0) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* Physically held */
    data->phys_held[idx] = true;

    /* Release any other active key */
    for (int i = 0; i < config->keycodes_count; i++) {
        if (i == idx) continue;

        if (data->keycode_states[i]) {
            data->keycode_states[i] = false;
            raise_zmk_keycode_state_changed_from_encoded(
                config->keycodes[i], false, k_uptime_get());
        }
    }

    data->keycode_states[idx] = true;
    return raise_zmk_keycode_state_changed_from_encoded(
        binding->param1, true, event.timestamp);
}

static int on_key_press_lip_binding_released(struct zmk_behavior_binding *binding,
                                             struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_key_press_lip_data *data = dev->data;
    const struct behavior_key_press_lip_config *config = dev->config;

    int idx = kp_lip_behavior_get_index(dev, binding->param1);
    if (idx < 0) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* Physical release */
    data->phys_held[idx] = false;

    /* If this key was already logically released, ignore it */
    if (!data->keycode_states[idx]) {
        /* Restore any other physically held key */
        for (int i = 0; i < config->keycodes_count; i++) {
            if (data->phys_held[i]) {
                data->keycode_states[i] = true;
                return raise_zmk_keycode_state_changed_from_encoded(
                    config->keycodes[i], true, event.timestamp);
            }
        }
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* Normal release */
    data->keycode_states[idx] = false;
    raise_zmk_keycode_state_changed_from_encoded(
        binding->param1, false, event.timestamp);

    /* Re-press any still-held key */
    for (int i = 0; i < config->keycodes_count; i++) {
        if (data->phys_held[i]) {
            data->keycode_states[i] = true;
            raise_zmk_keycode_state_changed_from_encoded(
                config->keycodes[i], true, event.timestamp);
            break;
        }
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_key_press_lip_driver_api = {
    .binding_pressed = on_key_press_lip_binding_pressed,
    .binding_released = on_key_press_lip_binding_released,
};

static int behavior_key_press_lip_init(const struct device *dev) {
    return 0;
}

#define INIT_FALSE(idx, inst) (false)

#define LIP_INST(n)                                                                    \
    static struct behavior_key_press_lip_data behavior_key_press_lip_data_##n = {     \
        .keycode_states_count = DT_INST_PROP_LEN(n, keycodes),                         \
        .keycode_states = { LISTIFY(DT_INST_PROP_LEN(n, keycodes), INIT_FALSE, (,), n) }, \
        .phys_held = { LISTIFY(DT_INST_PROP_LEN(n, keycodes), INIT_FALSE, (,), n) },   \
    };                                                                                 \
    static struct behavior_key_press_lip_config behavior_key_press_lip_config_##n = { \
        .index = n,                                                                    \
        .keycodes = DT_INST_PROP(n, keycodes),                                         \
        .keycodes_count = DT_INST_PROP_LEN(n, keycodes),                               \
    };                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_key_press_lip_init, NULL,                      \
                            &behavior_key_press_lip_data_##n,                         \
                            &behavior_key_press_lip_config_##n,                       \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,          \
                            &behavior_key_press_lip_driver_api);

DT_INST_FOREACH_STATUS_OKAY(LIP_INST)