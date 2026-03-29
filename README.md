# ZMK Input Processor - Rotate Plane

Rotates pointer or scroll input by a configurable angle. Corrects axis orientation when a trackball or touchpad is mounted at an angle relative to the keyboard.

## Usage

The module ships with two pre-defined nodes in `rotate_plane.dtsi`. Include it and override the angle:

```dts
#include <input/processors/rotate_plane.dtsi>

&zip_rotate_pointer {
    angle = <45>;
};
```

Then add it to your input listener:

```dts
/ {
    trackball_listener: trackball_listener {
        compatible = "zmk,input-listener";
        device = <&trackball>;
        input-processors = <&zip_rotate_pointer>;
    };
};
```

`zip_rotate_pointer` handles `INPUT_REL_X` / `INPUT_REL_Y`. `zip_rotate_scroll` handles `INPUT_REL_WHEEL` / `INPUT_REL_HWHEEL`. Both default to `angle = <0>`.

If you need a custom instance (different codes, sync window, etc.):

```dts
/ {
    input-processors {
        zip_rp_custom: rotate_plane_custom {
            compatible = "zmk,input-processor-rotate-plane";
            device-name = "mydevice";
            #input-processor-cells = <0>;
            type = <INPUT_EV_REL>;
            codes = <INPUT_REL_X INPUT_REL_Y>;
            angle = <-30>;
            sync-window = <2>;
        };
    };
};
```

- `angle`: rotation in degrees, can be negative
- `sync-window`: ms to wait for the second axis event before treating the missing one as 0. The two axis events from a sensor typically arrive within the same report, so 2ms is usually fine.

## Configuration

```kconfig
CONFIG_ZMK_INPUT_PROCESSOR_ROTATE_PLANE=y
CONFIG_ZMK_ROTATE_PLANE_SHELL=y
```

## Shell commands

```
plane status              # list all devices and their current angle
plane get <name>          # get angle for a specific device
plane set <name> <angle>  # set angle and persist to flash
```

Angles set via shell survive reboot.

## Runtime config

If `CONFIG_ZMK_RUNTIME_CONFIG` is enabled, the timeout is tunable at runtime:

```
rtcfg set rp/timeout_ms 2
```
