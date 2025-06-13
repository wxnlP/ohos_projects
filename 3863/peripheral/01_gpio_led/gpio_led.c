#include <stdio.h>
#include "ohos_init.h"
#include "ohos_types.h"
#include "iot_gpio.h"
#include "cmsis_os2.h"


#define LOG_INFO(info, ...) printf("[INFO] [%s : %s] "info"\n", __FILE__, __FUNCTION__, ##__VA_ARGS__)

#define LED_TASK_PIN            2
#define LED_TASK_STACK_SIZE     0x1024
#define LED_TASK_PRIORITY       25
#define LED_TASK_NAME           "LedTask"

static void GpioLedTask(void *arg)
{
    (void) arg;
    LOG_INFO("GPIO DEMO START.");
    IoTGpioInit(LED_TASK_PIN);
    IoTGpioSetDir(LED_TASK_PIN, IOT_GPIO_DIR_OUT);
    while (1) {
        IoTGpioSetOutputVal(LED_TASK_PIN, 0);
        LOG_INFO("LED OFF");
        osDelay(50);
        IoTGpioSetOutputVal(LED_TASK_PIN, 1);
        LOG_INFO("LED ON");
        osDelay(50);
    }

}

static void GpioLedTaskEntry(void)
{
    osThreadAttr_t attr = {
        .name = LED_TASK_NAME,
        .stack_size = LED_TASK_STACK_SIZE,
        .priority = LED_TASK_PRIORITY,
    };
    osThreadId_t thread_id = osThreadNew(GpioLedTask, NULL, &attr);
    if (thread_id == NULL) {
        printf("[Thread Create] osThreadNew(%s) failed.\r\n", LED_TASK_NAME);
    } else{
        printf("[Thread Create] osThreadNew(%s) success, thread id: %d.\r\n", LED_TASK_NAME, thread_id);
    }
}

SYS_RUN(GpioLedTaskEntry);
