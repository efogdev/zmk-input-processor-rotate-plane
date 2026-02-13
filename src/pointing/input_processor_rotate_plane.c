/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_rotate_plane

#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>
#include <drivers/input_processor_rotate_plane.h>
#include <zephyr/sys/util.h> // CLAMP

#if IS_ENABLED(CONFIG_SETTINGS)
#ifndef CONFIG_SETTINGS_RUNTIME
#define CONFIG_SETTINGS_RUNTIME true
#endif
#include <zephyr/settings/settings.h>
#endif

#include <zephyr/logging/log.h>
#include <zmk/keymap.h>
#include <math.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define M_PI 3.14159265358979323846
#define MAX_LEN 2
#define RP_NVS_PREFIX "zip_rp"

struct zip_rp_config {
    int16_t angle;
    uint8_t type, timeout;
    const char *device_name;
    size_t codes_len;
    uint16_t codes[];
};

struct zip_rp_data {
    float cos_angle;
    float sin_angle;
    int32_t pending_values[MAX_LEN];
    bool has_pending[MAX_LEN];
    struct k_work_delayable timeout_work;
    const struct device *dev;
};

static const struct device *devices[DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)];
static uint8_t num_dev = 0;

static void zip_rp_apply_rotation(const struct device *dev) {
    const struct zip_rp_config *cfg = dev->config;
    struct zip_rp_data *data = dev->data;

    const int32_t x_val = data->has_pending[0] ? data->pending_values[0] : 0;
    const int32_t y_val = data->has_pending[1] ? data->pending_values[1] : 0;

    const int32_t rotated_x = (int32_t) (x_val * data->cos_angle - y_val * data->sin_angle);
    const int32_t rotated_y = (int32_t) (x_val * data->sin_angle + y_val * data->cos_angle);

    data->pending_values[0] = rotated_x;
    data->pending_values[1] = rotated_y;

    data->has_pending[0] = rotated_x != 0;
    data->has_pending[1] = rotated_y != 0;
}

static void report_values(const struct device *dev) {
    const struct zip_rp_config *cfg = dev->config;
    struct zip_rp_data *data = dev->data;

    const bool has_x = data->has_pending[0];
    const bool has_y = data->has_pending[1];

    if (has_x) {
        input_report(dev, cfg->type, cfg->codes[0], data->pending_values[0], !has_y, K_NO_WAIT);
    }

    if (has_y) {
        input_report(dev, cfg->type, cfg->codes[1], data->pending_values[1], true, K_NO_WAIT);
    }

    for (int i = 0; i < cfg->codes_len; i++) {
        data->has_pending[i] = false;
        data->pending_values[i] = 0;
    }
}

static void zip_rp_timeout_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    const struct zip_rp_data *data = CONTAINER_OF(dwork, struct zip_rp_data, timeout_work);
    const struct device *dev = data->dev;

    zip_rp_apply_rotation(dev);
    report_values(dev);
}

