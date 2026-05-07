target_sources_ifdef(CONFIG_ZMK_ROTATE_PLANE_SHELL app PRIVATE rp_shell.c)
set_source_files_properties(rp_shell.c PROPERTIES COMPILE_FLAGS "-Os")
