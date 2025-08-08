idf_component_register(SRCS "smart_dog_v1.c" "network/wifi.c" "network/http_request.c" "network/websocket_client.c" "audio/inmp441_i2s.c" "audio/max98357_i2s.c" "audio/audio_echo.c" "sr/sr.c"
                    # 当前组件私有依赖项
                    PRIV_REQUIRES esp_wifi nvs_flash esp_http_client json esp_websocket_client esp_psram esp_driver_i2s esp_driver_gpio
                    # 本组件用的头文件目录，这样本组件/其他组件的源文件都可以从这个目录中找到头文件
                    INCLUDE_DIRS "network/include" "audio/include" "sr/include"
                    )

# #  将当前目录（即main/）添加为私有头文件搜索路径（这样wifi.c才能找到 "network/include/wifi.h"）
# target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
# #  将network/include目录添加为公共头文件搜索路径（等效于前面的 ‘INCLUDE_DIRS "network/include"’）
# target_include_directories(${COMPONENT_LIB} PUBLIC "network/include")