#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"

#include "esp_mn_models.h"
#include "model_path.h"
#include "esp_mn_iface.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_mn_speech_commands.h"

#include "inmp441_i2s.h"
#include "max98357_i2s.h"
#include "i2s_pins.h"
#include "websocket_client.h"

#include "sr.h"

static const char *TAG = "sr";

#define AUDIO_SAMPLE_RATE     (16000)
#define AUDIO_BITS_PER_SAMPLE (I2S_DATA_BIT_WIDTH_16BIT)

typedef struct {
    wakenet_state_t     wakenet_mode;
    esp_mn_state_t      state;
    int                 command_id;
} sr_result_t;

// AFE 句柄与实例
static esp_afe_sr_iface_t *afe_handle = NULL;
static esp_afe_sr_data_t *afe_data = NULL;
// MN 句柄与实例
static const esp_mn_iface_t *multinet = NULL;
static model_iface_data_t *model_data = NULL;
// 任务队列
static QueueHandle_t g_result_que = NULL;

static volatile bool task_flag = false;
static bool detect_flag = false;

static const char *cmd_phoneme[] = {
    "da kai kong tiao",
    "guan bi kong tiao",
    "da kai dian shi",
    "guan bi dian shi",
    "da kai wo shi deng",
    "guan bi wo shi deng",
    "bo fang yin yue",
    "guan bi yin yue",
};

static void feed_Task(void*);
static void detect_Task(void*);
static void sr_handler_task(void*);

/**
 * @brief 启动语音识别
 * 
 * @return esp_err_t 
 */
esp_err_t sr_start(void)
{
    ESP_LOGI(TAG, "sr_start begin");

    // 一、afe配置
    // 1.获取所有模型 - 首先尝试使用嵌入式模型
    srmodel_list_t *models = esp_srmodel_init("model");
    // 2.afe配置（包含了各种afe模型的配置）
    afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);

    // 默认就是关闭AEC回声消除（如果使用喇叭就需要用）
    // afe_config->aec_init = false;
    // 过滤得到第一个包含 "wn" 前缀的唤醒词模型名称（第一个wakenet）
    afe_config->wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    // afe_config->wakenet_model_name_2 = esp_srmodel_filter(models, ESP_WN_PREFIX, "walle");

    ESP_LOGI(TAG, "wakenet model name: %s", afe_config->wakenet_model_name);
    // ESP_LOGI(TAG, "wakenet model name 2: %s", afe_config->wakenet_model_name_2);
    ESP_LOGI(TAG, "Number of models: %d", models->num);
    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(TAG, "Model %d: %s", i, models->model_name[i]);
    }
    ESP_LOGI(TAG, "Number of models: %d", models->num);

    // 3.获取afe句柄
    afe_handle = esp_afe_handle_from_config(afe_config);
    // 4.创建afe实例
    afe_data = afe_handle->create_from_config(afe_config);
    

    // 二、命令词模型
    // 1.过滤得到第一个mn模型
    char *mn_model_name = esp_srmodel_filter(models, ESP_MN_CHINESE, NULL);
    if (NULL == mn_model_name) {
        ESP_LOGE(TAG, "No Multinet model found for Chinese.");
    } else {
        ESP_LOGI(TAG, "Multinet model name: %s", mn_model_name);
    }
    // 2.获取mn句柄
    multinet = esp_mn_handle_from_name(mn_model_name);
    // 3.创建mn实例
    model_data = multinet->create(mn_model_name, 5760);//设置唤醒超时时间
    // 4.清空、添加、应用、打印指令
    esp_mn_commands_clear();//清除唤醒指令
    for (int i = 0; i < sizeof(cmd_phoneme) / sizeof(cmd_phoneme[0]); i++) {
        esp_mn_commands_add(i, (char *)cmd_phoneme[i]);//逐个将唤醒指令放入
    }
    esp_mn_commands_update();//更新命令词列表
    esp_mn_commands_print();
    ESP_LOGI(TAG, "---Multinet instructions created successfully.");
    multinet->print_active_speech_commands(model_data);//输出目前激活的命令词

    // 三、麦克风和扬声器配置
    // 1.初始化inmp441和max98357
    inmp441_i2s_config_t inmp441_config = {
        .gpio_bclk = EXAMPLE_I2S_BCLK_IO1,
        .gpio_ws = EXAMPLE_I2S_WS_IO1,
        .gpio_din = EXAMPLE_I2S_DIN_IO1,
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = AUDIO_BITS_PER_SAMPLE,
        .channel_mode = INMP441_CHANNEL_LEFT, // 假设使用左声道
    };
    max98357_i2s_config_t max98357_config = {
        .gpio_bclk = EXAMPLE_I2S_BCLK_IO2,
        .gpio_ws = EXAMPLE_I2S_WS_IO2,
        .gpio_dout = EXAMPLE_I2S_DOUT_IO2,
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = AUDIO_BITS_PER_SAMPLE,
        .channel_mode = MAX98357_CHANNEL_MONO, // 使用单声道
        .slot_mask = MAX98357_MASK_LEFT, // 使用左声道输出
    };
    inmp441_i2s_init(&inmp441_config);
    max98357_i2s_init(&max98357_config);

    // 四、创建afe任务（3个任务调用）
    task_flag = true; // 设置任务标志位为true，表示任务可以执行
    g_result_que = xQueueCreate(1, sizeof(sr_result_t));
    xTaskCreatePinnedToCore(feed_Task, "feed_Task", 4 * 1024, afe_data, 5, NULL, 0);
    xTaskCreatePinnedToCore(detect_Task, "detect_Task", 6 * 1024, afe_data, 5, NULL, 1);
    xTaskCreatePinnedToCore(sr_handler_task, "sr_handler_task", 4 * 1024, g_result_que, 1, NULL, 0);

    ESP_LOGI(TAG, "sr_start done");
    return ESP_OK;
}

