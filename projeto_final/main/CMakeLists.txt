# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS
        "main.c"
        "button_control.c"
        "led_control.c"
		"tof_control.h" 
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_driver_gpio
        esp_timer
        freertos
        esp_common
        led_strip
)