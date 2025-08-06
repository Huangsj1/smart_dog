#ifndef MAX98357_I2S_H
#define MAX98357_I2S_H

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"

/**
 * @brief MAX98357 声道模式
 * 一个麦克风就只能选择单声道输出；
 * 两个麦克风就选择双声道输出
 */
typedef enum {
    MAX98357_CHANNEL_MONO,      /*!< 输出单声道 */
    MAX98357_CHANNEL_STEREO, /*!< 输出双声道 */
} max98357_channel_mode_t;

/**
 * @brief MAX98357 声道掩码
 * 一个功放只能输出单声道或双声道音频；
 * 两个功放就选择双声道输出
 *
 * 用于设置 I2S 标准模式的 slot_mask。
 * 这里使用 I2S_STD_SLOT_LEFT 和 I2S_STD_SLOT_RIGHT 来表示左声道和右声道。
 */
typedef enum {
    MAX98357_MASK_LEFT  = I2S_STD_SLOT_LEFT,   /*!< 左声道 */
    MAX98357_MASK_RIGHT = I2S_STD_SLOT_RIGHT,  /*!< 右声道 */
    MAX98357_MASK_BOTH  = I2S_STD_SLOT_BOTH, /*!< 双声道 */
} max98357_slot_mask_t;

/**
 * @brief MAX98357 I2S 配置结构体
 */
typedef struct {
    int                       gpio_bclk;      /*!< BCLK 引脚 */
    int                       gpio_ws;        /*!< WS (LRCK) 引脚 */
    int                       gpio_dout;      /*!< DOUT (DATA) 引脚 */
    int                       sample_rate;    /*!< 采样率，例如 16000 */
    max98357_channel_mode_t   channel_mode;   /*!< 声道模式 */
    max98357_slot_mask_t      slot_mask;       /*!< 声道掩码，用于设置 I2S 标准模式的 slot_mask */
    i2s_data_bit_width_t      bits_per_sample;/*!< 采样位宽, 通常为 16 或 32 位 */
} max98357_i2s_config_t;

/**
 * @brief 初始化 MAX98357 I2S 驱动
 *
 * 根据提供的配置初始化I2S总线并启动发送通道。
 *
 * @param config 指向配置结构体的指针
 * @return
 * - ESP_OK: 初始化成功
 * - 其他: 初始化失败
 */
esp_err_t max98357_i2s_init(const max98357_i2s_config_t *config);

/**
 * @brief 关闭 MAX98357 I2S 驱动
 *
 * 停止I2S通道并释放所有资源。
 *
 * @return
 * - ESP_OK: 成功
 */
esp_err_t max98357_i2s_close(void);

/**
 * @brief 向 MAX98357 写入I2S数据以供播放
 *
 * 这是一个阻塞函数，它会等待直到所有数据都被写入DMA缓冲区或超时。
 *
 * @param[in] src             指向待播放数据的缓冲区的指针
 * @param[in] size            期望写入的字节数
 * @param[out] bytes_written  实际写入的字节数
 * @param[in] ticks_to_wait   等待的 FreeRTOS Ticks 数，使用 portMAX_DELAY 表示一直等待
 * @return
 * - ESP_OK: 写入成功
 * - 其他: 写入失败
 */
esp_err_t max98357_i2s_write(const void *src, size_t size, size_t *bytes_written, TickType_t ticks_to_wait);

#endif // MAX98357_I2S_H
