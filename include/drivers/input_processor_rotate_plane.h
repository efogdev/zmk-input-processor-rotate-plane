/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ZMK_INPUT_PROCESSOR_ROTATE_PLANE_H
#define ZMK_INPUT_PROCESSOR_ROTATE_PLANE_H

#include <zephyr/device.h>

/**
 * @brief Get device pointer by device name
 *
 * @param name Device name
 * @return Device pointer or NULL if not found
 */
const struct device *rp_device_by_name(const char *name);

/**
 * @brief Get list of all device names
 *
 * @param names Pointer to store allocated array of names (caller must free)
 * @return Number of devices, or negative error code
 */
int rp_list_devices(char ***names);

/**
 * @brief Get angle for a device by name
 *
 * @param name Device name
 * @param angle Pointer to store angle value
 * @return 0 on success, negative error code
 */
int rp_get_angle_by_name(const char *name, int16_t *angle);

/**
 * @brief Set angle for a device by name
 *
 * @param name Device name
 * @param angle Angle value to set
 * @return 0 on success, negative error code
 */
int rp_set_angle_by_name(const char *name, int16_t angle);

#endif /* ZMK_INPUT_PROCESSOR_ROTATE_PLANE_H */
