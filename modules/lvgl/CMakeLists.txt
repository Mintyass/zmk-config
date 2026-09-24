set_source_files_properties(
        ${ZEPHYR_BASE}/modules/lvgl/lvgl.c
        TARGET_DIRECTORY modules__lvgl
        PROPERTIES HEADER_FILE_ONLY ON)
target_sources(modules__lvgl PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lvgl.c)