idf_component_register(SRCS "time_util.cpp" "file_utils.cpp" "device_utils.cpp"
                    INCLUDE_DIRS "include")
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)