idf_component_register(SRCS
                         "main.cpp"
                       INCLUDE_DIRS
                         "."
                       REQUIRES
                         constants
                         arduino-esp32
                         esp8266-oled-ssd1306
                         logger
                         dis
                         wifi
                       PRIV_REQUIRES
                         cxx
)