/**
 * @brief 停止语音识别
 * 清除所有资源，包括：AFE、MN、麦克风和扬声器等
 * 
 * @return esp_err_t 
 */
esp_err_t sr_stop(void) 
{
    ESP_LOGI(TAG, "sr_stop begin");

    // 停止任务
    task_flag = false;
    detect_flag = false;

    // 关闭麦克风和扬声器
    inmp441_i2s_close();
    max98357_i2s_close();

    // 销毁AFE句柄
    if (afe_handle) {
        afe_handle->destroy(afe_data);
        afe_handle = NULL;
    }

    // 销毁MN句柄
    if (multinet && model_data) {
        multinet->destroy(model_data);
        model_data = NULL;
        multinet = NULL;
    }

    // 清理结果队列
    if (g_result_que) {
        vQueueDelete(g_result_que);
        g_result_que = NULL;
    }

    ESP_LOGI(TAG, "sr_stop done");
    return ESP_OK;
}


/**
 * @brief 音频采集任务
 * 读取麦克风音频并传递给AFE处理
 * 
 * @param arg afe数据
 */
static void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t*)arg;

    // 1.获取采集音频数据帧长、通道数
    int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);
    ESP_LOGI(TAG, "feed_Task started with chunksize: %d, channels: %d", feed_chunksize, feed_nch);
    // 2.分配内存存储采集的音频数据
    // int16_t *feed_buff = (int16_t *) heap_caps_malloc(feed_chunksize * feed_nch * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *feed_buff = (int16_t *) malloc(feed_chunksize * feed_nch * sizeof(int16_t));
    assert(feed_buff);

    // 循环采集音频数据
    while (task_flag) {
        size_t bytesIn = 0;
        // 3.从I2S读取音频数据
        // esp_err_t result = i2s_read(I2S_NUM_0, feed_buff, feed_chunksize * feed_nch * sizeof(int16_t), &bytesIn, portMAX_DELAY);
        esp_err_t result = inmp441_i2s_read(feed_buff, feed_chunksize * feed_nch * sizeof(int16_t), &bytesIn, portMAX_DELAY);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read audio data from INMP441: %s", esp_err_to_name(result));
            continue; // 如果读取失败，继续下一次循环
        }

        // 4.将采集到的音频数据传递给AFE处理
        afe_handle->feed(afe_data, feed_buff);
    }

    // 5.释放采集的音频数据缓冲区
    if (feed_buff) {
        free(feed_buff);
        feed_buff = NULL;
    }

    ESP_LOGI(TAG, "[feed_Task] finished");

    vTaskDelete(NULL);
}

/**
 * @brief 音频检测任务
 * 该任务负责从AFE获取音频数据，并进行唤醒词检测和指令识别
 * 
 * @param arg afe数据
 */
