#ifndef AUDIO_ECHO_H
#define AUDIO_ECHO_H

#include "esp_err.h"

/**
 * @brief 启动实时音频回声任务
 *
 * 该函数会创建一个后台任务，持续地从麦克风录音并通过扬声器播放出来。
 */
void audio_echo_task_start(void);

#endif // AUDIO_ECHO_H
