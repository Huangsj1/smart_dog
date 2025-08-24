# Smart Dog V1 - 智能聊天机器人客户端

基于ESP32-S3的智能语音聊天机器人客户端，集成了语音识别、音频播放和网络通信功能。

## 📖 项目简介

Smart Dog V1是一个基于ESP-IDF开发框架的智能语音助手客户端，运行在ESP32-S3芯片上。该项目集成了麦克风音频采集、扬声器音频播放、WiFi网络连接、WebSocket通信以及ESP-SR语音识别等功能，可以实现语音唤醒、命令识别和与服务器的实时语音交互。

## 🔧 硬件配置

### 主控芯片
- **ESP32-S3**: 双核处理器，支持WiFi和蓝牙，内置PSRAM

### 音频硬件
- **INMP441**: 数字MEMS麦克风，用于音频采集
- **MAX98357**: 数字功放芯片，驱动扬声器

### 引脚配置

#### INMP441 麦克风 (I2S1)
```
BCLK: GPIO4
WS:   GPIO5  
DIN:  GPIO7
```

#### MAX98357 功放 (I2S2)
```
BCLK: GPIO15
WS:   GPIO16
DOUT: GPIO17
```

## 🚀 主要功能

### ✅ 已实现功能
- [x] **音频驱动**: INMP441麦克风和MAX98357功放驱动
- [x] **网络连接**: WiFi连接和管理
- [x] **WebSocket通信**: 与服务器建立WebSocket连接
- [x] **语音识别**: 集成ESP-SR的唤醒词和命令词识别
- [x] **音频播放**: 本地音频播放功能

### 🔧 已知问题
- **音频传输噪声**: 扬声器播放麦克风的录音没问题，但是向服务器传输音频时在服务器播放存在较大噪声，虽然能听到人声但音质较差
- **音频处理优化**: 需要进一步优化音频采集和传输的信号处理

### 🎯 待优化功能
- [ ] 音频降噪和信号处理优化
- [ ] 和服务器进行聊天通话
- [ ] 音频编码压缩
- [ ] 网络传输稳定性改进
- [ ] 功耗优化

## 📁 项目结构

```
smart_dog_v1/
├── main/
│   ├── smart_dog_v1.c          # 主程序入口
│   ├── audio/                  # 音频相关模块
│   │   ├── inmp441_i2s.c       # INMP441麦克风驱动
│   │   ├── max98357_i2s.c      # MAX98357功放驱动
│   │   ├── audio_echo.c        # 音频回声测试
│   │   └── include/
│   │       ├── i2s_pins.h      # I2S引脚定义
│   │       ├── inmp441_i2s.h   # INMP441驱动头文件
│   │       ├── max98357_i2s.h  # MAX98357驱动头文件
│   │       ├── wav_format.h    # WAV格式头文件
│   │       └── audio_echo.h    # 音频回声测试头文件
│   ├── network/                # 网络通信模块
│   │   ├── wifi.c             # WiFi连接管理
│   │   ├── websocket_client.c # WebSocket客户端
│   │   ├── http_request.c     # HTTP请求处理
│   │   └── include/
│   └── sr/                     # 语音识别模块
│       └── include/
│           └── sr.h           # ESP-SR语音识别接口
├── managed_components/         # 管理的组件
│   ├── espressif__esp-sr/     # ESP语音识别库
│   ├── espressif__esp-dsp/    # ESP数字信号处理库
│   └── ...
├── CMakeLists.txt
├── sdkconfig                  # ESP-IDF配置文件（通过menuconfig修改，无法直接修改）
└── README.md
```

## ⚙️ 开发环境

- **操作系统**: macOS
- **ESP-IDF版本**: v6.0.0
- **开发框架**: ESP-IDF开发框架
- **芯片**: ESP32-S3 N16R8

## menuconfig 配置
* Serial flasher config: 
  * Flash size: 16MB
* ESP Speech Recognition
  * Load Multiple Wake Words: 设置需要的唤醒词
* Component config
  * ESP PSRAM：开启PSRAM
  * SPI RAM config
    * Octal Mode PSRAM：设置8线模式（4线会报错）
  * ESP System Settings:
    * Memory protection: 
      * Task Watchdog timeout period (seconds): 10（延长看门狗时间，否则等待连接时容易报错）  


## 📡 网络配置

### WiFi配置
在`main/network/wifi.c`中配置你的WiFi信息：
```c
static wifi_credentials_t wifi_config_data = {
    .ssid = "wifi用户名",
    .password = "wifi密码"
};
```

### WebSocket服务器
在`main/network/websocket_client.c`中配置服务器地址：
```c
#define WEBSOCKET_URI "ws://192.168.1.9:8000/ws"
```

## 🎵 音频配置

### 采样率设置
项目默认使用16kHz采样率，16位深度：
```c
.sample_rate = 16000,
.bits_per_sample = I2S_DATA_BIT_WIDTH_16BIT,
```

### I2S配置
音频使用双I2S配置：
- **I2S0**: 连接INMP441麦克风 (录音)
- **I2S1**: 连接MAX98357功放 (播放)