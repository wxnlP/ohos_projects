#include "ohos_init.h"
#include "stdio.h"
#include "cmsis_os2.h"
#include "iot_i2c.h"
#include "iot_gpio.h"
#include "iot_errno.h"
#include "hi_io.h"
#include "aht20.h"

#define AHT20_BAUDRATE 400 * 1000
#define AHT20_I2C_IDX 1

static void Aht20Task(void *arg)
{
    (void)arg;
    float temp = 0, humi = 0;
    IoTGpioInit(HI_IO_NAME_GPIO_0);
    IoTGpioInit(HI_IO_NAME_GPIO_1);

    // 设置GPIO-13引脚功能为I2C0_SDA
    hi_io_set_func(HI_IO_NAME_GPIO_0, HI_IO_FUNC_GPIO_0_I2C1_SDA);
    // 设置GPIO-14引脚功能为I2C0_SCL
    hi_io_set_func(HI_IO_NAME_GPIO_1, HI_IO_FUNC_GPIO_1_I2C1_SCL);

    // 用指定的波特速率初始化I2C0
    IoTI2cInit(AHT20_I2C_IDX, AHT20_BAUDRATE);
    uint32_t retval = AHT20Init();
    printf("[AHT20 TASK] %d\r\n", retval);
    while (1)
    {
        if (AHT20MeasureResult(&humi, &temp) == IOT_SUCCESS) {
            printf("[AHT20 TASK] 湿度 %f 温度 %f\r\n", humi, temp);
            osDelay(100);
        } else {
            printf("[AHT20 ERROR] 获取测量结果失败");
        }
    }
    
}

static void Aht20Entry(void)
{
     osThreadAttr_t attr = {
        .name = "Aht20Task",
        .stack_size = 2048,
        .priority = osPriorityNormal
    };
    if (osThreadNew(Aht20Task, NULL, &attr) == NULL) {
        printf("[ERROR] Thread Create Faild.\r\n");
    } else {
        printf("[INFO] Thread Create Success.\r\n");
    }
}

// APP_FEATURE_INIT(Aht20Entry);