/*
 * Copyright (c) 2024 The ZMK Contributors
 *
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
    const uint32_t *keycodes;
};

struct behavior_key_press_lip_data {
    uint8_t keycode_states_count;
    bool *keycode_states;
};

static int kp_lip_behavior_get_index(const struct device *dev, uint32_t binding_p1) {
    const struct behavior_key_press_lip_config *config = dev->config;

    for (int i = 0; i < config->keycodes_count; i++) {
        if (config->keycodes[i] == binding_p1) {
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

    /* Release any other held key in this LIP group */
    for (int i = 0; i < config->keycodes_count; i++) {
        if (i == idx) {
            continue;
        }

        if (data->keycode_states[i]) {
            data->keycode_states[i] = false;
            raise_zmk_keycode_state_changed_from_encoded(
                config->keycodes[i], false, event.timestamp);
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

    int idx = kp_lip_behavior_get_index(dev, binding->param1);
    if (idx < 0) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (!data->keycode_states[idx]) {
        /* suppressed release */
        return ZMK_BEHAVIOR_OPAQUE;
    }

    data->keycode_states[idx] = false;

    return raise_zmk_keycode_state_changed_from_encoded(
        binding->param1, false, event.timestamp);
}

static const struct behavior_driver_api behavior_key_press_lip_driver_api = {
    .binding_pressed = on_key_press_lip_binding_pressed,
    .binding_released = on_key_press_lip_binding_released,
};

static int behavior_key_press_lip_init(const struct device *dev) {
    return 0;
}

#define LIP_INST(n)                                                                    \
    static bool behavior_key_press_lip_states_##n[DT_INST_PROP_LEN(n, keycodes)];     \
                                                                                       \
    static struct behavior_key_press_lip_data behavior_key_press_lip_data_##n = {     \
        .keycode_states_count = DT_INST_PROP_LEN(n, keycodes),                         \
        .keycode_states = behavior_key_press_lip_states_##n,                          \
    };                                                                                 \
                                                                                       \
    static const struct behavior_key_press_lip_config                                  \
        behavior_key_press_lip_config_##n = {                                          \
            .index = n,                                                                \
            .keycodes = DT_INST_PROP(n, keycodes),                                     \
            .keycodes_count = DT_INST_PROP_LEN(n, keycodes),                           \
    };                                                                                 \
                                                                                       \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_key_press_lip_init, NULL,                      \
                            &behavior_key_press_lip_data_##n,                         \
                            &behavior_key_press_lip_config_##n,                       \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,         \
                            &behavior_key_press_lip_driver_api);

DT_INST_FOREACH_STATUS_OKAY(LIP_INST)
