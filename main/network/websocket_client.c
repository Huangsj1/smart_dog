#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "cJSON.h"

// 包含我们自己创建的头文件
#include "websocket_client.h"

// --- 模块内部定义 ---
// !!! 重要: 请将此处的IP地址和端口替换为您Python服务器的实际地址 !!!
#define WEBSOCKET_URI "ws://192.168.31.55:8000/ws" // 替换为您电脑的实际IP地址（不能用localhost，因为这是在开发板上面运行的，不是本机）

static const char *TAG = "WEBSOCKET_CLIENT";

// --- 静态变量 (模块内部使用) ---
static esp_websocket_client_handle_t client = NULL;

static volatile bool is_connecting = false; // 用于标记是否正在连接

// --- 静态函数声明 ---
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

// --- 公共函数实现 ---
// 连接&断开相关--------------------------------------------------------------------------------
// 一、初始化websocket client，启动WebSocket客户端（连接到服务器）
esp_err_t websocket_client_start(void) {
    if (client) {
        ESP_LOGW(TAG, "Client already started. Skipping.");
        return ESP_OK;
    }

    // 1. 创建 WebSocket 客户端配置
    esp_websocket_client_config_t websocket_cfg = {
        .uri = WEBSOCKET_URI,
    };

    ESP_LOGI(TAG, "Initializing WebSocket server at %s...", websocket_cfg.uri);

    // 2. 初始化 WebSocket 客户端
    client = esp_websocket_client_init(&websocket_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client");
        return ESP_FAIL;
    }

    // 3. 注册事件处理器
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    
    // 4. 启动连接
    is_connecting = true;
    if (esp_websocket_client_start(client) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket client");
        return ESP_FAIL;
    }

    return ESP_OK;
}


// 二、关闭WebSocket连接（发送CLOSE消息等待连接断开） & 清理资源
esp_err_t websocket_client_close_clean(void)
{
    if (client == NULL) {
        return ESP_OK; // 如果客户端未初始化，直接返回成功
    }
    // 1.关闭连接
    if (esp_websocket_client_is_connected(client)) {
        // 最大等待事件，超出了就强制断开连接
        TickType_t timeout = pdMS_TO_TICKS(3000); 
        esp_err_t err = esp_websocket_client_close(client, timeout);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to close client gracefully: %s", esp_err_to_name(err));
        }
    }
    
    // 2.清理资源
    esp_err_t err = esp_websocket_client_destroy(client);
    if (err != ESP_OK) {
         ESP_LOGE(TAG, "Failed to destroy client: %s", esp_err_to_name(err));
    }
    client = NULL;
    is_connecting = false; // 客户端被销毁，重置连接标志

    ESP_LOGI(TAG, "WebSocket client closed and cleaned.");

    return ESP_OK;
}

// 三、重连WebSocket客户端
esp_err_t websocket_reconnect(void)
{
    ESP_LOGI(TAG, "Reconnecting WebSocket client...");

    // 关闭连接 & 释放资源
    websocket_client_close_clean();

    // 重新初始化 & 启动
    websocket_client_start();

    return ESP_OK;
}


// 下面的函数用于发送消息到WebSocket服务器------------------------------------------------
esp_err_t websocket_client_send_text(const char *text) {
    if (!websocket_is_connected()) {
        ESP_LOGE(TAG, "Cannot send text: WebSocket is not connected.");
        return ESP_FAIL;
    }
    int len = strlen(text);
    ESP_LOGI(TAG, "Sending text: %s", text);
    return esp_websocket_client_send_text(client, text, len, portMAX_DELAY);
}

esp_err_t websocket_client_send_binary(const uint8_t *data, int len) {
    if (!websocket_is_connected()) {
        ESP_LOGE(TAG, "Cannot send binary data: WebSocket is not connected.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Sending binary data of length %d", len);
    return esp_websocket_client_send_bin(client, (const char *)data, len, portMAX_DELAY);
}

bool websocket_is_connected(void) {
    return (client != NULL && esp_websocket_client_is_connected(client));
}

bool websocket_is_initialized(void) {
    return (client != NULL);
}

bool websocket_is_connecting(void) {
    return is_connecting;
}

// --- 静态函数实现 ---

static void log_error_if_nonzero(const char *message, int error_code) {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED: Connection established.");
            is_connecting = false; // 连接成功，重置标志
            // TODO: 在这里可以发送一个初始的握手消息或设置一个准备就绪的标志
            break;
        // 注意：不能在此处调用 websocket_client_cleanup()，因为这里是在client的事件处理上下文中，
        //      destroy的话会释放client资源，即释放掉当前资源，会出错
        //      所以需要交给使用websocket的任务来处理
        case WEBSOCKET_EVENT_DISCONNECTED:
            // 异常断开连接
            ESP_LOGE(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            is_connecting = false; // 连接断开，重置连接标志
            // 此事件可用于在需要时触发重连逻辑
            // websocket_client_cleanup();
            break;
        case WEBSOCKET_EVENT_CLOSED:
            // 正常关闭连接（可能是服务器断开，也可能是客户端主动断开）
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CLOSED: Connection closed.");
            is_connecting = false; // 连接关闭，重置连接标志
            // websocket_client_cleanup();
            break;
        case WEBSOCKET_EVENT_DATA:
            ESP_LOGV(TAG, "WEBSOCKET_EVENT_DATA received");
            ESP_LOGV(TAG, "Received opcode=%d, len=%d", data->op_code, data->data_len);

            // 根据操作码处理不同类型的消息
            if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
                // 处理文本数据 (例如: JSON格式的控制消息)
                ESP_LOGI(TAG, "Received text data: %.*s", data->data_len, (char *)data->data_ptr);
                // 示例: 解析 session_id 或 stream_end 信号
                // cJSON *root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
                // if (root) {
                //     // 处理JSON...
                //     cJSON_Delete(root);
                // }

            } else if (data->op_code == WS_TRANSPORT_OPCODES_BINARY) {
                // 处理二进制数据 (例如: 音频流)
                ESP_LOGI(TAG, "Received binary data of length %d, payload length: %d", data->data_len, data->payload_len);
                // TODO: 将此音频数据送入音频播放任务的队列中
                // audio_player_enqueue(data->data_ptr, data->data_len);
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
            is_connecting = false; // 连接错误，重置连接标志
            log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
            break;
        default:
            // 其他事件如 BEGIN, FINISH 等，如果需要调试可以取消注释
            ESP_LOGD(TAG, "Unhandled event: %ld", event_id);
            break;
    }
}
