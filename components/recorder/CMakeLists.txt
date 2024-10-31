set(COMPONENT_SRCS "recorder.cpp" "util.cpp")
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_PRIV_REQUIRES "audio_board" "audio_hal" "audio_sal" "driver" "esp_peripherals" "audio_stream")

register_component()

include($ENV{ADF_PATH}/CMakeLists.txt)