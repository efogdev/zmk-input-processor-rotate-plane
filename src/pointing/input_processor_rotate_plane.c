/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_rotate_plane

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>
#include <zephyr/sys/util.h> // CLAMP

#if IS_ENABLED(CONFIG_SETTINGS)
#ifndef CONFIG_SETTINGS_RUNTIME
#define CONFIG_SETTINGS_RUNTIME true
#endif
#include <zephyr/settings/settings.h>
#endif

#include <zephyr/logging/log.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define MAX_LEN 4

struct zip_rp_config {
    uint8_t angle;
    uint8_t type;
    size_t codes_len;
    uint16_t codes[];
};

struct zip_rp_data {
  // stores pre-calculated rotation matrix
};

static int zip_rp_handle_event(const struct device *dev, struct input_event *event, 
                                uint32_t param1, uint32_t param2, 
                                struct zmk_input_processor_state *state) {
    const struct zip_rp_config *cfg = dev->config;
    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    for (int i = 0; i < cfg->codes_len; i++) {
        if (cfg->codes[i] == event->code) {
            // rotate
        }
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api sy_driver_api = {
    .handle_event = zip_rp_handle_event,
};

static int zip_rp_init(const struct device *dev) {
    // calculate rotation matrix
    return 0;
}

#define RP_INST(n)                                                                            \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, codes)                                                    \
                 <= MAX_LEN,                \
                 "Codes length > MAX_LEN"); \
    static struct zip_rp_data data_##n = {};                                                  \
    static struct zip_rp_config config_##n = {                                                \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                                        \
        .limit_ble_only = DT_INST_PROP(n, limit_ble_only),                                     \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                               \
        .codes = DT_INST_PROP(n, codes),                                                       \
    };                                                                                         \
    DEVICE_DT_INST_DEFINE(n, &zip_rp_init, NULL, &data_##n, &config_##n, POST_KERNEL,         \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &sy_driver_api);

// ToDo refactor to support multiple instances?
DT_INST_FOREACH_STATUS_OKAY(RP_INST)

#if IS_ENABLED(CONFIG_SETTINGS)
// ReSharper disable once CppParameterMayBeConst
static int zip_rp_settings_load_cb(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const int err = read_cb(cb_arg, &g_delay, sizeof(g_delay));
    if (err < 0) {
        LOG_ERR("Failed to load settings (err = %d)", err);
    }

    return err;
}

SETTINGS_STATIC_HANDLER_DEFINE(zip_rp_settings, zip_rp_SETTINGS_PREFIX, NULL, zip_rp_settings_load_cb, NULL, NULL);
#endif

