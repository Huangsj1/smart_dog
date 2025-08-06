#include "audio_echo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "inmp441_i2s.h"
#include "max98357_i2s.h"
#include "i2s_pins.h"

static const char *TAG = "AUDIO_ECHO";

#define ECHO_TASK_STACK_SIZE (4 * 1024)
#define ECHO_TASK_PRIORITY   5

// 音频参数，必须对录音和播放保持一致
#define ECHO_SAMPLE_RATE     (16000)
#define ECHO_BITS_PER_SAMPLE (I2S_DATA_BIT_WIDTH_32BIT)

// 用于在录音和播放之间传递数据的缓冲区大小
#define ECHO_BUFFER_SIZE     (2048)

/**
 * @brief 实时回声任务
 * @param arg 未使用
 */
static void echo_task(void *arg)
{
    // 1. 配置 INMP441 录音驱动
    inmp441_i2s_config_t inmp441_config = {
        .gpio_bclk = EXAMPLE_I2S_BCLK_IO1,
        .gpio_ws = EXAMPLE_I2S_WS_IO1,
        .gpio_din = EXAMPLE_I2S_DIN_IO1,
        .sample_rate = ECHO_SAMPLE_RATE,
        .bits_per_sample = ECHO_BITS_PER_SAMPLE,
        .channel_mode = INMP441_CHANNEL_LEFT, // 假设使用左声道
        // .channel_mode = INMP441_CHANNEL_RIGHT, // 假设使用右声道
    };

    // 2. 配置 MAX98357 播放驱动
    max98357_i2s_config_t max98357_config = {
        .gpio_bclk = EXAMPLE_I2S_BCLK_IO2,
        .gpio_ws = EXAMPLE_I2S_WS_IO2,
        .gpio_dout = EXAMPLE_I2S_DOUT_IO2,
        .sample_rate = ECHO_SAMPLE_RATE,
        .bits_per_sample = ECHO_BITS_PER_SAMPLE,
        .channel_mode = MAX98357_CHANNEL_MONO, // 使用单声道
        .slot_mask = MAX98357_MASK_LEFT, // 使用左声道输出
    };

    // 3. 初始化两个驱动
    ESP_LOGI(TAG, "Initializing audio drivers...");
    if (inmp441_i2s_init(&inmp441_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize INMP441 driver");
        vTaskDelete(NULL);
    }
    if (max98357_i2s_init(&max98357_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MAX98357 driver");
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "Audio drivers initialized.");

    // 4. 分配音频缓冲区
    uint8_t *audio_buffer = (uint8_t *)malloc(ECHO_BUFFER_SIZE);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio buffer");
        vTaskDelete(NULL);
    }

    size_t bytes_read = 0;
    size_t bytes_written = 0;

    ESP_LOGI(TAG, "Starting echo loop...");
    while (1) {
        // 从麦克风读取数据
        esp_err_t read_ret = inmp441_i2s_read(audio_buffer, ECHO_BUFFER_SIZE, &bytes_read, portMAX_DELAY);

        if (read_ret == ESP_OK && bytes_read > 0) {
            // 将读取到的数据直接写入扬声器
            max98357_i2s_write(audio_buffer, bytes_read, &bytes_written, portMAX_DELAY);
        } else {
            ESP_LOGE(TAG, "I2S read error: %s", esp_err_to_name(read_ret));
        }
    }

    // 理论上不会执行到这里
    free(audio_buffer);
    inmp441_i2s_close();
    max98357_i2s_close();
    vTaskDelete(NULL);
}

/**
 * @brief 启动实时音频回声任务
 */
void audio_echo_task_start(void)
{
    xTaskCreate(echo_task, "echo_task", ECHO_TASK_STACK_SIZE, NULL, ECHO_TASK_PRIORITY, NULL);
    ESP_LOGI(TAG, "Audio echo task started.");
}
