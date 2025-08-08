#include "inmp441_i2s.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include <string.h>

// I2S DMA 缓冲区配置
#define DMA_DESC_NUM  4
#define DMA_FRAME_NUM 512

static const char *TAG = "INMP441_DRIVER";

// 模块级静态变量，用于保存I2S通道句柄
static i2s_chan_handle_t s_rx_chan = NULL;

/**
 * @brief 初始化 INMP441 I2S 驱动
 */
esp_err_t inmp441_i2s_init(const inmp441_i2s_config_t *config)
{
    // 1. 检查输入参数是否有效
    if (config == NULL) {
        ESP_LOGE(TAG, "Configuration cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    // 防止重复初始化
    if (s_rx_chan != NULL) {
        ESP_LOGW(TAG, "I2S driver already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing INMP441 I2S driver...");

    // 2. 配置I2S通道
    // i2s_chan_config_t chan_cfg = {
    //     .id = I2S_NUM_0, // 使用I2S0
    //     .role = I2S_ROLE_MASTER,
    //     .dma_desc_num = DMA_DESC_NUM,
    //     .dma_frame_num = DMA_FRAME_NUM,
    //     .auto_clear = true, // 当缓冲区满时自动清除旧数据
    // };
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_DESC_NUM;
    chan_cfg.dma_frame_num = DMA_FRAME_NUM;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &s_rx_chan));

    // 3. 配置I2S标准模式
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(config->bits_per_sample, 
            (config->channel_mode == INMP441_CHANNEL_STEREO) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = config->gpio_bclk,
            .ws   = config->gpio_ws,
            .din  = config->gpio_din,
            .dout = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    // 根据通道模式设置正确的slot_mask
    switch (config->channel_mode) {
        case INMP441_CHANNEL_LEFT:
            std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
            break;
        case INMP441_CHANNEL_RIGHT:
            std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
            break;
        case INMP441_CHANNEL_STEREO:
        default:
            std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
            break;
    }

    // 4. 初始化通道
    esp_err_t ret = i2s_channel_init_std_mode(s_rx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init std mode: %s", esp_err_to_name(ret));
        // 初始化失败时，清理已创建的通道
        i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
        return ret;
    }

    // 5. 启动接收
    ret = i2s_channel_enable(s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable channel: %s", esp_err_to_name(ret));
        i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "I2S driver initialized successfully.");
    return ESP_OK;
}

/**
 * @brief 关闭 INMP441 I2S 驱动
 */
esp_err_t inmp441_i2s_close(void)
{
    if (s_rx_chan == NULL) {
        return ESP_OK; // 已经反初始化
    }

    ESP_LOGI(TAG, "De-initializing I2S driver...");
    // 必须先 disable 再 delete
    i2s_channel_disable(s_rx_chan);
    esp_err_t ret = i2s_del_channel(s_rx_chan);
    s_rx_chan = NULL; // 将句柄设为NULL，防止悬空指针
    return ret;
}

/**
 * @brief 从 INMP441 读取I2S数据
 */
esp_err_t inmp441_i2s_read(void *dest, size_t size, size_t *bytes_read, TickType_t ticks_to_wait)
{
    if (s_rx_chan == NULL) {
        ESP_LOGE(TAG, "I2S driver is not initialized.");
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_channel_read(s_rx_chan, dest, size, bytes_read, ticks_to_wait);
}
