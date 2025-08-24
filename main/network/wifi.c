#include <string.h>
#include <stdbool.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"

// 包含我们自己创建的头文件
#include "wifi.h"

// --- 内部宏定义 ---
#define LIGHT_ESP_MAXIMUM_RETRY 5
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

// --- 内部静态变量 ---
static const char *TAG = "wifi_station";

// FreeRTOS事件组，用于在连接成功或失败时发出信号
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

// --- 模拟配置结构体 ---
typedef struct {
    char ssid[32];
    char password[64];
} wifi_credentials_t;

// 初始默认值，实际使用时会被用户输入覆盖
static wifi_credentials_t wifi_config_data = {
    .ssid = "USER_306633",      // TODO: 后续从NVS或网页配置写入
    .password = "31531515"
};


// --- 内部静态函数声明 ---
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void get_wifi_credentials_from_user(wifi_credentials_t *credentials);


// --- 公共函数实现 (在 wifi.h 中声明) ---

void wifi_scan(void)
{
    ESP_LOGI(TAG, "Starting Wi-Fi scan...");
    
    // 开始扫描（阻塞式）
    esp_wifi_scan_start(NULL, true);

    uint16_t ap_num;
    wifi_ap_record_t *ap_records;

    esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_num == 0) {
        ESP_LOGI(TAG, "No AP found");
        return;
    }
    ESP_LOGI(TAG, "Found %d APs", ap_num);

    ap_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_num);
    if (!ap_records) {
        ESP_LOGE(TAG, "Failed to allocate memory for AP records");
        return;
    }
    
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));

    for (int i = 0; i < ap_num; i++) {
        ESP_LOGI(TAG, "SSID:%s\t RSSI:%d\t BSSID:" MACSTR, ap_records[i].ssid, ap_records[i].rssi, MAC2STR(ap_records[i].bssid));
    }
    
    free(ap_records);
}


void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理器
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // 配置Wi-Fi模式为Station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    
    // 启动Wi-Fi驱动
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished. Waiting for STA_START event...");
    
    // 注意：实际的扫描、配置和连接操作被移动到了WIFI_EVENT_STA_START事件的回调中
    // 这样做更符合ESP-IDF的事件驱动模型
}


/**
 * @brief 检查Wi-Fi当前是否已连接成功（并获取到IP）
 */
bool wifi_is_connected(void)
{
    // 如果事件组还未创建，那肯定没连接
    if (s_wifi_event_group == NULL) {
        return false;
    }
    
    // 获取事件组的当前位状态
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);

    // 检查WIFI_CONNECTED_BIT是否被设置
    if ((bits & WIFI_CONNECTED_BIT) != 0) {
        // 确实被设置了，说明已经连接
        return true;
    } else {
        // 未被设置，说明未连接或已断开
        return false;
    }
}


// --- 内部静态函数实现 ---

// 模拟用户输入WiFi凭据的函数
static void get_wifi_credentials_from_user(wifi_credentials_t *credentials)
{
    // 在真实应用中，这里应该实现真正的用户交互
    ESP_LOGI(TAG, "Waiting for user to select WiFi and enter password... (using default credentials)");
    // strcpy(credentials->ssid, "Xiaomi_251D");
    // strcpy(credentials->password, "Aa123456");
    ESP_LOGI(TAG, "Using SSID: %s", credentials->ssid);
}


// Wi-Fi和IP事件的回调函数
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START: Wi-Fi station started.");
        
        // 扫描可用的Wi-Fi网络
        ESP_LOGI(TAG, "Scanning available WiFi networks...");
        wifi_scan();
        
        // 从用户获取Wi-Fi凭据
        get_wifi_credentials_from_user(&wifi_config_data);

        // 配置Wi-Fi连接参数
        wifi_config_t wifi_config = {
            .sta = {
                /* authmode a ouvert */
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = {
                    .capable = true,
                    .required = false
                },
            },
        };
        strncpy((char*)wifi_config.sta.ssid, wifi_config_data.ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, wifi_config_data.password, sizeof(wifi_config.sta.password));
        
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        
        // 开始连接
        esp_wifi_connect();
        ESP_LOGI(TAG, "Connecting to AP: %s...", wifi_config_data.ssid);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // 【重要】当Wi-Fi断开连接时，清除连接成功标志位
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (s_retry_num < LIGHT_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (%d/%d)", s_retry_num, LIGHT_ESP_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Connection failed after %d retries", LIGHT_ESP_MAXIMUM_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        // 设置连接成功标志位
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        // 这里可以执行连接成功后的操作
        ESP_LOGI(TAG, "Connected to SSID: %s", wifi_config_data.ssid);

    }
}