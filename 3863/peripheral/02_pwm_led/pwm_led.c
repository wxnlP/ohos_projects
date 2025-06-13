#include <stdio.h>
#include "ohos_init.h"
#include "ohos_types.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_pwm.h"
#include "iot_gpio_ex.h"
// 不包含编译也不会报错，建议包含，便于阅读
#include "pwm.h"

#define LOG_INFO(info, ...) printf("[INFO] [%s : %s] "info"\n", __FILE__, __FUNCTION__, ##__VA_ARGS__)

#define PWM_LED_TASK_PIN            2
#define PWM_LED_TASK_PORT           2
#define PWM_LED_TASK_STACK_SIZE     0x1024
#define PWM_LED_TASK_PRIORITY       25
#define PWM_LED_TASK_NAME           "PwmLedTask"

static void PwmLedTask(void *arg)
{
    (void) arg;
    // 初始化GPIO
    IoTGpioInit(PWM_LED_TASK_PIN);

    // 设置引脚的工作模式
    IoTSetFunc(PWM_LED_TASK_PIN, IOT_IO_FUNC_GPIO_2_PWM2_OUT);

    // 初始化PWM
    IoTPwmInit(PWM_LED_TASK_PIN);
    int i;
    while (1) {
        /* 配置PWM参数，占空比递增后递减、频率4000HZ */
        for (i=0 ; i<100 ; i++) {
            IoTPwmStart(PWM_LED_TASK_PIN, i, 4000);
            osDelay(1);
            uapi_pwm_clear_group(PWM_LED_TASK_PORT);
        }
        for (i=0 ; i<100 ; i++) {
            IoTPwmStart(PWM_LED_TASK_PIN, 100-i, 4000);
            osDelay(1);
            uapi_pwm_clear_group(PWM_LED_TASK_PORT);
        }
    }
}

static void PwmLedTaskEntry(void)
{
    osThreadAttr_t attr = {
        .name = PWM_LED_TASK_NAME,
        .stack_size = PWM_LED_TASK_STACK_SIZE,
        .priority = PWM_LED_TASK_PRIORITY,
    };
    osThreadId_t thread_id = osThreadNew(PwmLedTask, NULL, &attr);
    if (thread_id == NULL) {
        printf("[Thread Create] osThreadNew(%s) failed.\r\n", PWM_LED_TASK_NAME);
    } else{
        printf("[Thread Create] osThreadNew(%s) success, thread id: %d.\r\n", PWM_LED_TASK_NAME, thread_id);
    }
}

SYS_RUN(PwmLedTaskEntry);