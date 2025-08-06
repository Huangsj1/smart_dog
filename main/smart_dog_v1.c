#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_psram.h"

#include "network/include/wifi.h"
#include "network/include/http_request.h"
#include "network/include/websocket_client.h"
#include "audio/include/audio_echo.h"

static const char *TAG = "app_main";

#define HTTP_URL "http://www.metools.info"

void test_psram();
void test_websocket();
void test_echo();

void app_main(void)
{
    ESP_LOGI(TAG, "app_main started ---------------------------------------");

    test_psram();
    ESP_LOGI(TAG, "------------------------------------------------------");
    // test_websocket();
    // ESP_LOGI(TAG, "------------------------------------------------------");
    test_echo();

    ESP_LOGI(TAG, "app_main finished ---------------------------------------");
}

void test_echo() {
    ESP_LOGI(TAG, "Testing audio echo...");

    // 启动音频回声任务
    audio_echo_task_start();

    // 主任务可以继续执行其他操作，或者进入低功耗模式
    // 这里我们让它每10秒打印一次日志，以证明它没有被阻塞
    while(1) {
        ESP_LOGI(TAG, "Main task is still running...");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    ESP_LOGI(TAG, "Audio echo test completed.");
}

void test_psram() {
    ESP_LOGI(TAG, "Testing PSRAM...");

    size_t psram_size = esp_psram_get_size();
    printf("PSRAM size: %d bytes\n", psram_size);
    size_t heap_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    printf("Heap size: %d bytes\n", heap_size);
    size_t free_heap_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    printf("Free heap size: %d bytes\n", free_heap_size);
    // 分配1M内存测试
    char *test_buffer = (char *)heap_caps_malloc(1024 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (test_buffer) {
        printf("Allocated 1M memory successfully.\n");
        free_heap_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        printf("Free heap size after allocation: %d bytes\n", free_heap_size);
        heap_caps_free(test_buffer);
    } else {
        printf("Failed to allocate 1M memory.\n");
    }
}

void test_websocket() {
    ESP_LOGI(TAG, "Testing WebSocket client...");

    // 初始化NVS. Wi-Fi驱动依赖于NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 调用Wi-Fi模块的初始化函数，扫描并连接wifi（非阻塞的）
    ESP_LOGI(TAG, "Wi-Fi initialization...");
    wifi_init_sta();

    // 在这里可以继续执行其他任务
    // 注意：wifi_init_sta() 是非阻塞的，连接过程在后台进行。
    int i = 0;
    while (1) {
        ESP_LOGI(TAG, "[%02d] Main loop is running...", i++);
        
        // 等待直到Wi-Fi连接成功
        if (wifi_is_connected()) {
            if (!websocket_is_initialized()) {
                // 1.客户端未初始化 -> 初始化WebSocket客户端，并连接到服务器
                ESP_LOGI(TAG, "Wi-Fi is ready, initializing & starting WebSocket client...");
                websocket_client_start();
            } else if (!websocket_is_connected()) {
                // 2.客户端已初始化，但未连接到服务器 -> 
                //  i.正在连接中
                if (websocket_is_connecting()) {
                    ESP_LOGI(TAG, "WebSocket client is connecting, waiting...");
                } else {
                    // ii.未连接，尝试重连
                    ESP_LOGI(TAG, "WebSocket client is not connected, attempting to reconnect...");
                    websocket_reconnect();
                }
            } else {
                // 3.如果WebSocket已连接，发送测试消息
                ESP_LOGI(TAG, "Sending a test message via WebSocket...");
                if (i < 4) {
                    // 发送测试消息
                    websocket_client_send_text("Hello from ESP32!");
                } else if (i < 6) {
                    // 发送其他消息
                    websocket_client_send_text("exit");
                } else {
                    ESP_LOGI(TAG, "Closing WebSocket client after test messages...");
                    websocket_client_close_clean();
                }
                vTaskDelay(pdMS_TO_TICKS(15000));
            }
        } else {
            ESP_LOGW(TAG, "Wi-Fi is not connected. Waiting...");
            // 如果Wi-Fi断开, 确保WebSocket客户端也停止
            if (websocket_is_connected()) {
                ESP_LOGI(TAG, "Wi-Fi connection lost, stopping WebSocket client.");
                websocket_client_close_clean();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}