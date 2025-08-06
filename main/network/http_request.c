#include <string.h> // for memcpy, memset
#include "freertos/FreeRTOS.h" // for heap_caps_malloc, etc.
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"

// 推荐将头文件声明放在一个专门的 .h 文件中
#include "http_request.h"

static const char *TAG = "HTTP_REQUEST";

// 定义最大HTTP输出缓冲区大小（超出部分截断）
#define MAX_HTTP_OUTPUT_BUFFER 4096

/**
 * @brief 用于传递给HTTP事件处理器的状态结构体
 */
typedef struct {
    char *buffer;           // 指向存储响应数据的缓冲区
    int buffer_size;        // 缓冲区的总大小
    int data_len;           // 当前已接收数据的长度
} http_response_data_t;

// HTTP 请求的事件处理函数
esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    // 从 user_data 获取我们的状态结构体
    http_response_data_t *response_data = (http_response_data_t *)evt->user_data;

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            // 请求开始时，重置数据长度
            response_data->data_len = 0;
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // 检查缓冲区是否已满
            if (response_data->data_len + evt->data_len < response_data->buffer_size) {
                // 将新数据追加到缓冲区
                memcpy(response_data->buffer + response_data->data_len, evt->data, evt->data_len);
                response_data->data_len += evt->data_len;
            } else {
                // 缓冲区空间不足，不再接收更多数据，防止溢出
                ESP_LOGW(TAG, "HTTP output buffer is full, truncating response.");
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            // 确保响应字符串以空字符结尾
            if (response_data->buffer) {
                response_data->buffer[response_data->data_len] = '\0';
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            // 在这里可以处理意外断开的清理逻辑，但主要清理工作在 request 函数中完成
            break;
        // 其他事件 case 可以根据需要添加
        default:
            break;
    }
    return ESP_OK;
}

void parse_json(const char *json_string) {
    if (json_string == NULL) {
        ESP_LOGE(TAG, "JSON string is NULL, cannot parse.");
        return;
    }
    
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON string. Error before: [%s]", cJSON_GetErrorPtr());
        return;
    }

    cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "body");
    if (cJSON_IsString(body) && (body->valuestring != NULL)) {
        ESP_LOGI(TAG, "Parsed body: %s", body->valuestring);
    } else {
        ESP_LOGW(TAG, "Could not find 'body' property in JSON response.");
    }

    cJSON_Delete(root);
}

void request(const char *url) {
    // 1. 将资源声明放在函数开头
    char *buffer = NULL;
    esp_http_client_handle_t client = NULL;
    esp_err_t err;

    // 2. 分配缓冲区内存
    buffer = (char *)heap_caps_malloc(MAX_HTTP_OUTPUT_BUFFER, MALLOC_CAP_8BIT);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for HTTP output buffer");
        return; // 使用 return 而不是 abort()，让调用者可以处理失败
    }
    
    // 初始化缓冲区
    memset(buffer, 0, MAX_HTTP_OUTPUT_BUFFER);

    // 3. 创建并初始化用于事件处理器的状态结构体
    http_response_data_t response_data = {
        .buffer = buffer,
        .buffer_size = MAX_HTTP_OUTPUT_BUFFER,
        .data_len = 0
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .user_data = &response_data, // 传递状态结构体的地址
        .disable_auto_redirect = true, // 建议明确设置重定向策略
    };

    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        goto cleanup; // 跳转到清理步骤
    }

    // 4. 执行 HTTP GET 请求
    err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                 status_code,
                 esp_http_client_get_content_length(client));
        // 检查状态码是否为200 OK
        if (status_code == 200) {
            ESP_LOGI(TAG, "Response (len=%d): %s", response_data.data_len, response_data.buffer);
            parse_json(response_data.buffer);
        } else {
            ESP_LOGW(TAG, "Server returned non-200 status code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

// 5. 使用 goto 进行统一的资源清理，这是C语言中推荐的错误处理模式
cleanup:
    if (client) {
        esp_http_client_cleanup(client);
    }
    if (buffer) {
        heap_caps_free(buffer);
    }
}