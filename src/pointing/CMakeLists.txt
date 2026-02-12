# Copyright (c) 2023 The ZMK Contributors
# SPDX-License-Identifier: MIT

target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_ROTATE_PLANE app PRIVATE input_processor_rotate_plane.c)
