#ifndef SR_H
#define SR_H

#include "esp_err.h"

/**
 * @brief 启动语音识别功能
 *
 * 该函数会初始化所有必要的模型（AFE, Wakenet, Multinet）、I2S驱动，
 * 并创建后台任务来处理音频流和语音识别。
 * 这是一个非阻塞函数。
 *
 * @return
 * - ESP_OK: 成功启动
 * - 其他: 启动失败
 */
esp_err_t sr_start(void);

/**
 * @brief 停止语音识别功能
 *
 * 该函数会停止并清理所有相关的任务和资源，包括：AFE、MN、麦克风和扬声器等。
 *
 * @return
 * - ESP_OK: 成功停止
 * - 其他: 停止失败
 */
esp_err_t sr_stop(void);

#endif
