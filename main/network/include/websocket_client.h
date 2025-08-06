#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief 初始化 WebSocket 客户端，启动 WebSocket 客户端并连接到服务器。
 *
 * 这是一个非阻塞函数，它会开始连接过程。
 *
 * @return 成功返回 ESP_OK，失败返回错误码。
 */
esp_err_t websocket_client_start(void);

/**
 * @brief 关闭 WebSocket 连接并清理资源。
 * 
 * 组合调用 close 和 cleanup 函数。
 * 
 * @return 成功返回 ESP_OK，失败返回错误码。
 */
esp_err_t websocket_client_close_clean(void);

/**
 * @brief 重新连接 WebSocket 客户端。
 * 
 * 关闭当前连接，释放资源，然后重新初始化并启动。
 * 
 * @return 成功返回 ESP_OK，失败返回错误码。
 */
esp_err_t websocket_reconnect(void);

/**
 * @brief 通过 WebSocket 发送文本消息。
 *
 * @param text 要发送的以 null 结尾的字符串。
 * @return 成功返回 ESP_OK，如果客户端未连接则返回 ESP_FAIL。
 */
esp_err_t websocket_client_send_text(const char *text);

/**
 * @brief 通过 WebSocket 发送二进制消息。
 *
 * @param data 指向二进制数据的指针。
 * @param len  数据的长度。
 * @return 成功返回 ESP_OK，如果客户端未连接则返回 ESP_FAIL。
 */
esp_err_t websocket_client_send_binary(const uint8_t *data, int len);

/**
 * @brief 检查 WebSocket 客户端当前是否已连接。
 *
 * @return true 如果已连接，否则返回 false。
 */
bool websocket_is_connected(void);

/**
 * @brief 检查 WebSocket 客户端是否已初始化。
 *
 * @return true 如果已初始化，否则返回 false。
 */
bool websocket_is_initialized(void);

/**
 * @brief 检查 WebSocket 客户端是否正在连接中。
 *
 * @return true 如果正在连接中，否则返回 false。
 */
bool websocket_is_connecting(void);

#endif // WEBSOCKET_CLIENT_H
