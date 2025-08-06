#ifndef I2S_PINS_H
#define I2S_PINS_H

// 第一个I2S设备的引脚定义
#define EXAMPLE_I2S_BCLK_IO1        GPIO_NUM_4      // I2S bit clock io number
#define EXAMPLE_I2S_WS_IO1          GPIO_NUM_5      // I2S word select io number
#define EXAMPLE_I2S_DOUT_IO1        GPIO_NUM_6      // I2S data out io number
#define EXAMPLE_I2S_DIN_IO1         GPIO_NUM_7      // I2S data in io number
// PDM RX 4 line IO（如果使用PDM数据引脚，下面第二个I2S就不能使用，否则会冲突）
#define EXAMPLE_I2S_DIN1_IO1        GPIO_NUM_15     // I2S data in io number
#define EXAMPLE_I2S_DIN2_IO1        GPIO_NUM_16     // I2S data in io number
#define EXAMPLE_I2S_DIN3_IO1        GPIO_NUM_17     // I2S data in io number

// 第二个I2S设备的引脚定义（如果需要使用第二个I2S设备）
#define EXAMPLE_I2S_BCLK_IO2        GPIO_NUM_15      // I2S bit clock io number
#define EXAMPLE_I2S_WS_IO2          GPIO_NUM_16      // I2S word select io number
#define EXAMPLE_I2S_DOUT_IO2        GPIO_NUM_17      // I2S data out io number
#define EXAMPLE_I2S_DIN_IO2         GPIO_NUM_18      // I2S data in io number

#endif // I2S_PINS_H