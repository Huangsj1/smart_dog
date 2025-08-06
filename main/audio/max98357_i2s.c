#include "max98357_i2s.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include <string.h>

// I2S DMA 缓冲区配置
#define DMA_DESC_NUM  4
#define DMA_FRAME_NUM 512

static const char *TAG = "MAX98357_DRIVER";

// 模块级静态变量，用于保存I2S发送通道句柄
static i2s_chan_handle_t s_tx_chan = NULL;

/**
 * @brief 初始化 MAX98357 I2S 驱动
 */
esp_err_t max98357_i2s_init(const max98357_i2s_config_t *config)
{
    // 1. 检查输入参数是否有效
    if (config == NULL) {
        ESP_LOGE(TAG, "Configuration cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_tx_chan != NULL) {
        ESP_LOGW(TAG, "I2S TX driver already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing MAX98357 I2S driver...");

    // 2. 配置I2S通道
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_DESC_NUM;
    chan_cfg.dma_frame_num = DMA_FRAME_NUM;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx_chan, NULL)); // 创建 TX 通道

    // 3. 配置I2S标准模式
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(config->bits_per_sample, 
            (config->channel_mode == MAX98357_CHANNEL_STEREO) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = config->gpio_bclk,
            .ws   = config->gpio_ws,
            .dout = config->gpio_dout,
            .din  = I2S_GPIO_UNUSED, // 不使用输入引脚
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    // 根据掩码设置正确的slot_mask
    switch (config->slot_mask) {
        case MAX98357_MASK_LEFT:
            std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
            break;
        case MAX98357_MASK_RIGHT:
            std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
            break;
        case MAX98357_MASK_BOTH:
        default:
            std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
            break;
    }

    // 4. 初始化I2S标准模式
    esp_err_t ret = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init std mode: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        return ret;
    }

    // 5. 启动发送
    ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable channel: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "I2S TX driver initialized successfully.");
    return ESP_OK;
}

/**
 * @brief 关闭 MAX98357 I2S 驱动
 */
esp_err_t max98357_i2s_close(void)
{
    if (s_tx_chan == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "De-initializing I2S TX driver...");
    i2s_channel_disable(s_tx_chan);
    esp_err_t ret = i2s_del_channel(s_tx_chan);
    s_tx_chan = NULL;
    return ret;
}

/**
 * @brief 向 MAX98357 写入I2S数据
 */
esp_err_t max98357_i2s_write(const void *src, size_t size, size_t *bytes_written, TickType_t ticks_to_wait)
{
    if (s_tx_chan == NULL) {
        ESP_LOGE(TAG, "I2S TX driver is not initialized.");
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_channel_write(s_tx_chan, src, size, bytes_written, ticks_to_wait);
}