static void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t*)arg;

    // 1.获取采集音频数据帧长、通道数
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    int afe_nch = afe_handle->get_feed_channel_num(afe_data);

    int mn_chunksize = multinet->get_samp_chunksize(model_data);
    int mn_chunknum = multinet->get_samp_chunknum(model_data);
    ESP_LOGI(TAG, "detect_Task started with chunksize: %d, channels: %d, mn_chunksize: %d, mn_chunknum: %d", 
             afe_chunksize, afe_nch, mn_chunksize, mn_chunknum);
    
    assert(afe_chunksize == mn_chunksize);
    ESP_LOGI(TAG, "------------detect start------------");

    while (task_flag) {
        // 3.从AFE获取音频数据（res->data_size = 1024（字节数=512*2））
        afe_fetch_result_t* res = afe_handle->fetch(afe_data);

        // 在MAX98357中播放
        max98357_i2s_write(res->data, res->data_size, NULL, portMAX_DELAY);
        // 发送给服务器播放
        websocket_client_send_binary((const uint8_t *)res->data, res->data_size);


        if (!res || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "fetch error!");
            break;
        }

        // 4.1.检测到唤醒词（但是要等到verify之后才能获取afe数据）
        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "model index:%d, word index:%d", res->wakenet_model_index, res->wake_word_index);
            ESP_LOGI(TAG, "-----------LISTENING-----------");
            sr_result_t result = {
                .wakenet_mode = WAKENET_DETECTED,
                .state = ESP_MN_STATE_DETECTING,
                .command_id = 0,
            };
            // 开启命令词
            detect_flag = true;
            // 将唤醒结果发送到结果队列
            xQueueSend(g_result_que, &result, 10);
        } else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
            // 4.2.对于多通道的AFE，需要等到唤醒词验证通过后才能进行指令检测（单通道不会进入这里）
            ESP_LOGI(TAG, "wakenet channel verified");
            detect_flag = true;
            // 禁用唤醒词检测
            afe_handle->disable_wakenet(afe_data);
        }

        // 5.如果唤醒词验证通过，则开始检测指令（每次唤醒会多次识别命令词，直到超时）
        if (true == detect_flag) {
            esp_mn_state_t mn_state = ESP_MN_STATE_DETECTING;

            // 获取mn模型的命令词检测结果（直接获取当前命令词检测状态）
            mn_state = multinet->detect(model_data, res->data);

            // i.检测中 - 不做任何处理，继续下一次循环快速fetch数据
            if (ESP_MN_STATE_DETECTING == mn_state) {
                continue;
            }

            // ii.超时
            if (ESP_MN_STATE_TIMEOUT == mn_state) {
                ESP_LOGW(TAG, "Time out");
                sr_result_t result = {
                    .wakenet_mode = WAKENET_NO_DETECT,
                    .state = mn_state,
                    .command_id = 0,
                };
                // 将超时结果发送到结果队列
                xQueueSend(g_result_que, &result, 10);
                // 清空afe缓冲区，因为存在延迟，所以丢弃旧的数据保证实时
                afe_handle->reset_buffer(afe_data);
                // 重新启用唤醒词检测
                afe_handle->enable_wakenet(afe_data);
                detect_flag = false;
                continue;
            }

            // iii.得到指令
            if (ESP_MN_STATE_DETECTED == mn_state) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                // 打印所有检测到的指令
                for (int i = 0; i < mn_result->num; i++) {
                    ESP_LOGI(TAG, "TOP %d, command_id: %d, phrase_id: %d, prob: %f",
                            i + 1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->prob[i]);
                }

                int sr_command_id = mn_result->command_id[0];
                ESP_LOGI(TAG, "Detected command : %d", sr_command_id);
                sr_result_t result = {
                    .wakenet_mode = WAKENET_NO_DETECT,
                    .state = mn_state,
                    .command_id = sr_command_id,
                };
                // 将检测到的指令结果发送到结果队列
                xQueueSend(g_result_que, &result, 10);
                // // 检测到命令后，重新启用唤醒词检测（一次唤醒词 -> 一次命令词）
                // afe_handle->enable_wakenet(afe_data);
                // detect_flag = false;
            }
        }
    }

    // // 6.清理mn资源
    // if (model_data) {
    //     multinet->destroy(model_data);
    //     model_data = NULL;
    // }

    vTaskDelete(NULL);
}

/**
 * @brief 结果处理任务
 * 该任务负责处理从AFE和MN获取的结果，并进行相应的操作
 * 
 * @param pvParam 结果队列句柄
 */
static void sr_handler_task(void *pvParam)
{
    QueueHandle_t xQueue = (QueueHandle_t)pvParam;

    while (true) {
        sr_result_t result;
        // 从队列中接收结果
        xQueueReceive(xQueue, &result, portMAX_DELAY);

        ESP_LOGI(TAG, "cmd:%d, wakemode:%d,state:%d", result.command_id, result.wakenet_mode, result.state);

        // 1.如果是命令词超时
        if (ESP_MN_STATE_TIMEOUT == result.state) {
            ESP_LOGI(TAG, "timeout");

            continue;
        }

        // 2.如果是检测到唤醒词
        if (WAKENET_DETECTED == result.wakenet_mode) {
            ESP_LOGI(TAG, "wakenet detected");
            
            printf("%d",result.command_id);
            continue;
        }

        // 3.如果是检测到命令词
        if (ESP_MN_STATE_DETECTED == result.state) {
            ESP_LOGI(TAG, "Command detected, ID: %d", result.command_id);
        }
    }
}
