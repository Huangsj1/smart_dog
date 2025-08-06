#ifndef INMP441_I2S_H
#define INMP441_I2S_H

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"

/**
 * @brief INMP441 声道模式
 * 一个麦克风就只能选择单声道输出；
 * 两个麦克风就选择双声道输出
 */
typedef enum {
    INMP441_CHANNEL_LEFT,   /*!< 仅采集左声道 */
    INMP441_CHANNEL_RIGHT,  /*!< 仅采集右声道 */
    INMP441_CHANNEL_STEREO, /*!< 采集双声道 */
} inmp441_channel_mode_t;

/**
 * @brief INMP441 I2S 配置结构体
 */
typedef struct {
    int                     gpio_bclk;      /*!< BCLK 引脚 */
    int                     gpio_ws;        /*!< WS (LRCK) 引脚 */
    int                     gpio_din;       /*!< DIN (DATA) 引脚 */
    int                     sample_rate;    /*!< 采样率，例如 16000 */
    inmp441_channel_mode_t  channel_mode;   /*!< 声道模式 */
    i2s_data_bit_width_t    bits_per_sample;/*!< 采样位宽, 通常为 16 或 32 位 */
} inmp441_i2s_config_t;

/**
 * @brief 初始化 INMP441 I2S 驱动
 *
 * 根据提供的配置初始化I2S总线并启动接收通道。
 *
 * @param config 指向配置结构体的指针
 * @return
 * - ESP_OK: 初始化成功
 * - 其他: 初始化失败
 */
esp_err_t inmp441_i2s_init(const inmp441_i2s_config_t *config);

/**
 * @brief 关闭 INMP441 I2S 驱动
 *
 * 停止I2S通道并释放所有资源。
 *
 * @return
 * - ESP_OK: 成功
 */
esp_err_t inmp441_i2s_close(void);

/**
 * @brief 从 INMP441 读取I2S数据
 *
 * 这是一个阻塞函数，它会等待直到读取到指定大小的数据或超时。
 *
 * @param[out] dest         指向存储读取数据的缓冲区的指针
 * @param[in]  size         期望读取的字节数
 * @param[out] bytes_read   实际读取到的字节数
 * @param[in]  ticks_to_wait 等待的 FreeRTOS Ticks 数，使用 portMAX_DELAY 表示一直等待
 * @return
 * - ESP_OK: 读取成功
 * - 其他: 读取失败
 */
esp_err_t inmp441_i2s_read(void *dest, size_t size, size_t *bytes_read, TickType_t ticks_to_wait);

#endif // INMP441_I2S_H