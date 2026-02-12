#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>
#include <zephyr/settings/settings.h>
#include <drivers/input_processor_rotate_plane.h>

#define DT_DRV_COMPAT zmk_rp_shell
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_SHELL) && IS_ENABLED(CONFIG_ZMK_ROTATE_PLANE_SHELL)
#define shprint(_sh, _fmt, ...) \
do { \
  if ((_sh) != NULL) \
    shell_print((_sh), _fmt, ##__VA_ARGS__); \
} while (0)

static int cmd_status(const struct shell *sh, size_t argc, char **argv) {
    char **names = NULL;
    const int available = rp_list_devices(&names);
    if (available <= 0) {
        shprint(sh, "No devices found.");
        return 0;
    }

    shprint(sh, "Devices available:");
    for (int i = 0; i < available; i++) {
        const struct device *dev = rp_device_by_name(names[i]);
        if (dev == NULL) {
            shprint(sh, "  %s (not found)", names[i]);
            continue;
        }
        uint8_t angle = 0;
        rp_get_angle_by_name(names[i], &angle);
        shprint(sh, "  %s (angle: %d)", names[i], angle);
    }

    shprint(sh, "");
    return 0;
}

static int cmd_get(const struct shell *sh, size_t argc, char **argv) {
    if (argc < 2) {
        shprint(sh, "Usage: plane get <name>");
        return -EINVAL;
    }

    uint8_t angle = 0;
    const int ret = rp_get_angle_by_name(argv[1], &angle);
    if (ret != 0) {
        shprint(sh, "Device not found.");
        return ret;
    }

    shprint(sh, "%s: angle=%d", argv[1], angle);
    return 0;
}

static int cmd_set(const struct shell *sh, size_t argc, char **argv) {
    if (argc < 3) {
        shprint(sh, "Usage: plane set <name> <angle>");
        return -EINVAL;
    }

    uint8_t angle = (uint8_t)atoi(argv[2]);
    const int ret = rp_set_angle_by_name(argv[1], angle);
    if (ret != 0) {
        shprint(sh, "Device not found.");
        return ret;
    }

    shprint(sh, "Set %s to angle %d", argv[1], angle);
    return 0;
}


SHELL_STATIC_SUBCMD_SET_CREATE(sub_rp,
    SHELL_CMD(status, NULL, "Get status of all devices", cmd_status),
    SHELL_CMD(get, NULL, "Get angle for a device", cmd_get),
    SHELL_CMD(set, NULL, "Set angle for a device", cmd_set),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(plane, &sub_rp, "Rotate plane processor", NULL);
#endif
