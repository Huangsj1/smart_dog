#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"
#include <stdbool.h> // 包含此头文件以使用 bool 类型

/**
 * @brief 初始化Wi-Fi Station模式、扫描、并连接到配置好的AP
 *
 * 这个函数会完成以下工作：
 * 1. 初始化底层TCP/IP协议栈和事件循环。
 * 2. 初始化Wi-Fi并设置为Station模式。
 * 3. 扫描周围的Wi-Fi网络并打印列表。
 * 4. 使用预设或用户输入的凭据连接到目标AP。
 * 5. 阻塞等待，直到连接成功或达到最大重试次数。
 *
 * @note 此函数在返回前会一直阻塞。
 */
void wifi_init_sta(void);

/**
 * @brief 扫描并打印所有可用的Wi-Fi网络
 */
void wifi_scan(void);

/**
 * @brief 检查Wi-Fi当前是否已连接成功（并获取到IP）
 *
 * @return true 如果Wi-Fi已连接, false 如果未连接.
 */
bool wifi_is_connected(void);


#endif // WIFI_H