static int zip_rp_handle_event(const struct device *dev, struct input_event *event, 
                                uint32_t param1, uint32_t param2, 
                                struct zmk_input_processor_state *state) {
    const struct zip_rp_config *cfg = dev->config;
    struct zip_rp_data *data = dev->data;

    if (cfg->angle == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int code_index = -1;
    for (int i = 0; i < cfg->codes_len; i++) {
        if (cfg->codes[i] == event->code) {
            code_index = i;
            break;
        }
    }

    if (code_index == -1) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    data->pending_values[code_index] += event->value;
    data->has_pending[code_index] = true;

    bool all_present = true;
    for (int i = 0; i < cfg->codes_len; i++) {
        if (!data->has_pending[i]) {
            all_present = false;
            break;
        }
    }

    if (all_present) {
        k_work_cancel_delayable(&data->timeout_work);
        zip_rp_apply_rotation(dev);
        report_values(dev);

        event->value = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    event->value = 0;
    k_work_reschedule(&data->timeout_work, K_MSEC(cfg->timeout));
    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api sy_driver_api = {
    .handle_event = zip_rp_handle_event,
};

static void zip_rp_update_angle(const struct device *dev, const int16_t new_angle) {
    struct zip_rp_data *data = dev->data;
    const float angle_rad = (new_angle * M_PI) / 180.0f;
    data->cos_angle = cosf(angle_rad);
    data->sin_angle = sinf(angle_rad);
}

static int save_angle_to_nvs(const struct device *dev, const int16_t angle) {
    const struct zip_rp_config *config = dev->config;
    char setting_name[32];
    snprintf(setting_name, sizeof(setting_name), "%s/%s", RP_NVS_PREFIX, config->device_name);

    const int rc = settings_save_one(setting_name, &angle, sizeof(angle));
    if (rc != 0) {
        LOG_ERR("Failed to save angle to NVS for %s: %d", config->device_name, rc);
        return rc;
    }

    LOG_DBG("Saved angle to NVS for %s: %d", config->device_name, angle);
    return 0;
}

const struct device *rp_device_by_name(const char *name) {
    if (name == NULL) return NULL;
    for (uint8_t i = 0; i < num_dev; i++) {
        if (devices[i] == NULL) continue;

        const struct zip_rp_config *config = devices[i]->config;
        if (strcmp(config->device_name, name) == 0) {
            return devices[i];
        }
    }

    return NULL;
}

int rp_list_devices(char ***names) {
    if (num_dev == 0) {
        *names = NULL;
        return 0;
    }

    *names = NULL;
    char **collected = malloc(sizeof(char *) * num_dev);
    if (!collected) {
        LOG_ERR("Failed to allocate memory for device names");
        return -ENOMEM;
    }

    for (uint8_t i = 0; i < num_dev; i++) {
        if (devices[i] == NULL) {
            LOG_ERR("Unexpected NULL ptr");
            free(collected);
            return -EINVAL;
        };

        const struct zip_rp_config *config = devices[i]->config;
        collected[i] = strdup(config->device_name);
    }

    *names = collected;
    return num_dev;
}

int rp_get_angle_by_name(const char *name, int16_t *angle) {
    const struct device *dev = rp_device_by_name(name);
    if (dev == NULL) {
        return -EINVAL;
    }

    const struct zip_rp_config *config = dev->config;
    *angle = config->angle;
    return 0;
}

int rp_set_angle_by_name(const char *name, const int16_t angle) {
    const struct device *dev = rp_device_by_name(name);
    if (dev == NULL) {
        return -EINVAL;
    }

    struct zip_rp_config *config = (struct zip_rp_config *)dev->config;
    int16_t old_angle = config->angle;
    config->angle = angle;
    zip_rp_update_angle(dev, angle);

    char value[4];
    snprintf(value, sizeof(value), "%d", angle);
    save_angle_to_nvs(dev, angle);

    LOG_DBG("Set angle for %s: %d → %d", name, old_angle, angle);
    return 0;
}

static int zip_rp_init(const struct device *dev) {
    const struct zip_rp_config *cfg = dev->config;
    struct zip_rp_data *data = dev->data;

    const float angle_rad = (cfg->angle * M_PI) / 180.0f;
    data->cos_angle = cosf(angle_rad);
    data->sin_angle = sinf(angle_rad);

    data->dev = dev;
    k_work_init_delayable(&data->timeout_work, zip_rp_timeout_handler);

    for (int i = 0; i < MAX_LEN; i++) {
        data->pending_values[i] = 0;
        data->has_pending[i] = false;
    }

    for (uint8_t i = 0; i < num_dev; i++) {
        if (devices[i] == NULL) continue;

        const struct zip_rp_config *_config = devices[i]->config;
        if (strcmp(cfg->device_name, _config->device_name) == 0) {
            LOG_ERR("Duplicate device name: %s", cfg->device_name);
            return -EINVAL;
        }
    }

    if (num_dev < DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)) {
        devices[num_dev] = dev;
        num_dev++;
    } else {
        LOG_ERR("Too many devices");
        return -EINVAL;
    }

    return 0;
}

#define RP_INST(n)                                                                            \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, codes)                                                    \
                 <= MAX_LEN,                \
                 "Codes length > MAX_LEN"); \
    static struct zip_rp_data data_##n = {};                                                  \
    static struct zip_rp_config config_##n = {                                                \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                                        \
        .angle = DT_INST_PROP(n, angle),                                                       \
        .timeout = DT_INST_PROP(n, timeout),                                                  \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                              \
        .device_name = DT_INST_PROP_OR(n, device_name, "unknown"),                             \
        .codes = DT_INST_PROP(n, codes),                                                       \
    };                                                                                         \
    DEVICE_DT_INST_DEFINE(n, &zip_rp_init, NULL, &data_##n, &config_##n, POST_KERNEL,         \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &sy_driver_api);

DT_INST_FOREACH_STATUS_OKAY(RP_INST)

#if IS_ENABLED(CONFIG_SETTINGS)
// ReSharper disable once CppParameterMayBeConst
static int zip_rp_settings_load_cb(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const struct device *dev = rp_device_by_name(name);
    if (dev == NULL) {
        return -EINVAL;
    }

    int16_t angle = 0;
    const int err = read_cb(cb_arg, &angle, sizeof(angle));
    if (err < 0) {
        LOG_ERR("Failed to load settings (err = %d)", err);
    }

    struct zip_rp_config *config = (struct zip_rp_config *) dev->config;
    zip_rp_update_angle(dev, angle);
    config->angle = angle;

    return err;
}

SETTINGS_STATIC_HANDLER_DEFINE(zip_rp_settings, RP_NVS_PREFIX, NULL, zip_rp_settings_load_cb, NULL, NULL);
#endif

